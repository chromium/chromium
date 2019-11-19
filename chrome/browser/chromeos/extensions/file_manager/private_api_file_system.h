// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides file system related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "components/drive/file_errors.h"
#include "extensions/browser/extension_function.h"
#include "services/device/public/mojom/mtp_storage_info.mojom.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {
class FileSystemContext;
class FileSystemURL;
class WatcherManager;
}  // namespace storage

namespace file_manager {
class EventRouter;
namespace util {
struct EntryDefinition;
typedef std::vector<EntryDefinition> EntryDefinitionList;
}  // namespace util
}  // namespace file_manager

namespace drive {
namespace util {
class FileStreamMd5Digester;
}  // namespace util

// File path and its MD5 hash obtained from drive.
struct HashAndFilePath {
  std::string hash;
  base::FilePath path;
};

}  // namespace drive

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

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.grantAccess",
                             FILEMANAGERPRIVATE_GRANTACCESS)

 protected:
  ~FileManagerPrivateGrantAccessFunction() override = default;

 private:
  ExtensionFunction::ResponseAction Run() override;
  const ChromeExtensionFunctionDetails chrome_details_;
  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateGrantAccessFunction);
};

// Base class for FileManagerPrivateInternalAddFileWatchFunction and
// FileManagerPrivateInternalRemoveFileWatchFunction. Although it's called
// "FileWatch",
// the class and its sub classes are used only for watching changes in
// directories.
class FileWatchFunctionBase : public LoggedExtensionFunction {
 public:
  using ResponseCallback = base::Callback<void(bool success)>;

  // Calls Respond() with |success| converted to base::Value.
  void RespondWith(bool success);

 protected:
  ~FileWatchFunctionBase() override = default;

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
  void OnGetDriveAvailableSpace(drive::FileError error,
                                int64_t bytes_total,
                                int64_t bytes_used);

  void OnGetMtpAvailableSpace(device::mojom::MtpStorageInfoPtr mtp_storage_info,
                              const bool error);

  void OnGetSizeStats(const uint64_t* total_size,
                      const uint64_t* remaining_size);
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

// Implements the chrome.fileManagerPrivate.startCopy method.
class FileManagerPrivateInternalStartCopyFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalStartCopyFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.startCopy",
                             FILEMANAGERPRIVATEINTERNAL_STARTCOPY)

 protected:
  ~FileManagerPrivateInternalStartCopyFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void RunAfterGetFileMetadata(base::File::Error result,
                               const base::File::Info& file_info);

  // Part of RunAsync(). Called after the amount of space on the destination
  // is known.
  void RunAfterCheckDiskSpace(int64_t space_needed,
                              const std::vector<int64_t>& spaces_available);

  // Part of RunAsync(). Called after FreeDiskSpaceIfNeededFor() is completed on
  // IO thread.
  void RunAfterFreeDiskSpace(bool available);

  // Part of RunAsync(). Called after Copy() is started on IO thread.
  void RunAfterStartCopy(int operation_id);

  storage::FileSystemURL source_url_;
  storage::FileSystemURL destination_url_;
  const ChromeExtensionFunctionDetails chrome_details_;
};

// Implements the chrome.fileManagerPrivate.cancelCopy method.
class FileManagerPrivateCancelCopyFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.cancelCopy",
                             FILEMANAGERPRIVATE_CANCELCOPY)

 protected:
  ~FileManagerPrivateCancelCopyFunction() override = default;

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

class FileManagerPrivateInternalComputeChecksumFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalComputeChecksumFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.computeChecksum",
                             FILEMANAGERPRIVATEINTERNAL_COMPUTECHECKSUM)

 protected:
  ~FileManagerPrivateInternalComputeChecksumFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  std::unique_ptr<drive::util::FileStreamMd5Digester> digester_;

  void RespondWith(std::string hash);
};

// Implements the chrome.fileManagerPrivate.searchFilesByHashes method.
class FileManagerPrivateSearchFilesByHashesFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateSearchFilesByHashesFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.searchFilesByHashes",
                             FILEMANAGERPRIVATE_SEARCHFILESBYHASHES)

 protected:
  ~FileManagerPrivateSearchFilesByHashesFunction() override = default;

 private:
  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // Fallback to walking the filesystem and checking file attributes.
  std::vector<drive::HashAndFilePath> SearchByAttribute(
      const std::set<std::string>& hashes,
      const base::FilePath& dir,
      const base::FilePath& prefix);
  void OnSearchByAttribute(const std::set<std::string>& hashes,
                           const std::vector<drive::HashAndFilePath>& results);

  // Sends a response with |results| to the extension.
  void OnSearchByHashes(const std::set<std::string>& hashes,
                        drive::FileError error,
                        const std::vector<drive::HashAndFilePath>& results);

  const ChromeExtensionFunctionDetails chrome_details_;
};

class FileManagerPrivateSearchFilesFunction : public LoggedExtensionFunction {
 public:
  FileManagerPrivateSearchFilesFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.searchFiles",
                             FILEMANAGERPRIVATE_SEARCHFILES)

 protected:
  ~FileManagerPrivateSearchFilesFunction() override = default;

 private:
  // ExtensionFunction overrides.
  ResponseAction Run() override;

  void OnSearchByPattern(
      const std::vector<std::pair<base::FilePath, bool>>& results);

  const ChromeExtensionFunctionDetails chrome_details_;
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

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_FILE_SYSTEM_H_
