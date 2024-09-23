// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/file_system.mojom-forward.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/watcher_manager.h"

class GURL;

namespace arc {

// Represents a file system root in Android Documents Provider.
//
// All methods must be called on the UI thread.
// If this object is deleted while there are in-flight operations, callbacks
// for those operations will be never called.
class ArcDocumentsProviderRoot : public ArcFileSystemOperationRunner::Observer {
 public:
  struct ThinFileInfo {
    base::FilePath::StringType name;
    std::string document_id;
    bool is_directory;
    base::Time last_modified;
  };

  // Extra metadata in addition to the metadata provided in base::File::Info.
  struct ExtraFileMetadata {
    // True if a document is deletable.
    bool supports_delete;
    // True if a document can be renamed.
    bool supports_rename;
    // True if a document is a directory that supports creation of new files
    // within it.
    bool dir_supports_create;
    // True if a document will return a valid thumbnail from
    // the DocumentsProvider.openDocumentThumbnail() Android API call.
    bool supports_thumbnail;
    // Last modified time of the the file, returned in the COLUMN_LAST_MODIFIED
    // from the DocumentsProvider.queryDocument() and .queryChildDocuments(). If
    // unknown, it's set to the base::Time().
    base::Time last_modified;
    // Size of the file in bytes, returned in the COLUMN_SIZE from the
    // DocumentsProvider.queryDocument() and .queryChildDocuments(). If the
    // size unknown, it's set to -1.
    int64_t size;
  };

  // TODO(crbug.com/40535136): Use OnceCallback/RepeatingCallback.
  using GetFileInfoCallback = storage::AsyncFileUtil::GetFileInfoCallback;
  using StatusCallback = storage::AsyncFileUtil::StatusCallback;
  using ReadDirectoryCallback =
      base::OnceCallback<void(base::File::Error error,
                              std::vector<ThinFileInfo> files)>;
  using ChangeType = storage::WatcherManager::ChangeType;
  using WatcherNotificationCallback =
      storage::WatcherManager::NotificationCallback;
  using WatcherStatusCallback = storage::WatcherManager::StatusCallback;
  using ResolveToContentUrlCallback =
      base::OnceCallback<void(const GURL& content_url)>;
  using GetExtraMetadataCallback =
      base::OnceCallback<void(base::File::Error error,
                              const ExtraFileMetadata& metadata)>;
  using GetRootSizeCallback =
      base::OnceCallback<void(const bool error,
                              const uint64_t available_bytes,
                              const uint64_t capacity_bytes)>;

  ArcDocumentsProviderRoot(ArcFileSystemOperationRunner* runner,
                           const std::string& authority,
                           const std::string& root_document_id,
                           const std::string& root_id,
                           bool read_only,
                           const std::vector<std::string>& mime_types);

  ArcDocumentsProviderRoot(const ArcDocumentsProviderRoot&) = delete;
  ArcDocumentsProviderRoot& operator=(const ArcDocumentsProviderRoot&) = delete;

  ~ArcDocumentsProviderRoot() override;

  // Queries information of a file just like AsyncFileUtil.GetFileInfo(). If the
  // file metadata reports unknown size, it will attempt to open the file and
  // read the size from the file descriptor.
  void GetFileInfo(const base::FilePath& path,
                   storage::FileSystemOperation::GetMetadataFieldSet fields,
                   GetFileInfoCallback callback);

  // Queries a list of files under a directory just like
  // AsyncFileUtil.ReadDirectory().
  void ReadDirectory(const base::FilePath& path,
                     ReadDirectoryCallback callback);

  // Deletes a file/directory at the given path.
  //
  // - File::FILE_ERROR_NOT_FOUND if |path| does not exist.
  // - File::FILE_ERROR_ACCESS_DENIED if this root is read-only.
  void DeleteFile(const base::FilePath& path, StatusCallback callback);

  // Creates a file at the given path.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |path|'s parent directory does not exist.
  // - File::FILE_ERROR_EXISTS if a file already exists at |path|.
  // - File::FILE_ERROR_ACCESS_DENIED if this root is read-only.
  void CreateFile(const base::FilePath& path, StatusCallback callback);

