// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides Drive specific API functions.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/time/time.h"
#include "chrome/browser/ash/extensions/file_manager/logged_extension_function.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "components/drive/file_errors.h"

namespace extensions {

namespace api {
namespace file_manager_private {
struct EntryProperties;
}  // namespace file_manager_private
}  // namespace api

// Retrieves property information for an entry and returns it as a dictionary.
// On error, returns a dictionary with the key "error" set to the error number
// (base::File::Error).
class FileManagerPrivateInternalGetEntryPropertiesFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getEntryProperties",
                             FILEMANAGERPRIVATEINTERNAL_GETENTRYPROPERTIES)

  FileManagerPrivateInternalGetEntryPropertiesFunction();

 protected:
  ~FileManagerPrivateInternalGetEntryPropertiesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void CompleteGetEntryProperties(
      size_t index,
      const storage::FileSystemURL& url,
      std::unique_ptr<api::file_manager_private::EntryProperties> properties,
      base::File::Error error);

  size_t processed_count_;
  std::vector<api::file_manager_private::EntryProperties> properties_list_;
};

// Implements the chrome.fileManagerPrivate.pinDriveFile method.
class FileManagerPrivateInternalPinDriveFileFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalPinDriveFileFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.pinDriveFile",
                             FILEMANAGERPRIVATEINTERNAL_PINDRIVEFILE)

 protected:
  ~FileManagerPrivateInternalPinDriveFileFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ResponseAction RunAsyncForDriveFs(
      const storage::FileSystemURL& file_system_url,
      bool pin);

  // Callback for and RunAsyncForDriveFs().
  void OnPinStateSet(drive::FileError error);
};

class FileManagerPrivateSearchDriveFunction : public LoggedExtensionFunction {
 public:
  FileManagerPrivateSearchDriveFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.searchDrive",
                             FILEMANAGERPRIVATE_SEARCHDRIVE)

 protected:
  ~FileManagerPrivateSearchDriveFunction() override = default;

  ResponseAction Run() override;

 private:
  void OnSearchDriveFs(std::optional<base::Value::List> results);

  base::TimeTicks operation_start_;
  bool is_offline_;
};

// Similar to FileManagerPrivateSearchDriveFunction but this one is used for
// searching drive metadata which is stored locally.
class FileManagerPrivateSearchDriveMetadataFunction
    : public LoggedExtensionFunction {
 public:
  enum class SearchType {
    kText,
    kSharedWithMe,
    kOffline,
  };

  FileManagerPrivateSearchDriveMetadataFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.searchDriveMetadata",
                             FILEMANAGERPRIVATE_SEARCHDRIVEMETADATA)

 protected:
  ~FileManagerPrivateSearchDriveMetadataFunction() override = default;

  ResponseAction Run() override;

 private:
  void OnSearchDriveFs(const std::string& query_text,
                       std::optional<base::Value::List> results);

  base::TimeTicks operation_start_;
  SearchType search_type_;
  bool is_offline_;
};

// Implements the chrome.fileManagerPrivate.getDriveConnectionState method.
class FileManagerPrivateGetDriveConnectionStateFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getDriveConnectionState",
                             FILEMANAGERPRIVATE_GETDRIVECONNECTIONSTATE)

 protected:
  ~FileManagerPrivateGetDriveConnectionStateFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.notifyDriveDialogResult method.
class FileManagerPrivateNotifyDriveDialogResultFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.notifyDriveDialogResult",
                             FILEMANAGERPRIVATE_NOTIFYDRIVEDIALOGRESULT)

 protected:
  ~FileManagerPrivateNotifyDriveDialogResultFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.pollDriveHostedFilePinStates method.
class FileManagerPrivatePollDriveHostedFilePinStatesFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.pollDriveHostedFilePinStates",
                             FILEMANAGERPRIVATE_POLLDRIVEHOSTEDFILEPINSTATES)

 protected:
  ~FileManagerPrivatePollDriveHostedFilePinStatesFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.openManageSyncSettings method.
class FileManagerPrivateOpenManageSyncSettingsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.openManageSyncSettings",
                             FILEMANAGERPRIVATE_OPENMANAGESYNCSETTINGS)

 protected:
  ~FileManagerPrivateOpenManageSyncSettingsFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getBulkPinProgress method.
class FileManagerPrivateGetBulkPinProgressFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getBulkPinProgress",
                             FILEMANAGERPRIVATE_GETBULKPINPROGRESS)

 protected:
  ~FileManagerPrivateGetBulkPinProgressFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.calculateBulkPinRequiredSpace
// method.
class FileManagerPrivateCalculateBulkPinRequiredSpaceFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.calculateBulkPinRequiredSpace",
                             FILEMANAGERPRIVATE_CALCULATEBULKPINREQUIREDSPACE)

 protected:
  ~FileManagerPrivateCalculateBulkPinRequiredSpaceFunction() override = default;

  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_
