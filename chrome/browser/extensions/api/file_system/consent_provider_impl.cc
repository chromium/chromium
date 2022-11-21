// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/consent_provider_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/file_system/request_file_system_notification.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/views/extensions/request_file_system_dialog_view.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/kiosk/kiosk_delegate.h"
#include "extensions/common/api/file_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/file_manager/app_id.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {

// List of allowlisted component apps and extensions by their ids for
// chrome.fileSystem.requestFileSystem.
const char* const kRequestFileSystemComponentAllowlist[] = {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    file_manager::kFileManagerAppId, file_manager::kImageLoaderExtensionId,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    // TODO(henryhsu,b/110126438): Remove this extension id, and add it only
    // for tests.
    "pkplfbidichfdicaijlchgnapepdginl"  // Testing extensions.
};

ui::DialogButton g_auto_dialog_button_for_test = ui::DIALOG_BUTTON_NONE;

// Gets a WebContents instance handle for a current window of a platform app
// with |app_id|. If not found, then returns NULL.
content::WebContents* GetWebContentsForAppId(Profile* profile,
                                             const std::string& app_id) {
  AppWindowRegistry* const registry = AppWindowRegistry::Get(profile);
  DCHECK(registry);
  AppWindow* const app_window = registry->GetCurrentAppWindowForApp(app_id);
  return app_window ? app_window->web_contents() : nullptr;
}

// Converts the clicked button to a consent result and passes it via the
// |callback|.
void DialogResultToConsent(
    file_system_api::ConsentProviderImpl::ConsentCallback callback,
    ui::DialogButton button) {
  switch (button) {
    case ui::DIALOG_BUTTON_NONE:
      std::move(callback).Run(ConsentProvider::CONSENT_IMPOSSIBLE);
      break;
    case ui::DIALOG_BUTTON_OK:
      std::move(callback).Run(ConsentProvider::CONSENT_GRANTED);
      break;
    // The following is wired to both Cancel and Close callbacks.
    case ui::DIALOG_BUTTON_CANCEL:
      std::move(callback).Run(ConsentProvider::CONSENT_REJECTED);
      break;
  }
}

}  // namespace

namespace file_system_api {

/******** ConsentProviderImpl::DelegateInterface ********/

ConsentProviderImpl::DelegateInterface::DelegateInterface() = default;
ConsentProviderImpl::DelegateInterface::~DelegateInterface() = default;

/******** ConsentProviderImpl ********/

ConsentProviderImpl::ConsentProviderImpl(
    std::unique_ptr<DelegateInterface> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

ConsentProviderImpl::~ConsentProviderImpl() = default;

void ConsentProviderImpl::RequestConsent(content::RenderFrameHost* host,
                                         const Extension& extension,
                                         const std::string& volume_id,
                                         const std::string& volume_label,
                                         bool writable,
                                         ConsentCallback callback) {
  DCHECK(IsGrantable(extension));

  // If an allowlisted component, then no need to ask or inform the user.
  if (extension.location() == mojom::ManifestLocation::kComponent &&
      delegate_->IsAllowlistedComponent(extension)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), CONSENT_GRANTED));
    return;
  }

  // If auto-launched kiosk app, then no need to ask user either, but show the
  // notification.
  if (delegate_->IsAutoLaunched(extension)) {
    delegate_->ShowNotification(extension.id(), extension.name(), volume_id,
                                volume_label, writable);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), CONSENT_GRANTED));
    return;
  }

  // If it's a kiosk app running in manual-launch kiosk session, then show the
  // confirmation dialog.
  if (KioskModeInfo::IsKioskOnly(&extension) &&
      profiles::IsChromeAppKioskSession()) {
    delegate_->ShowDialog(
        host, extension.id(), extension.name(), volume_id, volume_label,
        writable, base::BindOnce(&DialogResultToConsent, std::move(callback)));
    return;
  }

  NOTREACHED() << "Cannot request consent for non-grantable extensions.";
}

bool ConsentProviderImpl::IsGrantable(const Extension& extension) {
  // Only kiosk apps in kiosk sessions can use file system API.
  // Additionally it is enabled for allowlisted component extensions and apps.
  const bool is_allowlisted_component =
      delegate_->IsAllowlistedComponent(extension);

  const bool is_running_in_kiosk_session =
      KioskModeInfo::IsKioskOnly(&extension) &&
      profiles::IsChromeAppKioskSession();

  return is_allowlisted_component || is_running_in_kiosk_session;
}

/******** ConsentProviderDelegate ********/

ConsentProviderDelegate::ConsentProviderDelegate(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  profile_observation_.Observe(profile_);
}

ConsentProviderDelegate::~ConsentProviderDelegate() = default;

// static
void ConsentProviderDelegate::SetAutoDialogButtonForTest(
    ui::DialogButton button) {
  g_auto_dialog_button_for_test = button;
}

void ConsentProviderDelegate::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile_, profile);
  profile_observation_.Reset();
  profile_ = nullptr;
}

void ConsentProviderDelegate::ShowDialog(
    content::RenderFrameHost* host,
    const extensions::ExtensionId& extension_id,
    const std::string& extension_name,
    const std::string& volume_id,
    const std::string& volume_label,
    bool writable,
    file_system_api::ConsentProviderImpl::ShowDialogCallback callback) {
  DCHECK(host);
  // Reject if |profile_| is gone.
  if (!profile_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), ui::DIALOG_BUTTON_NONE));
    return;
  }

  content::WebContents* web_contents = nullptr;

  // Find an app window to host the dialog.
  content::WebContents* const foreground_contents =
      content::WebContents::FromRenderFrameHost(host);
  if (AppWindowRegistry::Get(profile_)->GetAppWindowForWebContents(
          foreground_contents)) {
    web_contents = foreground_contents;
  }

  // If there is no web contents handle, then the method is most probably
  // executed from a background page.
  if (!web_contents)
    web_contents = GetWebContentsForAppId(profile_, extension_id);

  if (!web_contents) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), ui::DIALOG_BUTTON_NONE));
    return;
  }

  // Short circuit the user consent dialog for tests. This is far from a pretty
  // code design.
  if (g_auto_dialog_button_for_test != ui::DIALOG_BUTTON_NONE) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  /*result=*/g_auto_dialog_button_for_test));
    return;
  }

  RequestFileSystemDialogView::ShowDialog(
      web_contents, extension_name,
      volume_label.empty() ? volume_id : volume_label, writable,
      std::move(callback));
}

void ConsentProviderDelegate::ShowNotification(
    const extensions::ExtensionId& extension_id,
    const std::string& extension_name,
    const std::string& volume_id,
    const std::string& volume_label,
    bool writable) {
  // Skip if |profile_| is gone.
  if (!profile_)
    return;

  ShowNotificationForAutoGrantedRequestFileSystem(profile_, extension_id,
                                                  extension_name, volume_id,
                                                  volume_label, writable);
}

bool ConsentProviderDelegate::IsAutoLaunched(const Extension& extension) {
  return ExtensionsBrowserClient::Get()
      ->GetKioskDelegate()
      ->IsAutoLaunchedKioskApp(extension.id());
}

bool ConsentProviderDelegate::IsAllowlistedComponent(
    const Extension& extension) {
  for (auto* allowlisted_id : kRequestFileSystemComponentAllowlist) {
    if (extension.id().compare(allowlisted_id) == 0)
      return true;
  }
  return false;
}

}  // namespace file_system_api
}  // namespace extensions
