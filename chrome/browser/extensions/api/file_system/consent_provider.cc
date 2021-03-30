// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/consent_provider.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/extensions/api/file_system/request_file_system_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/extensions/request_file_system_dialog_view.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/api/file_system.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace {

// List of allowlisted component apps and extensions by their ids for
// chrome.fileSystem.requestFileSystem.
const char* const kRequestFileSystemComponentAllowlist[] = {
    file_manager::kFileManagerAppId, file_manager::kVideoPlayerAppId,
    file_manager::kGalleryAppId, file_manager::kAudioPlayerAppId,
    file_manager::kImageLoaderExtensionId, file_manager::kZipArchiverId,
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
    file_system_api::ConsentProvider::ConsentCallback callback,
    ui::DialogButton button) {
  switch (button) {
    case ui::DIALOG_BUTTON_NONE:
      std::move(callback).Run(
          file_system_api::ConsentProvider::CONSENT_IMPOSSIBLE);
      break;
    case ui::DIALOG_BUTTON_OK:
      std::move(callback).Run(
          file_system_api::ConsentProvider::CONSENT_GRANTED);
      break;
    case ui::DIALOG_BUTTON_CANCEL:
      std::move(callback).Run(
          file_system_api::ConsentProvider::CONSENT_REJECTED);
      break;
  }
}

}  // namespace

namespace file_system_api {

ConsentProvider::ConsentProvider(DelegateInterface* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

ConsentProvider::~ConsentProvider() {
}

void ConsentProvider::RequestConsent(
    const Extension& extension,
    content::RenderFrameHost* host,
    const base::WeakPtr<file_manager::Volume>& volume,
    bool writable,
    ConsentCallback callback) {
  DCHECK(IsGrantableForVolume(extension, volume));

  // If a allowlisted component, then no need to ask or inform the user.
  if (extension.location() == mojom::ManifestLocation::kComponent &&
      delegate_->IsAllowlistedComponent(extension)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), CONSENT_GRANTED));
    return;
  }

  // If a allowlisted app or extensions to access Downloads folder, then no
  // need to ask or inform the user.
  if (volume.get() &&
      volume->type() == file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY &&
      delegate_->HasRequestDownloadsPermission(extension)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), CONSENT_GRANTED));
    return;
  }

  // If auto-launched kiosk app, then no need to ask user either, but show the
  // notification.
  if (delegate_->IsAutoLaunched(extension)) {
    delegate_->ShowNotification(extension, volume, writable);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), CONSENT_GRANTED));
    return;
  }

  // If it's a kiosk app running in manual-launch kiosk session, then show
  // the confirmation dialog.
  if (KioskModeInfo::IsKioskOnly(&extension) &&
      user_manager::UserManager::Get()->IsLoggedInAsKioskApp()) {
    delegate_->ShowDialog(
        extension, host, volume, writable,
        base::BindOnce(&DialogResultToConsent, std::move(callback)));
    return;
  }

  NOTREACHED() << "Cannot request consent for non-grantable extensions.";
}

FileSystemDelegate::GrantVolumesMode ConsentProvider::GetGrantVolumesMode(
    const Extension& extension) {
  const bool is_allowlisted_component =
      delegate_->IsAllowlistedComponent(extension);

  const bool is_running_in_kiosk_session =
      KioskModeInfo::IsKioskOnly(&extension) &&
      user_manager::UserManager::Get()->IsLoggedInAsKioskApp();

  if (is_allowlisted_component || is_running_in_kiosk_session) {
    return FileSystemDelegate::kGrantAll;
  }

  const bool is_allowlisted_non_component =
      delegate_->HasRequestDownloadsPermission(extension);

  return is_allowlisted_non_component ? FileSystemDelegate::kGrantPerVolume
                                      : FileSystemDelegate::kGrantNone;
}

bool ConsentProvider::IsGrantableForVolume(
    const Extension& extension,
    const base::WeakPtr<file_manager::Volume>& volume) {
  if (volume.get() &&
      volume->type() == file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY &&
      delegate_->HasRequestDownloadsPermission(extension)) {
    return true;
  }

  return GetGrantVolumesMode(extension) == FileSystemDelegate::kGrantAll;
}

ConsentProviderDelegate::ConsentProviderDelegate(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

ConsentProviderDelegate::~ConsentProviderDelegate() {
}

// static
void ConsentProviderDelegate::SetAutoDialogButtonForTest(
    ui::DialogButton button) {
  g_auto_dialog_button_for_test = button;
}

void ConsentProviderDelegate::ShowDialog(
    const Extension& extension,
    content::RenderFrameHost* host,
    const base::WeakPtr<file_manager::Volume>& volume,
    bool writable,
    file_system_api::ConsentProvider::ShowDialogCallback callback) {
  DCHECK(host);
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
    web_contents = GetWebContentsForAppId(profile_, extension.id());

  if (!web_contents) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), ui::DIALOG_BUTTON_NONE));
    return;
  }

  // Short circuit the user consent dialog for tests. This is far from a pretty
  // code design.
  if (g_auto_dialog_button_for_test != ui::DIALOG_BUTTON_NONE) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  g_auto_dialog_button_for_test /* result */));
    return;
  }

  // If the volume is gone, then cancel the dialog.
  if (!volume.get()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), ui::DIALOG_BUTTON_CANCEL));
    return;
  }

  RequestFileSystemDialogView::ShowDialog(
      web_contents, extension.name(),
      (volume->volume_label().empty() ? volume->volume_id()
                                      : volume->volume_label()),
      writable, std::move(callback));
}

void ConsentProviderDelegate::ShowNotification(
    const Extension& extension,
    const base::WeakPtr<file_manager::Volume>& volume,
    bool writable) {
  ShowNotificationForAutoGrantedRequestFileSystem(profile_, extension, volume,
                                                  writable);
}

bool ConsentProviderDelegate::IsAutoLaunched(const Extension& extension) {
  ash::KioskAppManager::App app_info;
  return ash::KioskAppManager::Get()->GetApp(extension.id(), &app_info) &&
         app_info.was_auto_launched_with_zero_delay;
}

bool ConsentProviderDelegate::IsAllowlistedComponent(
    const Extension& extension) {
  for (auto* allowlisted_id : kRequestFileSystemComponentAllowlist) {
    if (extension.id().compare(allowlisted_id) == 0)
      return true;
  }
  return false;
}

bool ConsentProviderDelegate::HasRequestDownloadsPermission(
    const Extension& extension) {
  return extension.permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kFileSystemRequestDownloads);
}

}  // namespace file_system_api
}  // namespace extensions
