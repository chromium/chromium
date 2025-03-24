// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_shared_memory.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/common/chrome_features.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/histogram_controller.h"
#include "components/metrics/public/mojom/histogram_fetcher.mojom.h"
#include "components/remote_cocoa/browser/application_host.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

AppShimHost::AppShimHost(AppShimHost::Client* client,
                         const std::string& app_id,
                         const base::FilePath& profile_path,
                         bool uses_remote_views)
    : client_(client),
      app_shim_receiver_(app_shim_.BindNewPipeAndPassReceiver()),
      app_id_(app_id),
      profile_path_(profile_path),
      uses_remote_views_(uses_remote_views),
      child_process_host_id_(
          content::ChildProcessHost::GenerateChildProcessUniqueId()),
      launch_weak_factory_(this) {
  // Create the interfaces used to host windows, so that browser windows may be
  // created before the host process finishes launching.
  if (uses_remote_views_ &&
      base::FeatureList::IsEnabled(features::kAppShimRemoteCocoa)) {
    // Create the interface that will be used by views::NativeWidgetMac to
    // create NSWindows hosted in the app shim process.
    mojo::PendingAssociatedReceiver<remote_cocoa::mojom::Application>
        views_application_receiver;
    remote_cocoa_application_host_ =
        std::make_unique<remote_cocoa::ApplicationHost>(
            &views_application_receiver,
            web_app::GetBundleIdentifierForShim(app_id, profile_path));
    app_shim_->CreateRemoteCocoaApplication(
        std::move(views_application_receiver));
  }

  auto shared_memory = base::HistogramSharedMemory::Create(
      child_process_host_id_,
      {content::PROCESS_TYPE_UTILITY, "AppShimMetrics", 512 << 10});
  if (shared_memory) {
    histogram_allocator_ = std::move(shared_memory->allocator);
  }
  metrics::HistogramController::GetInstance()->SetHistogramMemory(
      this,
      shared_memory ? std::move(shared_memory->region)
                    : base::UnsafeSharedMemoryRegion(),
      metrics::HistogramController::ChildProcessMode::kGetHistogramData);
}

AppShimHost::~AppShimHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  metrics::HistogramController::GetInstance()->NotifyChildDied(this);
  // If this instance gets destructed while a test is still waiting for it to be
  // connected, we should unblock the test. The shim would have never connected,
  // but unblocking the test at least can cause the test to fail gracefully
  // rather than timeout waiting for something that will never happen.
  if (on_shim_connected_for_testing_) {
    std::move(on_shim_connected_for_testing_).Run();
  }
}

void AppShimHost::ChannelError(uint32_t custom_reason,
                               const std::string& description) {
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;

  if (auto* provider = metrics::SubprocessMetricsProvider::GetInstance()) {
    provider->DeregisterSubprocessAllocator(child_process_host_id_);
  } else {
    // SubprocessMetricsProvider can be null in tests.
    CHECK_IS_TEST();
  }

  // OnShimProcessDisconnected will delete |this|.
  client_->OnShimProcessDisconnected(this);
}

void AppShimHost::LaunchShimInternal(
    web_app::LaunchShimUpdateBehavior update_behavior,
    web_app::ShimLaunchMode launch_mode) {
  DCHECK(launch_shim_has_been_called_);
  DCHECK(!bootstrap_);
  launch_weak_factory_.InvalidateWeakPtrs();
  client_->OnShimLaunchRequested(
      this, update_behavior, launch_mode,
      base::BindOnce(&AppShimHost::OnShimProcessLaunched,
                     launch_weak_factory_.GetWeakPtr(), update_behavior,
                     launch_mode),
      base::BindOnce(&AppShimHost::OnShimProcessTerminated,
                     launch_weak_factory_.GetWeakPtr(), update_behavior,
                     launch_mode));
}

void AppShimHost::OnShimProcessLaunched(
    web_app::LaunchShimUpdateBehavior update_behavior,
    web_app::ShimLaunchMode launch_mode,
    base::Process shim_process) {
  // If a bootstrap connected, then it should have invalidated all weak
  // pointers, preventing this from being called.
  DCHECK(!bootstrap_);

  // If the shim process was created, then await either an AppShimHostBootstrap
  // connecting or the process exiting.
  if (shim_process.IsValid()) {
    return;
  }

  // Shim launch failing is treated the same as the shim launching but
  // terminating before connecting.
  OnShimProcessTerminated(update_behavior, launch_mode);
}

void AppShimHost::OnShimProcessTerminated(
    web_app::LaunchShimUpdateBehavior update_behavior,
    web_app::ShimLaunchMode launch_mode) {
  DCHECK(!bootstrap_);

  if (auto* provider = metrics::SubprocessMetricsProvider::GetInstance()) {
    provider->DeregisterSubprocessAllocator(child_process_host_id_);
  } else {
    // SubprocessMetricsProvider can be null in tests.
    CHECK_IS_TEST();
  }

  // If this was a launch without recreating shims, then the launch may have
  // failed because the shims were not present, or because they were out of
  // date. Try again, recreating the shims this time.
  if (!web_app::RecreateShimsRequested(update_behavior)) {
    DLOG(ERROR) << "Failed to launch shim, attempting to recreate.";
    LaunchShimInternal(
        web_app::LaunchShimUpdateBehavior::kRecreateUnconditionally,
        launch_mode);
    return;
  }

  // If we attempted to recreate the app shims and still failed to launch, then
  // there is no hope to launch the app. Close its windows (since they will
  // never be seen).
  // TODO(crbug.com/40605763): Consider adding some UI to tell the
  // user that the process launch failed.
  DLOG(ERROR) << "Failed to launch recreated shim, giving up.";

  // OnShimProcessDisconnected will delete |this|.
  client_->OnShimProcessDisconnected(this);
}

