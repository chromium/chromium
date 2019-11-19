// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate.h"

#include <utility>
#include <vector>

#include "apps/saved_files_service.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/api/file_system/file_entry_picker.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/directory_access_confirmation_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/api/file_system/saved_files_service_interface.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/file_system.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/foundation_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/extensions/api/file_system/consent_provider.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "url/url_constants.h"
#endif

namespace extensions {

namespace file_system = api::file_system;

#if defined(OS_CHROMEOS)
using file_system_api::ConsentProvider;
using file_system_api::ConsentProviderDelegate;

namespace {

const char kConsentImpossible[] =
    "Impossible to ask for user consent as there is no app window visible.";
const char kNotSupportedOnNonKioskSessionError[] =
    "Operation only supported for kiosk apps running in a kiosk session.";
const char kRequiresFileSystemWriteError[] =
    "Operation requires fileSystem.write permission";
const char kSecurityError[] = "Security error.";
const char kVolumeNotFoundError[] = "Volume not found.";

// Fills a list of volumes mounted in the system.
bool GetVolumeListForExtension(
    const std::vector<base::WeakPtr<file_manager::Volume>>& available_volumes,
    ConsentProvider* consent_provider,
    const Extension& extension,
    std::vector<file_system::Volume>* result_volumes) {
  if (!consent_provider)
    return false;

  const FileSystemDelegate::GrantVolumesMode mode =
      consent_provider->GetGrantVolumesMode(extension);
  if (mode == FileSystemDelegate::kGrantNone)
    return false;

  // Convert available_volumes to result_volume_list.
  for (const auto& volume : available_volumes) {
    if (mode == FileSystemDelegate::kGrantAll ||
        (mode == FileSystemDelegate::kGrantPerVolume &&
         consent_provider->IsGrantableForVolume(extension, volume))) {
      file_system::Volume result_volume;
      result_volume.volume_id = volume->volume_id();
      result_volume.writable = !volume->is_read_only();
      result_volumes->push_back(std::move(result_volume));
    }
  }
  return true;
}

// Callback called when consent is granted or denied.
void OnConsentReceived(
    content::BrowserContext* browser_context,
    scoped_refptr<ExtensionFunction> requester,
    const FileSystemDelegate::FileSystemCallback& success_callback,
    const FileSystemDelegate::ErrorCallback& error_callback,
    const std::string& extension_id,
    const base::WeakPtr<file_manager::Volume>& volume,
    bool writable,
    ConsentProvider::Consent result) {
  using file_manager::VolumeManager;
  using file_manager::Volume;

  // Render frame host can be gone before this callback method is executed.
  if (!requester->render_frame_host()) {
    error_callback.Run(std::string());
    return;
  }

  switch (result) {
    case ConsentProvider::CONSENT_REJECTED:
      error_callback.Run(kSecurityError);
      return;

    case ConsentProvider::CONSENT_IMPOSSIBLE:
      error_callback.Run(kConsentImpossible);
      return;

    case ConsentProvider::CONSENT_GRANTED:
      break;
  }

  if (!volume.get()) {
    error_callback.Run(kVolumeNotFoundError);
    return;
  }

  const GURL site = util::GetSiteForExtensionId(extension_id, browser_context);
  scoped_refptr<storage::FileSystemContext> file_system_context =
      content::BrowserContext::GetStoragePartitionForSite(browser_context, site)
          ->GetFileSystemContext();
  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
  DCHECK(backend);

  base::FilePath virtual_path;
  if (!backend->GetVirtualPath(volume->mount_path(), &virtual_path)) {
    error_callback.Run(kSecurityError);
    return;
  }

  storage::IsolatedContext* const isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  const storage::FileSystemURL original_url =
      file_system_context->CreateCrackedFileSystemURL(
          GURL(std::string(kExtensionScheme) + url::kStandardSchemeSeparator +
               extension_id),
          storage::kFileSystemTypeExternal, virtual_path);

  // Set a fixed register name, as the automatic one would leak the mount point
  // directory.
  std::string register_name = "fs";
  const storage::IsolatedContext::ScopedFSHandle file_system =
      isolated_context->RegisterFileSystemForPath(
          storage::kFileSystemTypeNativeForPlatformApp,
          std::string() /* file_system_id */, original_url.path(),
          &register_name);
  if (!file_system.is_valid()) {
    error_callback.Run(kSecurityError);
    return;
  }

  backend->GrantFileAccessToExtension(extension_id, virtual_path);

  // Grant file permissions to the renderer hosting component.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  DCHECK(policy);

  const auto process_id = requester->source_process_id();
  // Read-only permisisons.
  policy->GrantReadFile(process_id, volume->mount_path());
  policy->GrantReadFileSystem(process_id, file_system.id());

  // Additional write permissions.
  if (writable) {
    policy->GrantCreateReadWriteFile(process_id, volume->mount_path());
    policy->GrantCopyInto(process_id, volume->mount_path());
    policy->GrantWriteFileSystem(process_id, file_system.id());
    policy->GrantDeleteFromFileSystem(process_id, file_system.id());
    policy->GrantCreateFileForFileSystem(process_id, file_system.id());
  }

  success_callback.Run(file_system.id(), register_name);
}

}  // namespace

namespace file_system_api {

void DispatchVolumeListChangeEvent(content::BrowserContext* browser_context) {
  DCHECK(browser_context);
  EventRouter* const event_router = EventRouter::Get(browser_context);
  if (!event_router)  // Possible on shutdown.
    return;

  ExtensionRegistry* const registry = ExtensionRegistry::Get(browser_context);
  if (!registry)  // Possible on shutdown.
    return;

  ConsentProviderDelegate consent_provider_delegate(
      Profile::FromBrowserContext(browser_context));
  ConsentProvider consent_provider(&consent_provider_delegate);

  const std::vector<base::WeakPtr<file_manager::Volume>> volume_list =
      file_manager::VolumeManager::Get(browser_context)->GetVolumeList();

  for (const auto& extension : registry->enabled_extensions()) {
    file_system::VolumeListChangedEvent event_args;
    if (!GetVolumeListForExtension(volume_list, &consent_provider,
                                   *extension.get(), &event_args.volumes)) {
      continue;
    }

    event_router->DispatchEventToExtension(
        extension->id(),
        std::make_unique<Event>(
            events::FILE_SYSTEM_ON_VOLUME_LIST_CHANGED,
            file_system::OnVolumeListChanged::kEventName,
            file_system::OnVolumeListChanged::Create(event_args)));
  }
}

}  // namespace file_system_api
#endif  // defined(OS_CHROMEOS)

ChromeFileSystemDelegate::ChromeFileSystemDelegate() {}

ChromeFileSystemDelegate::~ChromeFileSystemDelegate() {}

base::FilePath ChromeFileSystemDelegate::GetDefaultDirectory() {
  base::FilePath documents_dir;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &documents_dir);
  return documents_dir;
}