  // Creates a directory at the given path.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |path|'s parent directory does not exist.
  // - File::FILE_ERROR_EXISTS if a file already exists at |path|.
  // - File::FILE_ERROR_ACCESS_DENIED if this root is read-only.
  void CreateDirectory(const base::FilePath& path, StatusCallback callback);

  // Copies a file from |src_path| to |dest_path| inside this root.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |src_path| or the parent directory of
  //   |dest_path| does not exist.
  // - File::FILE_ERROR_ACCESS_DENIED if this root is read-only.
  void CopyFileLocal(const base::FilePath& src_path,
                     const base::FilePath& dest_path,
                     StatusCallback callback);

  // Moves a file from |src_path| to |dest_path| inside this root.
  //
  // This reports following error code via |callback|:
  // - File::FILE_ERROR_NOT_FOUND if |src_path| or the parent directory of
  //   |dest_path| does not exist.
  // - File::FILE_ERROR_ACCESS_DENIED if this root is read-only.
  void MoveFileLocal(const base::FilePath& src_path,
                     const base::FilePath& dest_path,
                     StatusCallback callback);

  // Installs a document watcher.
  //
  // It is not allowed to install multiple watchers at the same file path;
  // if attempted, duplicated requests will fail.
  //
  // Currently, watchers can be installed only on directories, and only
  // directory content changes are notified. The result of installing a watcher
  // to a non-directory in unspecified.
  //
  // NOTES ABOUT CORRECTNESS AND CONSISTENCY:
  //
  // Document watchers are not always correct and they may miss some updates or
  // even notify incorrect update events for several reasons, such as:
  //
  //   - Directory moves: Currently a watcher will misbehave if the watched
  //     directory is moved to another location.
  //     TODO(fukino): Handle the case when a watched directory is moved or
  //     renamed.
  //   - Duplicated file name handling in this class: The same reason as
  //     directory moves. This may happen even with MediaDocumentsProvider,
  //     but the chance will not be very high.
  //   - File system operation races: For example, an watcher can be installed
  //     to a non-directory in a race condition.
  //   - Broken DocumentsProviders: For example, we get no notification if a
  //     document provider does not call setNotificationUri().
  //
  // However, consistency of installed watchers is guaranteed. That is, after
  // a watcher is installed on a file path X, an attempt to uninstall a watcher
  // at X will always succeed.
  //
  // Unfortunately it is too difficult (or maybe theoretically impossible) to
  // implement a perfect Android document watcher which never misses document
  // updates. So the current implementation gives up correctness, but instead
  // focuses on following two goals:
  //
  //   1. Keep the implementation simple, rather than trying hard to catch
  //      race conditions or minor cases. Even if we return wrong results, the
  //      worst consequence is just that users do not see the latest contents
  //      until they refresh UI.
  //
  //   2. Keep consistency of installed watchers so that the caller can avoid
  //      dangling watchers.
  void AddWatcher(const base::FilePath& path,
                  WatcherNotificationCallback watcher_callback,
                  WatcherStatusCallback callback);

  // Uninstalls a document watcher.
  // See the documentation of AddWatcher() above.
  void RemoveWatcher(const base::FilePath& path,
                     WatcherStatusCallback callback);

  // Resolves a file path into a content:// URL pointing to the file
  // on DocumentsProvider. Returns URL that can be passed to
  // ArcContentFileSystemFileSystemReader to read the content.
  // On errors, an invalid GURL is returned.
  void ResolveToContentUrl(const base::FilePath& path,
                           ResolveToContentUrlCallback callback);

  // Get extra metadata of the file at |path|.
  void GetExtraFileMetadata(const base::FilePath& path,
                            GetExtraMetadataCallback callback);

  // Instructs to make directory caches expire "soon" after callbacks are
  // called, that is, when the message loop gets idle.
  void SetDirectoryCacheExpireSoonForTesting();

  // ArcFileSystemOperationRunner::Observer overrides:
  void OnWatchersCleared() override;

  // Get DocumentsProvider root's available bytes.
  void GetRootSize(GetRootSizeCallback callback);

 private:
  friend class ArcDocumentsProviderRootMapTest;
  FRIEND_TEST_ALL_PREFIXES(ArcDocumentsProviderRootMapTest, Lookup);

  struct WatcherData;
  struct DirectoryCache;

  static const int64_t kInvalidWatcherId;
  static const uint64_t kInvalidWatcherRequestId;
  static const WatcherData kInvalidWatcherData;