////////////////////////////////////////////////////////////////////////////////
// AppShimHost, chrome::mojom::AppShimHost

void AppShimHost::SetOnShimConnectedForTesting(base::OnceClosure closure) {
  on_shim_connected_for_testing_ = std::move(closure);
}

base::ProcessId AppShimHost::GetAppShimPid() const {
  if (bootstrap_) {
    return bootstrap_->GetAppShimPid();
  }
  return base::kNullProcessId;
}

bool AppShimHost::HasBootstrapConnected() const {
  return bootstrap_ != nullptr;
}

void AppShimHost::OnBootstrapConnected(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  // Prevent any callbacks from any pending launches (e.g, if an internal and
  // external launch happen to race).
  launch_weak_factory_.InvalidateWeakPtrs();

  DCHECK(!bootstrap_);
  bootstrap_ = std::move(bootstrap);
  bootstrap_->OnConnectedToHost(std::move(app_shim_receiver_));

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  host_receiver_.Bind(bootstrap_->GetAppShimHostReceiver());
  host_receiver_.set_disconnect_with_reason_handler(
      base::BindOnce(&AppShimHost::ChannelError, base::Unretained(this)));

  auto* provider = metrics::SubprocessMetricsProvider::GetInstance();
  if (!provider) {
    CHECK_IS_TEST();
  } else if (histogram_allocator_) {
    provider->RegisterSubprocessAllocator(
        child_process_host_id_,
        std::make_unique<base::PersistentHistogramAllocator>(
            std::move(histogram_allocator_)));
  }

  if (on_shim_connected_for_testing_)
    std::move(on_shim_connected_for_testing_).Run();
}

void AppShimHost::LaunchShim(web_app::ShimLaunchMode launch_mode) {
  if (launch_shim_has_been_called_) {
    return;
  }
  launch_shim_has_been_called_ = true;

  if (bootstrap_) {
    // If there is a connected app shim process, and this is not a background
    // launch, focus the app windows.
    if (launch_mode != web_app::ShimLaunchMode::kBackground) {
      client_->OnShimFocus(this);
    }
  } else {
    // Otherwise, attempt to launch whatever app shims we find.
    LaunchShimInternal(web_app::LaunchShimUpdateBehavior::kDoNotRecreate,
                       launch_mode);
  }
}

void AppShimHost::FocusApp() {
  client_->OnShimFocus(this);
}

void AppShimHost::ReopenApp() {
  client_->OnShimReopen(this);
}

void AppShimHost::FilesOpened(const std::vector<base::FilePath>& files) {
  client_->OnShimOpenedFiles(this, files);
}

void AppShimHost::ProfileSelectedFromMenu(const base::FilePath& profile_path) {
  client_->OnShimSelectedProfile(this, profile_path);
}

void AppShimHost::OpenAppSettings() {
  client_->OnShimOpenedAppSettings(this);
}

void AppShimHost::UrlsOpened(const std::vector<GURL>& urls) {
  client_->OnShimOpenedUrls(this, urls);
}

void AppShimHost::OpenAppWithOverrideUrl(const GURL& override_url) {
  client_->OnShimOpenAppWithOverrideUrl(this, override_url);
}

void AppShimHost::EnableAccessibilitySupport(
    chrome::mojom::AppShimScreenReaderSupportMode mode) {
  content::BrowserAccessibilityState* accessibility_state =
      content::BrowserAccessibilityState::GetInstance();
  switch (mode) {
    case chrome::mojom::AppShimScreenReaderSupportMode::kComplete: {
      accessibility_state->OnScreenReaderDetected();
      break;
    }
    case chrome::mojom::AppShimScreenReaderSupportMode::kPartial: {
      if (!accessibility_state->GetAccessibilityMode().has_mode(
              ui::kAXModeBasic.flags())) {
        accessibility_state->AddAccessibilityModeFlags(ui::kAXModeBasic);
      }
      break;
    }
  }
}

void AppShimHost::ApplicationWillTerminate() {
  client_->OnShimWillTerminate(this);
}

void AppShimHost::NotificationPermissionStatusChanged(
    mac_notifications::mojom::PermissionStatus status) {
  client_->OnNotificationPermissionStatusChanged(this, status);
}

base::FilePath AppShimHost::GetProfilePath() const {
  // This should only be used by single-profile-app paths.
  DCHECK(!profile_path_.empty());
  return profile_path_;
}

std::string AppShimHost::GetAppId() const {
  return app_id_;
}

remote_cocoa::ApplicationHost* AppShimHost::GetRemoteCocoaApplicationHost()
    const {
  return remote_cocoa_application_host_.get();
}

chrome::mojom::AppShim* AppShimHost::GetAppShim() const {
  return app_shim_.get();
}

void AppShimHost::BindChildHistogramFetcherFactory(
    mojo::PendingReceiver<metrics::mojom::ChildHistogramFetcherFactory>
        factory) {
  app_shim_->BindChildHistogramFetcherFactory(std::move(factory));
}
