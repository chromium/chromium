// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides file system related API functions.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/extensions/file_manager/logged_extension_function.h"
#include "chrome/browser/ash/file_manager/trash_info_validator.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "components/drive/file_errors.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "services/device/public/mojom/mtp_storage_info.mojom-forward.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace storage {
class FileSystemContext;
class FileSystemURL;
class WatcherManager;
}  // namespace storage

namespace file_manager {
class EventRouter;
namespace util {
struct EntryDefinition;
using EntryDefinitionList = std::vector<EntryDefinition>;
}  // namespace util
}  // namespace file_manager

namespace drive::policy {
class DlpFilesControllerAsh;
}  // namespace drive::policy

namespace extensions {

// Grant permission to request externalfile scheme. The permission is needed to
// start drag for external file URL.
class FileManagerPrivateEnableExternalFileSchemeFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.enableExternalFileScheme",
                             FILEMANAGERPRIVATE_ENABLEEXTERNALFILESCHEME)

 protected:
  ~FileManagerPrivateEnableExternalFileSchemeFunction() override = default;

 private:
  ExtensionFunction::ResponseAction Run() override;
};

// Grants R/W permissions to profile-specific directories (Drive, Downloads)
// from other profiles.
class FileManagerPrivateGrantAccessFunction : public ExtensionFunction {
 public:
  FileManagerPrivateGrantAccessFunction();

  FileManagerPrivateGrantAccessFunction(
      const FileManagerPrivateGrantAccessFunction&) = delete;
  FileManagerPrivateGrantAccessFunction& operator=(
      const FileManagerPrivateGrantAccessFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.grantAccess",
                             FILEMANAGERPRIVATE_GRANTACCESS)

 protected:
  ~FileManagerPrivateGrantAccessFunction() override = default;

 private:
  ExtensionFunction::ResponseAction Run() override;
};

// Base class for FileManagerPrivateInternalAddFileWatchFunction and
// FileManagerPrivateInternalRemoveFileWatchFunction. Although it's called
// "FileWatch",
// the class and its sub classes are used only for watching changes in
// directories.
class FileWatchFunctionBase : public LoggedExtensionFunction {
 public:
  using ResponseCallback = base::OnceCallback<void(bool success)>;

  // Calls Respond() with |success| converted to base::Value.
  void RespondWith(bool success);

 protected:
  ~FileWatchFunctionBase() override = default;

  // A virtual method to tell the base class if the function is addFileWatch().
  virtual bool IsAddWatch() = 0;

  // Performs a file watch operation (ex. adds or removes a file watch) on
  // the IO thread with storage::WatcherManager.
  virtual void PerformFileWatchOperationOnIOThread(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      storage::WatcherManager* watcher_manager,
      const storage::FileSystemURL& file_system_url,
      base::WeakPtr<file_manager::EventRouter> event_router) = 0;

  // Performs a file watch operation (ex. adds or removes a file watch) on
  // the UI thread with file_manager::EventRouter. This is a fallback operation
  // called only when WatcherManager is unavailable.
  virtual void PerformFallbackFileWatchOperationOnUIThread(
      const storage::FileSystemURL& file_system_url,
      base::WeakPtr<file_manager::EventRouter> event_router) = 0;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void RunAsyncOnIOThread(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      const storage::FileSystemURL& file_system_url,
      base::WeakPtr<file_manager::EventRouter> event_router);
};

// Implements the chrome.fileManagerPrivate.addFileWatch method.
// Starts watching changes in directories.
class FileManagerPrivateInternalAddFileWatchFunction
    : public FileWatchFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.addFileWatch",
                             FILEMANAGERPRIVATEINTERNAL_ADDFILEWATCH)

 protected:
  ~FileManagerPrivateInternalAddFileWatchFunction() override = default;

  // FileWatchFunctionBase override.
  void PerformFileWatchOperationOnIOThread(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      storage::WatcherManager* watcher_manager,
      const storage::FileSystemURL& file_system_url,
      base::WeakPtr<file_manager::EventRouter> event_router) override;
  void PerformFallbackFileWatchOperationOnUIThread(
      const storage::FileSystemURL& file_system_url,
      base::WeakPtr<file_manager::EventRouter> event_router) override;
  bool IsAddWatch() override;
};