bool ChromeFileSystemDelegate::ShowSelectFileDialog(
    scoped_refptr<ExtensionFunction> extension_function,
    ui::SelectFileDialog::Type type,
    const base::FilePath& default_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    FileSystemDelegate::FilesSelectedCallback files_selected_callback,
    base::OnceClosure file_selection_canceled_callback) {
  const Extension* extension = extension_function->extension();
  content::WebContents* web_contents =
      extension_function->GetSenderWebContents();

  if (!web_contents)
    return false;

  // TODO(asargent/benwells) - As a short term remediation for
  // crbug.com/179010 we're adding the ability for a whitelisted extension to
  // use this API since chrome.fileBrowserHandler.selectFile is ChromeOS-only.
  // Eventually we'd like a better solution and likely this code will go back
  // to being platform-app only.

  // Make sure there is an app window associated with the web contents, so that
  // platform apps cannot open the file picker from a background page.
  // TODO(michaelpg): As a workaround for https://crbug.com/736930, allow this
  // to work from a background page for non-platform apps (which, in practice,
  // is restricted to whitelisted extensions).
  if (extension->is_platform_app() &&
      !AppWindowRegistry::Get(extension_function->browser_context())
           ->GetAppWindowForWebContents(web_contents)) {
    return false;
  }

  // The file picker will hold a reference to the ExtensionFunction
  // instance, preventing its destruction (and subsequent sending of the
  // function response) until the user has selected a file or cancelled the
  // picker. At that point, the picker will delete itself, which will also free
  // the function instance.
  new FileEntryPicker(web_contents, default_path, *file_types, type,
                      std::move(files_selected_callback),
                      std::move(file_selection_canceled_callback));
  return true;
}

