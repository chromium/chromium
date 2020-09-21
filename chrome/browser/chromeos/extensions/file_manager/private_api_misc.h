// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides miscellaneous API functions, which don't belong to
// other files.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/services/printing/public/mojom/pdf_thumbnailer.mojom.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/file_system/file_system_url.h"

class SkBitmap;

namespace chromeos {
class RecentFile;
}  // namespace chromeos

namespace crostini {
enum class CrostiniResult;
struct LinuxPackageInfo;
}

namespace file_manager {
namespace util {
struct EntryDefinition;
typedef std::vector<EntryDefinition> EntryDefinitionList;
}  // namespace util
}  // namespace file_manager

namespace google_apis {
class AuthServiceInterface;
}  // namespace google_apis

namespace extensions {

// Implements the chrome.fileManagerPrivate.logoutUserForReauthentication
// method.
class FileManagerPrivateLogoutUserForReauthenticationFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.logoutUserForReauthentication",
                             FILEMANAGERPRIVATE_LOGOUTUSERFORREAUTHENTICATION)

 protected:
  ~FileManagerPrivateLogoutUserForReauthenticationFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getPreferences method.
// Gets settings for the Files app.
class FileManagerPrivateGetPreferencesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getPreferences",
                             FILEMANAGERPRIVATE_GETPREFERENCES)

 protected:
  ~FileManagerPrivateGetPreferencesFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.setPreferences method.
// Sets settings for the Files app.
class FileManagerPrivateSetPreferencesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.setPreferences",
                             FILEMANAGERPRIVATE_SETPREFERENCES)

 protected:
  ~FileManagerPrivateSetPreferencesFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.zipSelection method.
// Creates a zip file for the selected files.
class FileManagerPrivateInternalZipSelectionFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.zipSelection",
                             FILEMANAGERPRIVATEINTERNAL_ZIPSELECTION)

  FileManagerPrivateInternalZipSelectionFunction();

 protected:
  ~FileManagerPrivateInternalZipSelectionFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // Receives the result from ZipFileCreator.
  void OnZipDone(bool success);
};

// Implements the chrome.fileManagerPrivate.zoom method.
// Changes the zoom level of the file manager by modifying the zoom level of the
// WebContents.
// TODO(hirono): Remove this function once the zoom level change is supported
// for all apps. crbug.com/227175.
class FileManagerPrivateZoomFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.zoom", FILEMANAGERPRIVATE_ZOOM)

 protected:
  ~FileManagerPrivateZoomFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class FileManagerPrivateRequestWebStoreAccessTokenFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.requestWebStoreAccessToken",
                             FILEMANAGERPRIVATE_REQUESTWEBSTOREACCESSTOKEN)

  FileManagerPrivateRequestWebStoreAccessTokenFunction();

 protected:
  ~FileManagerPrivateRequestWebStoreAccessTokenFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  std::unique_ptr<google_apis::AuthServiceInterface> auth_service_;

  void OnAccessTokenFetched(google_apis::DriveApiErrorCode code,
                            const std::string& access_token);
  const ChromeExtensionFunctionDetails chrome_details_;
};

class FileManagerPrivateGetProfilesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getProfiles",
                             FILEMANAGERPRIVATE_GETPROFILES)

 protected:
  ~FileManagerPrivateGetProfilesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.openInspector method.
class FileManagerPrivateOpenInspectorFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.openInspector",
                             FILEMANAGERPRIVATE_OPENINSPECTOR)

 protected:
  ~FileManagerPrivateOpenInspectorFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.openSettingsSubpage method.
class FileManagerPrivateOpenSettingsSubpageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.openSettingsSubpage",
                             FILEMANAGERPRIVATE_OPENSETTINGSSUBPAGE)

 protected:
  ~FileManagerPrivateOpenSettingsSubpageFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getMimeType method.
class FileManagerPrivateInternalGetMimeTypeFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getMimeType",
                             FILEMANAGERPRIVATEINTERNAL_GETMIMETYPE)

  FileManagerPrivateInternalGetMimeTypeFunction();

 protected:
  ~FileManagerPrivateInternalGetMimeTypeFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  void OnGetMimeType(const std::string& mimeType);
};