// Implements the chrome.fileManagerPrivate.removeFileWatch method.
// Stops watching changes in directories.
class FileManagerPrivateInternalRemoveFileWatchFunction
    : public FileWatchFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.removeFileWatch",
                             FILEMANAGERPRIVATEINTERNAL_REMOVEFILEWATCH)

 protected:
  ~FileManagerPrivateInternalRemoveFileWatchFunction() override = default;

  // FileWatchFunctionBase override.
  void PerformFileWatchOperationOnIOThread(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      storage::WatcherManager* watcher_manager,
      const storage::FileSystemURL& file_system_url,
      base::WeakPtr<file_manager::EventRouter> event_router) override;
  void PerformFallbackFileWatchOperationOnUIThread(
      const storage::FileSystemURL& file_system_url,
      base::WeakPtr<file_manager::EventRouter> event_router) override;
  bool IsAddWatch() override;
};

// Implements the chrome.fileManagerPrivate.getSizeStats method.
class FileManagerPrivateGetSizeStatsFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getSizeStats",
                             FILEMANAGERPRIVATE_GETSIZESTATS)

 protected:
  ~FileManagerPrivateGetSizeStatsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnGetMtpAvailableSpace(device::mojom::MtpStorageInfoPtr mtp_storage_info,
                              const bool error);

  void OnGetDocumentsProviderAvailableSpace(const bool error,
                                            const uint64_t available_bytes,
                                            const uint64_t capacity_bytes);

  void OnGetDriveQuotaUsage(drive::FileError error,
                            drivefs::mojom::QuotaUsagePtr usage);

  void OnGetSizeStats(const uint64_t* total_size,
                      const uint64_t* remaining_size);
};

// Implements the chrome.fileManagerPrivateInternal.getDriveQuotaMetadata
// method.
class FileManagerPrivateInternalGetDriveQuotaMetadataFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getDriveQuotaMetadata",
                             FILEMANAGERPRIVATE_GETDRIVEQUOTAMETADATA)

 protected:
  ~FileManagerPrivateInternalGetDriveQuotaMetadataFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnGetPooledQuotaUsage(drive::FileError error,
                             drivefs::mojom::PooledQuotaUsagePtr usage);
  void OnGetMetadata(drive::FileError error,
                     drivefs::mojom::FileMetadataPtr metadata);

  storage::FileSystemURL file_system_url_;
  api::file_manager_private::DriveQuotaMetadata quotaMetadata_;
};

// Implements the chrome.fileManagerPrivate.validatePathNameLength method.
class FileManagerPrivateInternalValidatePathNameLengthFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.validatePathNameLength",
      FILEMANAGERPRIVATEINTERNAL_VALIDATEPATHNAMELENGTH)

 protected:
  ~FileManagerPrivateInternalValidatePathNameLengthFunction() override =
      default;

  void OnFilePathLimitRetrieved(size_t current_length, size_t max_length);

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.formatVolume method.
// Formats Volume given its mount path.
class FileManagerPrivateFormatVolumeFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.formatVolume",
                             FILEMANAGERPRIVATE_FORMATVOLUME)

 protected:
  ~FileManagerPrivateFormatVolumeFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.singlePartitionFormat method.
// Deletes removable device partitions, create a single partition and format.
class FileManagerPrivateSinglePartitionFormatFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.singlePartitionFormat",
                             FILEMANAGERPRIVATE_SINGLEPARTITIONFORMAT)

 protected:
  ~FileManagerPrivateSinglePartitionFormatFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.renameVolume method.
// Renames Volume given its mount path and new Volume name.
class FileManagerPrivateRenameVolumeFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.renameVolume",
                             FILEMANAGERPRIVATE_RENAMEVOLUME)

 protected:
  ~FileManagerPrivateRenameVolumeFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getDisallowedTransfers method.