void ChromeFileSystemDelegate::ConfirmSensitiveDirectoryAccess(
    bool has_write_permission,
    const base::string16& app_name,
    content::WebContents* web_contents,
    const base::Closure& on_accept,
    const base::Closure& on_cancel) {
  CreateDirectoryAccessConfirmationDialog(has_write_permission, app_name,
                                          web_contents, on_accept, on_cancel);
}

int ChromeFileSystemDelegate::GetDescriptionIdForAcceptType(
    const std::string& accept_type) {
  if (accept_type == "image/*")
    return IDS_IMAGE_FILES;
  if (accept_type == "audio/*")
    return IDS_AUDIO_FILES;
  if (accept_type == "video/*")
    return IDS_VIDEO_FILES;
  return 0;
}

#if defined(OS_CHROMEOS)
FileSystemDelegate::GrantVolumesMode
ChromeFileSystemDelegate::GetGrantVolumesMode(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* render_frame_host,
    const Extension& extension) {
  // Only kiosk apps in kiosk sessions can use this API.
  // Additionally it is enabled for whitelisted component extensions and apps.
  ConsentProviderDelegate consent_provider_delegate(
      Profile::FromBrowserContext(browser_context));
  return ConsentProvider(&consent_provider_delegate)
      .GetGrantVolumesMode(extension);
}

void ChromeFileSystemDelegate::RequestFileSystem(
    content::BrowserContext* browser_context,
    scoped_refptr<ExtensionFunction> requester,
    const Extension& extension,
    std::string volume_id,
    bool writable,
    const FileSystemCallback& success_callback,
    const ErrorCallback& error_callback) {
  ConsentProviderDelegate consent_provider_delegate(
      Profile::FromBrowserContext(browser_context));
  ConsentProvider consent_provider(&consent_provider_delegate);

  using file_manager::VolumeManager;
  using file_manager::Volume;
  VolumeManager* const volume_manager = VolumeManager::Get(browser_context);
  DCHECK(volume_manager);

  if (writable &&
      !app_file_handler_util::HasFileSystemWritePermission(&extension)) {
    error_callback.Run(kRequiresFileSystemWriteError);
    return;
  }

  if (consent_provider.GetGrantVolumesMode(extension) ==
      FileSystemDelegate::kGrantNone) {
    error_callback.Run(kNotSupportedOnNonKioskSessionError);
    return;
  }

  base::WeakPtr<file_manager::Volume> volume =
      volume_manager->FindVolumeById(volume_id);
  if (!volume.get() ||
      !consent_provider.IsGrantableForVolume(extension, volume)) {
    error_callback.Run(kVolumeNotFoundError);
    return;
  }

  const GURL site =
      util::GetSiteForExtensionId(extension.id(), browser_context);
  scoped_refptr<storage::FileSystemContext> file_system_context =
      content::BrowserContext::GetStoragePartitionForSite(browser_context, site)
          ->GetFileSystemContext();
  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
  DCHECK(backend);

  base::FilePath virtual_path;
  if (!backend->GetVirtualPath(volume->mount_path(), &virtual_path)) {
    error_callback.Run(kSecurityError);
    return;
  }

  if (writable && (volume->is_read_only())) {
    error_callback.Run(kSecurityError);
    return;
  }

  const ConsentProvider::ConsentCallback& callback = base::Bind(
      &OnConsentReceived, browser_context, requester, success_callback,
      error_callback, extension.id(), volume, writable);

  consent_provider.RequestConsent(extension, requester->render_frame_host(),
                                  volume, writable, callback);
}

void ChromeFileSystemDelegate::GetVolumeList(
    content::BrowserContext* browser_context,
    const Extension& extension,
    const VolumeListCallback& success_callback,
    const ErrorCallback& error_callback) {
  ConsentProviderDelegate consent_provider_delegate(
      Profile::FromBrowserContext(browser_context));
  ConsentProvider consent_provider(&consent_provider_delegate);

  const std::vector<base::WeakPtr<file_manager::Volume>> volume_list =
      file_manager::VolumeManager::Get(browser_context)->GetVolumeList();
  std::vector<file_system::Volume> result_volume_list;

  GetVolumeListForExtension(volume_list, &consent_provider, extension,
                            &result_volume_list);
  success_callback.Run(result_volume_list);
}

#endif  // defined(OS_CHROMEOS)

SavedFilesServiceInterface* ChromeFileSystemDelegate::GetSavedFilesService(
    content::BrowserContext* browser_context) {
  return apps::SavedFilesService::Get(browser_context);
}

}  // namespace extensions