// Implements the chrome.fileManagerPrivate.getProviders method.
class FileManagerPrivateGetProvidersFunction : public ExtensionFunction {
 public:
  FileManagerPrivateGetProvidersFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getProviders",
                             FILEMANAGERPRIVATE_GETPROVIDERS)

  FileManagerPrivateGetProvidersFunction(
      const FileManagerPrivateGetProvidersFunction&) = delete;
  FileManagerPrivateGetProvidersFunction& operator=(
      const FileManagerPrivateGetProvidersFunction&) = delete;

 protected:
  ~FileManagerPrivateGetProvidersFunction() override = default;

 private:
  ResponseAction Run() override;
  const ChromeExtensionFunctionDetails chrome_details_;
};

// Implements the chrome.fileManagerPrivate.addProvidedFileSystem method.
class FileManagerPrivateAddProvidedFileSystemFunction
    : public ExtensionFunction {
 public:
  FileManagerPrivateAddProvidedFileSystemFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.addProvidedFileSystem",
                             FILEMANAGERPRIVATE_ADDPROVIDEDFILESYSTEM)

  FileManagerPrivateAddProvidedFileSystemFunction(
      const FileManagerPrivateAddProvidedFileSystemFunction&) = delete;
  FileManagerPrivateAddProvidedFileSystemFunction& operator=(
      const FileManagerPrivateAddProvidedFileSystemFunction&) = delete;

 protected:
  ~FileManagerPrivateAddProvidedFileSystemFunction() override = default;

 private:
  ResponseAction Run() override;
  const ChromeExtensionFunctionDetails chrome_details_;
};

// Implements the chrome.fileManagerPrivate.configureVolume method.
class FileManagerPrivateConfigureVolumeFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateConfigureVolumeFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.configureVolume",
                             FILEMANAGERPRIVATE_CONFIGUREVOLUME)

  FileManagerPrivateConfigureVolumeFunction(
      const FileManagerPrivateConfigureVolumeFunction&) = delete;
  FileManagerPrivateConfigureVolumeFunction& operator=(
      const FileManagerPrivateConfigureVolumeFunction&) = delete;

 protected:
  ~FileManagerPrivateConfigureVolumeFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnCompleted(base::File::Error result);

  const ChromeExtensionFunctionDetails chrome_details_;
};

// Implements the chrome.fileManagerPrivate.mountCrostini method.
// Starts and mounts crostini container.
class FileManagerPrivateMountCrostiniFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.mountCrostini",
                             FILEMANAGERPRIVATE_MOUNTCROSTINI)
  FileManagerPrivateMountCrostiniFunction();

  FileManagerPrivateMountCrostiniFunction(
      const FileManagerPrivateMountCrostiniFunction&) = delete;
  FileManagerPrivateMountCrostiniFunction& operator=(
      const FileManagerPrivateMountCrostiniFunction&) = delete;

 protected:
  ~FileManagerPrivateMountCrostiniFunction() override;

  ResponseAction Run() override;
  void RestartCallback(crostini::CrostiniResult);

 private:
  std::string source_path_;
  std::string mount_label_;
};

// Implements the chrome.fileManagerPrivate.importCrostiniImage method.
// Starts importing the crostini .tini image.
class FileManagerPrivateInternalImportCrostiniImageFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.importCrostiniImage",
                             FILEMANAGERPRIVATEINTERNAL_IMPORTCROSTINIIMAGE)
  FileManagerPrivateInternalImportCrostiniImageFunction();

  FileManagerPrivateInternalImportCrostiniImageFunction(
      const FileManagerPrivateInternalImportCrostiniImageFunction&) = delete;
  FileManagerPrivateInternalImportCrostiniImageFunction& operator=(
      const FileManagerPrivateInternalImportCrostiniImageFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalImportCrostiniImageFunction() override;

 private:
  ResponseAction Run() override;

  std::string image_path_;
};

// Implements the chrome.fileManagerPrivate.sharePathsWithCrostini
// method.  Shares specified paths.
class FileManagerPrivateInternalSharePathsWithCrostiniFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.sharePathsWithCrostini",
      FILEMANAGERPRIVATEINTERNAL_SHAREPATHSWITHCROSTINI)
  FileManagerPrivateInternalSharePathsWithCrostiniFunction() = default;

  FileManagerPrivateInternalSharePathsWithCrostiniFunction(
      const FileManagerPrivateInternalSharePathsWithCrostiniFunction&) = delete;
  FileManagerPrivateInternalSharePathsWithCrostiniFunction& operator=(
      const FileManagerPrivateInternalSharePathsWithCrostiniFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalSharePathsWithCrostiniFunction() override =
      default;

 private:
  ResponseAction Run() override;
  void SharePathsCallback(bool success, const std::string& failure_reason);
};

