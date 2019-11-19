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
#include "base/macros.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "storage/browser/file_system/file_system_url.h"

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

// Implements the chrome.fileManagerPrivate.isPiexLoaderEnabled method.
class FileManagerPrivateIsPiexLoaderEnabledFunction : public ExtensionFunction {
 public:
  FileManagerPrivateIsPiexLoaderEnabledFunction() = default;
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.isPiexLoaderEnabled",
                             FILEMANAGERPRIVATE_ISPIEXLOADERENABLED)
 protected:
  ~FileManagerPrivateIsPiexLoaderEnabledFunction() override = default;

 private:
  ResponseAction Run() override;
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateIsPiexLoaderEnabledFunction);
};

// Implements the chrome.fileManagerPrivate.getProviders method.
class FileManagerPrivateGetProvidersFunction : public ExtensionFunction {
 public:
  FileManagerPrivateGetProvidersFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getProviders",
                             FILEMANAGERPRIVATE_GETPROVIDERS)
 protected:
  ~FileManagerPrivateGetProvidersFunction() override = default;

 private:
  ResponseAction Run() override;
  const ChromeExtensionFunctionDetails chrome_details_;
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateGetProvidersFunction);
};

// Implements the chrome.fileManagerPrivate.addProvidedFileSystem method.
class FileManagerPrivateAddProvidedFileSystemFunction
    : public ExtensionFunction {
 public:
  FileManagerPrivateAddProvidedFileSystemFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.addProvidedFileSystem",
                             FILEMANAGERPRIVATE_ADDPROVIDEDFILESYSTEM)
 protected:
  ~FileManagerPrivateAddProvidedFileSystemFunction() override = default;

 private:
  ResponseAction Run() override;
  const ChromeExtensionFunctionDetails chrome_details_;
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateAddProvidedFileSystemFunction);
};

// Implements the chrome.fileManagerPrivate.configureVolume method.
class FileManagerPrivateConfigureVolumeFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateConfigureVolumeFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.configureVolume",
                             FILEMANAGERPRIVATE_CONFIGUREVOLUME)
 protected:
  ~FileManagerPrivateConfigureVolumeFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnCompleted(base::File::Error result);

  const ChromeExtensionFunctionDetails chrome_details_;
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateConfigureVolumeFunction);
};

// Implements the chrome.fileManagerPrivate.mountCrostini method.
// Starts and mounts crostini container.
class FileManagerPrivateMountCrostiniFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.mountCrostini",
                             FILEMANAGERPRIVATE_MOUNTCROSTINI)
  FileManagerPrivateMountCrostiniFunction();

 protected:
  ~FileManagerPrivateMountCrostiniFunction() override;

  ResponseAction Run() override;
  void RestartCallback(crostini::CrostiniResult);

 private:
  std::string source_path_;
  std::string mount_label_;
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateMountCrostiniFunction);
};

// Implements the chrome.fileManagerPrivate.importCrostiniImage method.
// Starts importing the crostini .tini image.
class FileManagerPrivateInternalImportCrostiniImageFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.importCrostiniImage",
                             FILEMANAGERPRIVATEINTERNAL_IMPORTCROSTINIIMAGE)
  FileManagerPrivateInternalImportCrostiniImageFunction();

 protected:
  ~FileManagerPrivateInternalImportCrostiniImageFunction() override;

 private:
  ResponseAction Run() override;

  std::string image_path_;
  DISALLOW_COPY_AND_ASSIGN(
      FileManagerPrivateInternalImportCrostiniImageFunction);
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

 protected:
  ~FileManagerPrivateInternalSharePathsWithCrostiniFunction() override =
      default;

 private:
  ResponseAction Run() override;
  void SharePathsCallback(bool success, const std::string& failure_reason);
  DISALLOW_COPY_AND_ASSIGN(
      FileManagerPrivateInternalSharePathsWithCrostiniFunction);
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

 protected:
  ~FileManagerPrivateInternalUnsharePathWithCrostiniFunction() override =
      default;

 private:
  ResponseAction Run() override;
  void UnsharePathCallback(bool success, const std::string& failure_reason);
  DISALLOW_COPY_AND_ASSIGN(
      FileManagerPrivateInternalUnsharePathWithCrostiniFunction);
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

 protected:
  ~FileManagerPrivateInternalGetCrostiniSharedPathsFunction() override =
      default;

 private:
  ResponseAction Run() override;
  DISALLOW_COPY_AND_ASSIGN(
      FileManagerPrivateInternalGetCrostiniSharedPathsFunction);
};

// Implements the chrome.fileManagerPrivate.getLinuxPackageInfo method.
// Retrieves information about a Linux package.
class FileManagerPrivateInternalGetLinuxPackageInfoFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getLinuxPackageInfo",
                             FILEMANAGERPRIVATEINTERNAL_GETLINUXPACKAGEINFO)
  FileManagerPrivateInternalGetLinuxPackageInfoFunction() = default;

 protected:
  ~FileManagerPrivateInternalGetLinuxPackageInfoFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnGetLinuxPackageInfo(
      const crostini::LinuxPackageInfo& linux_package_info);
  DISALLOW_COPY_AND_ASSIGN(
      FileManagerPrivateInternalGetLinuxPackageInfoFunction);
};

// Implements the chrome.fileManagerPrivate.installLinuxPackage method.
// Starts installation of a Linux package.
class FileManagerPrivateInternalInstallLinuxPackageFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.installLinuxPackage",
                             FILEMANAGERPRIVATEINTERNAL_INSTALLLINUXPACKAGE)
  FileManagerPrivateInternalInstallLinuxPackageFunction() = default;

 protected:
  ~FileManagerPrivateInternalInstallLinuxPackageFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnInstallLinuxPackage(crostini::CrostiniResult result);
  DISALLOW_COPY_AND_ASSIGN(
      FileManagerPrivateInternalInstallLinuxPackageFunction);
};

// Implements the chrome.fileManagerPrivate.getCustomActions method.
class FileManagerPrivateInternalGetCustomActionsFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetCustomActionsFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getCustomActions",
                             FILEMANAGERPRIVATEINTERNAL_GETCUSTOMACTIONS)
 protected:
  ~FileManagerPrivateInternalGetCustomActionsFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnCompleted(const chromeos::file_system_provider::Actions& actions,
                   base::File::Error result);

  const ChromeExtensionFunctionDetails chrome_details_;
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateInternalGetCustomActionsFunction);
};

// Implements the chrome.fileManagerPrivate.executeCustomAction method.
class FileManagerPrivateInternalExecuteCustomActionFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalExecuteCustomActionFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.executeCustomAction",
                             FILEMANAGERPRIVATEINTERNAL_EXECUTECUSTOMACTION)
 protected:
  ~FileManagerPrivateInternalExecuteCustomActionFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnCompleted(base::File::Error result);

  const ChromeExtensionFunctionDetails chrome_details_;
  DISALLOW_COPY_AND_ASSIGN(
      FileManagerPrivateInternalExecuteCustomActionFunction);
};

// Implements the chrome.fileManagerPrivateInternal.getRecentFiles method.
class FileManagerPrivateInternalGetRecentFilesFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetRecentFilesFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getRecentFiles",
                             FILEMANAGERPRIVATE_GETRECENTFILES)
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
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateInternalGetRecentFilesFunction);
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

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_
