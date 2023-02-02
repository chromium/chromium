// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/common/chrome_features.h"
#include "components/remote_cocoa/browser/application_host.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "content/public/browser/browser_thread.h"
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
            &views_application_receiver);
    app_shim_->CreateRemoteCocoaApplication(
        std::move(views_application_receiver));
  }
}

AppShimHost::~AppShimHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

  // OnShimProcessDisconnected will delete |this|.
  client_->OnShimProcessDisconnected(this);
}

void AppShimHost::LaunchShimInternal(bool recreate_shims) {
  DCHECK(launch_shim_has_been_called_);
  DCHECK(!bootstrap_);
  launch_weak_factory_.InvalidateWeakPtrs();
  client_->OnShimLaunchRequested(
      this, recreate_shims,
      base::BindOnce(&AppShimHost::OnShimProcessLaunched,
                     launch_weak_factory_.GetWeakPtr(), recreate_shims),
      base::BindOnce(&AppShimHost::OnShimProcessTerminated,
                     launch_weak_factory_.GetWeakPtr(), recreate_shims));
}

void AppShimHost::OnShimProcessLaunched(bool recreate_shims_requested,
                                        base::Process shim_process) {
  // If a bootstrap connected, then it should have invalidated all weak
  // pointers, preventing this from being called.
  DCHECK(!bootstrap_);

  // If the shim process was created, then await either an AppShimHostBootstrap
  // connecting or the process exiting.
  if (shim_process.IsValid())
    return;

  // Shim launch failing is treated the same as the shim launching but
  // terminating before connecting.
  OnShimProcessTerminated(recreate_shims_requested);
}

void AppShimHost::OnShimProcessTerminated(bool recreate_shims_requested) {
  DCHECK(!bootstrap_);

  // If this was a launch without recreating shims, then the launch may have
  // failed because the shims were not present, or because they were out of
  // date. Try again, recreating the shims this time.
  if (!recreate_shims_requested) {
    DLOG(ERROR) << "Failed to launch shim, attempting to recreate.";
    LaunchShimInternal(true /* recreate_shims */);
    return;
  }

  // If we attempted to recreate the app shims and still failed to launch, then
  // there is no hope to launch the app. Close its windows (since they will
  // never be seen).
  // TODO(https://crbug.com/913362): Consider adding some UI to tell the
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

  if (on_shim_connected_for_testing_)
    std::move(on_shim_connected_for_testing_).Run();
}

void AppShimHost::LaunchShim() {
  if (launch_shim_has_been_called_)
    return;
  launch_shim_has_been_called_ = true;

  if (bootstrap_) {
    // If there is a connected app shim process, focus the app windows.
    client_->OnShimFocus(this);
  } else {
    // Otherwise, attempt to launch whatever app shims we find.
    LaunchShimInternal(false /* recreate_shims */);
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

void AppShimHost::UrlsOpened(const std::vector<GURL>& urls) {
  client_->OnShimOpenedUrls(this, urls);
}

void AppShimHost::OpenAppWithOverrideUrl(const GURL& override_url) {
  client_->OnShimOpenAppWithOverrideUrl(this, override_url);
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