// Implements the chrome.fileManagerPrivate.unsharePathWithCrostini
// method.  Unshares specified path.
class FileManagerPrivateInternalUnsharePathWithCrostiniFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.unsharePathWithCrostini",
      FILEMANAGERPRIVATEINTERNAL_UNSHAREPATHWITHCROSTINI)
  FileManagerPrivateInternalUnsharePathWithCrostiniFunction() = default;

  FileManagerPrivateInternalUnsharePathWithCrostiniFunction(
      const FileManagerPrivateInternalUnsharePathWithCrostiniFunction&) =
      delete;
  FileManagerPrivateInternalUnsharePathWithCrostiniFunction& operator=(
      const FileManagerPrivateInternalUnsharePathWithCrostiniFunction&) =
      delete;

 protected:
  ~FileManagerPrivateInternalUnsharePathWithCrostiniFunction() override =
      default;

 private:
  ResponseAction Run() override;
  void UnsharePathCallback(bool success, const std::string& failure_reason);
};

// Implements the chrome.fileManagerPrivate.getCrostiniSharedPaths
// method.  Returns list of file entries.
class FileManagerPrivateInternalGetCrostiniSharedPathsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.getCrostiniSharedPaths",
      FILEMANAGERPRIVATEINTERNAL_GETCROSTINISHAREDPATHS)
  FileManagerPrivateInternalGetCrostiniSharedPathsFunction() = default;

  FileManagerPrivateInternalGetCrostiniSharedPathsFunction(
      const FileManagerPrivateInternalGetCrostiniSharedPathsFunction&) = delete;
  FileManagerPrivateInternalGetCrostiniSharedPathsFunction operator=(
      const FileManagerPrivateInternalGetCrostiniSharedPathsFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalGetCrostiniSharedPathsFunction() override =
      default;

 private:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getLinuxPackageInfo method.
// Retrieves information about a Linux package.
class FileManagerPrivateInternalGetLinuxPackageInfoFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getLinuxPackageInfo",
                             FILEMANAGERPRIVATEINTERNAL_GETLINUXPACKAGEINFO)
  FileManagerPrivateInternalGetLinuxPackageInfoFunction() = default;

  FileManagerPrivateInternalGetLinuxPackageInfoFunction(
      const FileManagerPrivateInternalGetLinuxPackageInfoFunction&) = delete;
  FileManagerPrivateInternalGetLinuxPackageInfoFunction operator=(
      const FileManagerPrivateInternalGetLinuxPackageInfoFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalGetLinuxPackageInfoFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnGetLinuxPackageInfo(
      const crostini::LinuxPackageInfo& linux_package_info);
};

// Implements the chrome.fileManagerPrivate.installLinuxPackage method.
// Starts installation of a Linux package.
class FileManagerPrivateInternalInstallLinuxPackageFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.installLinuxPackage",
                             FILEMANAGERPRIVATEINTERNAL_INSTALLLINUXPACKAGE)
  FileManagerPrivateInternalInstallLinuxPackageFunction() = default;

  FileManagerPrivateInternalInstallLinuxPackageFunction(
      const FileManagerPrivateInternalInstallLinuxPackageFunction&) = delete;
  FileManagerPrivateInternalInstallLinuxPackageFunction operator=(
      const FileManagerPrivateInternalInstallLinuxPackageFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalInstallLinuxPackageFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnInstallLinuxPackage(crostini::CrostiniResult result);
};

// Implements the chrome.fileManagerPrivate.getCustomActions method.
class FileManagerPrivateInternalGetCustomActionsFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetCustomActionsFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getCustomActions",
                             FILEMANAGERPRIVATEINTERNAL_GETCUSTOMACTIONS)

  FileManagerPrivateInternalGetCustomActionsFunction(
      const FileManagerPrivateInternalGetCustomActionsFunction&) = delete;
  FileManagerPrivateInternalGetCustomActionsFunction operator=(
      const FileManagerPrivateInternalGetCustomActionsFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalGetCustomActionsFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnCompleted(const chromeos::file_system_provider::Actions& actions,
                   base::File::Error result);

  const ChromeExtensionFunctionDetails chrome_details_;
};

// Implements the chrome.fileManagerPrivate.executeCustomAction method.
class FileManagerPrivateInternalExecuteCustomActionFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalExecuteCustomActionFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.executeCustomAction",
                             FILEMANAGERPRIVATEINTERNAL_EXECUTECUSTOMACTION)

  FileManagerPrivateInternalExecuteCustomActionFunction(
      const FileManagerPrivateInternalExecuteCustomActionFunction&) = delete;
  FileManagerPrivateInternalExecuteCustomActionFunction operator=(
      const FileManagerPrivateInternalExecuteCustomActionFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalExecuteCustomActionFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnCompleted(base::File::Error result);

  const ChromeExtensionFunctionDetails chrome_details_;
};