  // Mapping from a file name to a Document.
  using NameToDocumentMap =
      std::map<base::FilePath::StringType, mojom::DocumentPtr>;

  using ResolveToDocumentIdCallback =
      base::OnceCallback<void(const std::string& document_id)>;
  using ReadDirectoryInternalCallback =
      base::OnceCallback<void(base::File::Error error,
                              const NameToDocumentMap& mapping)>;
  using GetDocumentCallback =
      base::OnceCallback<void(base::File::Error error,
                              const mojom::DocumentPtr& document)>;

  void OnGetRootSize(GetRootSizeCallback callback,
                     mojom::RootSizePtr maybe_root_size);

  void GetFileInfoFromDocument(
      GetFileInfoCallback callback,
      const base::FilePath& path,
      storage::FileSystemOperation::GetMetadataFieldSet fields,
      base::File::Error error,
      const mojom::DocumentPtr& document);

  void ReadDirectoryWithDocumentId(ReadDirectoryCallback callback,
                                   const std::string& document_id);
  void ReadDirectoryWithNameToDocumentMap(ReadDirectoryCallback callback,
                                          base::File::Error error,
                                          const NameToDocumentMap& mapping);

  void DeleteFileWithDocumentId(StatusCallback callback,
                                const base::FilePath& path,
                                const std::string& document_id);
  void DeleteFileWithParentDocumentId(StatusCallback callback,
                                      const std::string& document_id,
                                      const std::string& parent_document_id);
  void OnFileDeleted(StatusCallback callback,
                     const std::string& parent_document_id,
                     bool success);

  void CreateFileAfterConflictCheck(StatusCallback callback,
                                    const base::FilePath& path,
                                    const std::string& document_id);
  void CreateFileWithParentDocumentId(StatusCallback callback,
                                      const base::FilePath& basename,
                                      const std::string& parent_document_id);

  void CreateDirectoryAfterConflictCheck(StatusCallback callback,
                                         const base::FilePath& path,
                                         const std::string& document_id);
  void CreateDirectoryWithParentDocumentId(
      StatusCallback callback,
      const base::FilePath& basename,
      const std::string& parent_document_id);
  void OnFileCreated(StatusCallback callback,
                     const std::string& parent_document_id,
                     mojom::DocumentPtr maybe_document);

  void RenameFileInternal(const base::FilePath& path,
                          const std::string& display_name,
                          StatusCallback callback);
  void RenameFileWithDocumentId(StatusCallback callback,
                                const base::FilePath& path,
                                const std::string& display_name,
                                const std::string& document_id);
  void RenameFileWithParentDocumentId(StatusCallback callback,
                                      const std::string& display_name,
                                      const std::string& document_id,
                                      const std::string& parent_document_id);
  void OnFileRenamed(StatusCallback callback,
                     const std::string& parent_document_id,
                     mojom::DocumentPtr document);

  void CopyFileWithSourceDocumentId(StatusCallback callback,
                                    const base::FilePath& target_path,
                                    const std::string& source_display_name,
                                    const std::string& source_document_id);
  void CopyFileWithTargetParentDocumentId(
      StatusCallback callback,
      const std::string& source_document_id,
      const std::string& target_display_name_to_rename,
      const std::string& target_parent_document_id);
  void OnFileCopied(StatusCallback callback,
                    const std::string& target_display_name_to_rename,
                    const std::string& target_parent_document_id,
                    mojom::DocumentPtr document);

  void MoveFileInternal(const base::FilePath& source_path,
                        const base::FilePath& target_path,
                        StatusCallback callback);
  void MoveFileWithSourceDocumentId(StatusCallback callback,
                                    const base::FilePath& source_parent_path,
                                    const base::FilePath& target_path,
                                    const std::string& source_display_name,
                                    const std::string& source_document_id);
  void MoveFileWithSourceParentDocumentId(
      StatusCallback callback,
      const std::string& source_document_id,
      const base::FilePath& target_path,
      const std::string& source_display_name,
      const std::string& source_parent_document_id);
  void MoveFileWithTargetParentDocumentId(
      StatusCallback callback,
      const std::string& source_document_id,
      const std::string& source_parent_document_id,
      const std::string& target_display_name_to_rename,
      const std::string& target_parent_document_id);
  void OnFileMoved(StatusCallback callback,
                   const std::string& target_display_name_to_rename,
                   const std::string& source_parent_document_id,
                   const std::string& target_parent_document_id,
                   mojom::DocumentPtr document);