class FileManagerPrivateInternalGetDisallowedTransfersFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetDisallowedTransfersFunction();

  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.getDisallowedTransfers",
      FILEMANAGERPRIVATEINTERNAL_GETDISALLOWEDTRANSFERS)

 protected:
  ~FileManagerPrivateInternalGetDisallowedTransfersFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnGetDisallowedFiles(
      std::vector<storage::FileSystemURL> disallowed_files);
  void OnConvertFileDefinitionListToEntryDefinitionList(
      std::unique_ptr<file_manager::util::EntryDefinitionList>
          entry_definition_list);

  raw_ptr<Profile> profile_ = nullptr;

  std::vector<storage::FileSystemURL> source_urls_;
  storage::FileSystemURL destination_url_;
};

// Implements the chrome.fileManagerPrivateInternal.getDlpMetadata method.
class FileManagerPrivateInternalGetDlpMetadataFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetDlpMetadataFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getDlpMetadata",
                             FILEMANAGERPRIVATEINTERNAL_GETDLPMETADATA)

 protected:
  ~FileManagerPrivateInternalGetDlpMetadataFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnGetDlpMetadata(
      std::vector<policy::DlpFilesControllerAsh::DlpFileMetadata> dlp_metadata);

  std::vector<storage::FileSystemURL> source_urls_;
};

// Implements the chrome.fileManagerPrivate.getDlpRestrictionDetails method.
class FileManagerPrivateGetDlpRestrictionDetailsFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateGetDlpRestrictionDetailsFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getDlpRestrictionDetails",
                             FILEMANAGERPRIVATE_GETDLPRESTRICTIONDETAILS)

 protected:
  ~FileManagerPrivateGetDlpRestrictionDetailsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getDlpBlockedComponents method.
class FileManagerPrivateGetDlpBlockedComponentsFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateGetDlpBlockedComponentsFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getDlpBlockedComponents",
                             FILEMANAGERPRIVATE_GETDLPBLOCKEDCOMPONENTS)

 protected:
  ~FileManagerPrivateGetDlpBlockedComponentsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getDialogCaller method.
class FileManagerPrivateGetDialogCallerFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getDialogCaller",
                             FILEMANAGERPRIVATE_GETDIALOGCALLER)

 protected:
  ~FileManagerPrivateGetDialogCallerFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivateInternal.resolveIsolatedEntries
// method.
class FileManagerPrivateInternalResolveIsolatedEntriesFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.resolveIsolatedEntries",
      FILEMANAGERPRIVATE_RESOLVEISOLATEDENTRIES)

 protected:
  ~FileManagerPrivateInternalResolveIsolatedEntriesFunction() override =
      default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList(
      std::unique_ptr<file_manager::util::EntryDefinitionList>
          entry_definition_list);
};

class FileManagerPrivateInternalSearchFilesFunction
    : public LoggedExtensionFunction {
 public:
  // The type for matched files. The second element of the pair indicates if the
  // path is that of a directory (true) or a plain file (false).
  using FileSearchResults = std::vector<std::pair<base::FilePath, bool>>;

  // A callback on which the results are to be delivered. The results are
  // expected to be delivered in a single invocation.
  using OnResultsReadyCallback = base::OnceCallback<void(FileSearchResults)>;

  FileManagerPrivateInternalSearchFilesFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.searchFiles",
                             FILEMANAGERPRIVATEINTERNAL_SEARCHFILES)

 protected:
  ~FileManagerPrivateInternalSearchFilesFunction() override = default;

 private:
  // ExtensionFunction overrides. The launch point of search by name
  // and search image by keywords.
  ResponseAction Run() override;

  // Runs the search files by file name task. Once done invokes the callback.
  // The root_path is the path to the top level directory which is to be
  // searched. Only results from this directory and nested directories are
  // accepted.
  void RunFileSearchByName(Profile* profile,
                           base::FilePath root_path,
                           const std::string& query,
                           base::Time modified_time,
                           ash::RecentSource::FileType file_type,
                           size_t max_results,
                           OnResultsReadyCallback callback);

  // Runs the search images by query task. Once done invokes the callback.
  // The root_path is the path to the top level directory which is to be
  // searched. Only results from this directory and nested directories are
  // accepted.
  void RunImageSearchByQuery(base::FilePath root_path,
                             const std::string& query,
                             base::Time modified_time,
                             size_t max_results,
                             OnResultsReadyCallback callback);

  void OnSearchByPatternDone(std::vector<FileSearchResults> results);
};