// Implements the chrome.fileManagerPrivateInternal.getRecentFiles method.
class FileManagerPrivateInternalGetRecentFilesFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetRecentFilesFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getRecentFiles",
                             FILEMANAGERPRIVATE_GETRECENTFILES)

  FileManagerPrivateInternalGetRecentFilesFunction(
      const FileManagerPrivateInternalGetRecentFilesFunction&) = delete;
  FileManagerPrivateInternalGetRecentFilesFunction& operator=(
      FileManagerPrivateInternalGetRecentFilesFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalGetRecentFilesFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnGetRecentFiles(
      api::file_manager_private::SourceRestriction restriction,
      const std::vector<chromeos::RecentFile>& files);
  void OnConvertFileDefinitionListToEntryDefinitionList(
      std::unique_ptr<file_manager::util::EntryDefinitionList>
          entry_definition_list);

  const ChromeExtensionFunctionDetails chrome_details_;
};

// Implements the chrome.fileManagerPrivate.detectCharacterEncoding method.
class FileManagerPrivateDetectCharacterEncodingFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.detectCharacterEncoding",
                             FILEMANAGERPRIVATE_DETECTCHARACTERENCODING)

 protected:
  ~FileManagerPrivateDetectCharacterEncodingFunction() override = default;

  ResponseAction Run() override;
};

class FileManagerPrivateInternalGetThumbnailFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getThumbnail",
                             FILEMANAGERPRIVATEINTERNAL_GETTHUMBNAIL)

  FileManagerPrivateInternalGetThumbnailFunction();

 protected:
  ~FileManagerPrivateInternalGetThumbnailFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // Attempts to generate a thumbnail for the given url. This method
  // checks that the URL references a Google Drive file.
  ResponseAction GetDrivefsThumbnail(
      const ChromeExtensionFunctionDetails& chrome_details,
      const storage::FileSystemURL& url,
      bool crop_to_square);

  // Attempts to generate a thumbnail for the given url. This method checks
  // that the URL references a local PDF file. If so, it attempts to generate
  // a one page thumbnail.
  ResponseAction GetLocalThumbnail(
      const ChromeExtensionFunctionDetails& chrome_details,
      const storage::FileSystemURL& url,
      bool crop_to_square);

  // For a given |content| starts fetching the first page PDF thumbnail by
  // calling PdfThumbnailer from the printing service. The first parameter,
  // |crop_to_square| is supplied by the JavaScript caller.
  void FetchPdfThumbnail(bool crop_to_square, const std::string& content);

  // Callback invoked by the thumbnailing service when a PDF thumbnail has been
  // generated. The solitary parameter |bitmap| is supplied by the callback.
  // If |bitmap| is null, an error occurred. Otherwise, |bitmap| contains the
  // generated thumbnail.
  void GotPdfThumbnail(const SkBitmap& bitmap);

  // Handles a mojo channel disconnect event.
  void PdfThumbnailDisconected();

  // A callback invoked when thumbnail data has been generated.
  void GotDriveThumbnail(const base::Optional<std::vector<uint8_t>>& data);

  // Responds with a base64 encoded PNG thumbnail data.
  void SendEncodedThumbnail(std::string thumbnail_data_url);

  // Holds the channel to Printing PDF thumbnailing service. Bound only
  // when needed.
  mojo::Remote<printing::mojom::PdfThumbnailer> pdf_thumbnailer_;

  // The dots per inch (dpi) resolution at which the PDF is rendered to a
  // thumbnail. The value of 30 is selected so that a US Letter size page does
  // not overflow a kSize x kSize thumbnail.
  constexpr static int kDpi = 30;

  // The default size if we are asked to generate a square thumbnail. The
  // value is set to match chromeos/components/drivefs/mojom/drivefs.mojom
  constexpr static int kSize = 360;

  // The default width if we are asked to generate a non-square thumbnail. The
  // value is set to match chromeos/components/drivefs/mojom/drivefs.mojom
  constexpr static int kWidth = 500;

  // The default height if we are asked to generate a non-square thumbnail. The
  // value is set to match chromeos/components/drivefs/mojom/drivefs.mojom
  constexpr static int kHeight = 500;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_
