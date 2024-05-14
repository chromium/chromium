// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate.h"

#include <string>
#include <utility>

#include "apps/saved_files_service.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/file_system/file_entry_picker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/directory_access_confirmation_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/file_system/saved_files_service_interface.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/extension.h"
#include "storage/common/file_system/file_system_util.h"

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include "base/apple/foundation_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/browser/event_router.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extensions {

namespace file_system = api::file_system;

#if BUILDFLAG(IS_CHROMEOS)

namespace file_system_api {

const char kConsentImpossible[] =
    "Impossible to ask for user consent as there is no app window visible.";
const char kNotSupportedOnNonKioskSessionError[] =
    "Operation only supported for kiosk apps running in a kiosk session.";
const char kRequiresFileSystemWriteError[] =
    "Operation requires fileSystem.write permission";
const char kSecurityError[] = "Security error.";
const char kVolumeNotFoundError[] = "Volume not found.";

// Returns error message, or null if none.
const char* ConsentResultToError(ConsentProvider::Consent result) {
  switch (result) {
    case ConsentProvider::CONSENT_REJECTED:
      return kSecurityError;
    case ConsentProvider::CONSENT_IMPOSSIBLE:
      return kConsentImpossible;
    case ConsentProvider::CONSENT_GRANTED:
      return nullptr;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace file_system_api
#endif  // BUILDFLAG(IS_CHROMEOS)

/******** ChromeFileSystemDelegate ********/

ChromeFileSystemDelegate::ChromeFileSystemDelegate() = default;

ChromeFileSystemDelegate::~ChromeFileSystemDelegate() = default;

base::FilePath ChromeFileSystemDelegate::GetDefaultDirectory() {
  base::FilePath documents_dir;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &documents_dir);
  return documents_dir;
}

base::FilePath ChromeFileSystemDelegate::GetManagedSaveAsDirectory(
    content::BrowserContext* browser_context,
    const Extension& extension) {
  if (extension.id() != extension_misc::kPdfExtensionId)
    return base::FilePath();

  ChromeDownloadManagerDelegate* download_manager =
      DownloadCoreServiceFactory::GetForBrowserContext(browser_context)
          ->GetDownloadManagerDelegate();
  DownloadPrefs* download_prefs = download_manager->download_prefs();
  if (!download_prefs->IsDownloadPathManaged())
    return base::FilePath();
  return download_prefs->DownloadPath();
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
  // crbug.com/179010 we're adding the ability for a allowlisted extension to
  // use this API since chrome.fileBrowserHandler.selectFile is ChromeOS-only.
  // Eventually we'd like a better solution and likely this code will go back
  // to being platform-app only.

  // Make sure there is an app window associated with the web contents, so that
  // platform apps cannot open the file picker from a background page.
  // TODO(michaelpg): As a workaround for https://crbug.com/736930, allow this
  // to work from a background page for non-platform apps (which, in practice,
  // is restricted to allowlisted extensions).
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
    const std::u16string& app_name,
    content::WebContents* web_contents,
    base::OnceClosure on_accept,
    base::OnceClosure on_cancel) {
  CreateDirectoryAccessConfirmationDialog(has_write_permission, app_name,
                                          web_contents, std::move(on_accept),
                                          std::move(on_cancel));
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

#if BUILDFLAG(IS_CHROMEOS)
void ChromeFileSystemDelegate::RequestFileSystem(
    content::BrowserContext* browser_context,
    scoped_refptr<ExtensionFunction> requester,
    ConsentProvider* consent_provider,
    const Extension& extension,
    std::string volume_id,
    bool writable,
    FileSystemCallback success_callback,
    ErrorCallback error_callback) {}

void ChromeFileSystemDelegate::GetVolumeList(
    content::BrowserContext* browser_context,
    VolumeListCallback success_callback,
    ErrorCallback error_callback) {}
#endif  // BUILDFLAG(IS_CHROMEOS)

SavedFilesServiceInterface* ChromeFileSystemDelegate::GetSavedFilesService(
    content::BrowserContext* browser_context) {
  return apps::SavedFilesService::Get(browser_context);
}

}  // namespace extensions