// Implements the chrome.fileManagerPrivate.getDirectorySize method.
class FileManagerPrivateInternalGetDirectorySizeFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getDirectorySize",
                             FILEMANAGERPRIVATEINTERNAL_GETDIRECTORYSIZE)

 protected:
  ~FileManagerPrivateInternalGetDirectorySizeFunction() override = default;

  void OnDirectorySizeRetrieved(int64_t size);

  // ExtensionFunction overrides
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.startIOTask method.
class FileManagerPrivateInternalStartIOTaskFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.startIOTask",
                             FILEMANAGERPRIVATEINTERNAL_STARTIOTASK)

 protected:
  ~FileManagerPrivateInternalStartIOTaskFunction() override = default;

  // ExtensionFunction overrides
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.cancelIOTask method.
class FileManagerPrivateCancelIOTaskFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.cancelIOTask",
                             FILEMANAGERPRIVATE_CANCELIOTASK)

 protected:
  ~FileManagerPrivateCancelIOTaskFunction() override = default;

  // ExtensionFunction overrides
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.resumeIOTask method.
class FileManagerPrivateResumeIOTaskFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.resumeIOTask",
                             FILEMANAGERPRIVATE_RESUMEIOTASK)

 protected:
  ~FileManagerPrivateResumeIOTaskFunction() override = default;

  // ExtensionFunction overrides
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.dismissIOTask method.
class FileManagerPrivateDismissIOTaskFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.dismissIOTask",
                             FILEMANAGERPRIVATE_DISMISSIOTASK)

 protected:
  ~FileManagerPrivateDismissIOTaskFunction() override = default;

  // ExtensionFunction overrides
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.progressPausedTasks method.
class FileManagerPrivateProgressPausedTasksFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.progressPausedTasks",
                             FILEMANAGERPRIVATE_PROGRESSPAUSEDTASKS)

 protected:
  ~FileManagerPrivateProgressPausedTasksFunction() override = default;

  // ExtensionFunction overrides
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.showPolicyDialog method.
class FileManagerPrivateShowPolicyDialogFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.showPolicyDialog",
                             FILEMANAGERPRIVATE_SHOWPOLICYDIALOG)

 protected:
  ~FileManagerPrivateShowPolicyDialogFunction() override = default;

  // ExtensionFunction overrides
  ResponseAction Run() override;
};

class FileManagerPrivateInternalParseTrashInfoFilesFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalParseTrashInfoFilesFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.parseTrashInfoFiles",
                             FILEMANAGERPRIVATEINTERNAL_PARSETRASHINFOFILES)

 protected:
  ~FileManagerPrivateInternalParseTrashInfoFilesFunction() override;

  // ExtensionFunction overrides
  ResponseAction Run() override;

 private:
  // Invoked after calling the `ValidateAndParseTrashInfoFile` with all the data
  // retrieved from the .trashinfo files. If any are error'd out they are logged
  // and ultimately discarded.
  void OnTrashInfoFilesParsed(
      std::vector<file_manager::trash::ParsedTrashInfoDataOrError> parsed_data);

  // After converting the restorePath (converted to Entry to ensure we can
  // perform a getMetadata on it to verify existence) zip the 2 `std::vector`'s
  // together to return back to the UI.
  void OnConvertFileDefinitionListToEntryDefinitionList(
      std::vector<file_manager::trash::ParsedTrashInfoData> parsed_data,
      std::unique_ptr<file_manager::util::EntryDefinitionList>
          entry_definition_list);

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // The TrashInfoValidator that maintains a connection to the TrashService
  // which performs the parsing.
  std::unique_ptr<file_manager::trash::TrashInfoValidator> validator_ = nullptr;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