  void AddWatcherWithDocumentId(const base::FilePath& path,
                                uint64_t watcher_request_id,
                                WatcherNotificationCallback watcher_callback,
                                const std::string& document_id);
  void OnWatcherAdded(const base::FilePath& path,
                      uint64_t watcher_request_id,
                      int64_t watcher_id);
  void OnWatcherAddedButRemoved(bool success);

  void OnWatcherRemoved(WatcherStatusCallback callback, bool success);

  // Returns true if the specified watcher request has been canceled.
  // This function should be called only while the request is in-flight.
  bool IsWatcherInflightRequestCanceled(const base::FilePath& path,
                                        uint64_t watcher_request_id) const;

  void ResolveToContentUrlWithDocumentId(ResolveToContentUrlCallback callback,
                                         const std::string& document_id);

  void GetExtraMetadataFromDocument(GetExtraMetadataCallback callback,
                                    base::File::Error error,
                                    const mojom::DocumentPtr& document);

  // Queries for a single document at the given |path|, using a directory cache,
  // if present.
  void GetDocument(const base::FilePath& path, GetDocumentCallback callback);
  void GetDocumentWithParentDocumentId(GetDocumentCallback callback,
                                       const base::FilePath& basename,
                                       const std::string& parent_document_id);
  void GetDocumentWithNameToDocumentMap(GetDocumentCallback callback,
                                        const base::FilePath& basename,
                                        base::File::Error error,
                                        const NameToDocumentMap& mapping);
  // Resolves |path| to a document ID. Failures are indicated by an empty
  // document ID.
  void ResolveToDocumentId(const base::FilePath& path,
                           ResolveToDocumentIdCallback callback);
  void ResolveToDocumentIdRecursively(
      const std::string& document_id,
      const std::vector<base::FilePath::StringType>& components,
      ResolveToDocumentIdCallback callback);
  void ResolveToDocumentIdRecursivelyWithNameToDocumentMap(
      const std::vector<base::FilePath::StringType>& components,
      ResolveToDocumentIdCallback callback,
      base::File::Error error,
      const NameToDocumentMap& mapping);

  // Enumerates child documents of a directory specified by |document_id|.
  // If |force_refresh| is true, the backend is queried even if there is a
  // directory cache.
  // The result is returned as a NameToDocumentMap. It is valid only within
  // the callback and might get deleted immediately after the callback
  // returns.
  void ReadDirectoryInternal(const std::string& document_id,
                             bool force_refresh,
                             ReadDirectoryInternalCallback callback);
  void ReadDirectoryInternalWithChildDocuments(
      const std::string& document_id,
      std::optional<std::vector<mojom::DocumentPtr>> maybe_children);

  // Clears a directory cache.
  void ClearDirectoryCache(const std::string& document_id);

  // |runner_| outlives this object. ArcDocumentsProviderRootMap, the owner of
  // this object, depends on ArcFileSystemOperationRunner in the
  // BrowserContextKeyedServiceFactory dependency graph.
  const raw_ptr<ArcFileSystemOperationRunner> runner_;

  const std::string authority_;
  const std::string root_document_id_;
  const std::string root_id_;
  const bool read_only_;
  const std::vector<std::string> mime_types_;

  bool directory_cache_expire_soon_ = false;

  // Cache of directory contents. Keys are document IDs of directories.
  std::map<std::string, DirectoryCache> directory_cache_;

  // Map from a document ID to callbacks pending for ReadDirectoryInternal()
  // calls.
  std::map<std::string, std::vector<ReadDirectoryInternalCallback>>
      pending_callbacks_map_;

  // Map from a file path to a watcher data.
  //
  // Note that we do not use a document ID as a key here to guarantee that
  // a watch installed by AddWatcher() can be always identified in
  // RemoveWatcher() with the same file path specified.
  // See the documentation of AddWatcher() for more details.
  std::map<base::FilePath, WatcherData> path_to_watcher_data_;

  uint64_t next_watcher_request_id_ = 1;

  base::WeakPtrFactory<ArcDocumentsProviderRoot> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_
