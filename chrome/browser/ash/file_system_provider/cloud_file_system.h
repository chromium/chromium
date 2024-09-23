// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CLOUD_FILE_SYSTEM_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CLOUD_FILE_SYSTEM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_error_or.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_system_provider/abort_callback.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"
#include "chrome/browser/ash/file_system_provider/opened_cloud_file.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_observer.h"
#include "storage/browser/file_system/async_file_util.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace base {
class FilePath;
class MetronomeTimer;
}  // namespace base

class GURL;
class OperationRequestManager;

namespace ash::file_system_provider {

// A simple wrapper over a `ProvidedFileSystem` that adds additional logging,
// currently this is hidden behind the `FileSystemProviderCloudFileSystem`
// feature flag.
class CloudFileSystem : public ProvidedFileSystemInterface,
                        public ContentCache::Observer {
 public:
  explicit CloudFileSystem(
      std::unique_ptr<ProvidedFileSystemInterface> file_system);

  CloudFileSystem(std::unique_ptr<ProvidedFileSystemInterface> file_system,
                  CacheManager* cache_manager);

  CloudFileSystem(const CloudFileSystem&) = delete;
  CloudFileSystem& operator=(const CloudFileSystem&) = delete;

  ~CloudFileSystem() override;

  // ProvidedFileSystemInterface
  AbortCallback RequestUnmount(
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback GetMetadata(const base::FilePath& entry_path,
                            MetadataFieldMask fields,
                            GetMetadataCallback callback) override;
  AbortCallback GetActions(const std::vector<base::FilePath>& entry_paths,
                           GetActionsCallback callback) override;
  AbortCallback ExecuteAction(
      const std::vector<base::FilePath>& entry_paths,
      const std::string& action_id,
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback ReadDirectory(
      const base::FilePath& directory_path,
      storage::AsyncFileUtil::ReadDirectoryCallback callback) override;
  AbortCallback OpenFile(const base::FilePath& file_path,
                         OpenFileMode mode,
                         OpenFileCallback callback) override;
  AbortCallback CloseFile(
      int file_handle,
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback ReadFile(int file_handle,
                         net::IOBuffer* buffer,
                         int64_t offset,
                         int length,
                         ReadChunkReceivedCallback callback) override;
  AbortCallback CreateDirectory(
      const base::FilePath& directory_path,
      bool recursive,
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback DeleteEntry(
      const base::FilePath& entry_path,
      bool recursive,
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback CreateFile(
      const base::FilePath& file_path,
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback CopyEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback MoveEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback Truncate(
      const base::FilePath& file_path,
      int64_t length,
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback WriteFile(
      int file_handle,
      net::IOBuffer* buffer,
      int64_t offset,
      int length,
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback FlushFile(
      int file_handle,
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback AddWatcher(const GURL& origin,
                           const base::FilePath& entry_path,
                           bool recursive,
                           bool persistent,
                           storage::AsyncFileUtil::StatusCallback callback,
                           storage::WatcherManager::NotificationCallback
                               notification_callback) override;
  void RemoveWatcher(const GURL& origin,
                     const base::FilePath& entry_path,
                     bool recursive,
                     storage::AsyncFileUtil::StatusCallback callback) override;
  const ProvidedFileSystemInfo& GetFileSystemInfo() const override;
  OperationRequestManager* GetRequestManager() override;
  Watchers* GetWatchers() override;
  const OpenedFiles& GetOpenedFiles() const override;
  void AddObserver(ProvidedFileSystemObserver* observer) override;
  void RemoveObserver(ProvidedFileSystemObserver* observer) override;
  void Notify(const base::FilePath& entry_path,
              bool recursive,
              storage::WatcherManager::ChangeType change_type,
              std::unique_ptr<ProvidedFileSystemObserver::Changes> changes,
              const std::string& tag,
              storage::AsyncFileUtil::StatusCallback callback) override;
  void Configure(storage::AsyncFileUtil::StatusCallback callback) override;
  base::WeakPtr<ProvidedFileSystemInterface> GetWeakPtr() override;
  std::unique_ptr<ScopedUserInteraction> StartUserInteraction() override;

  // ContentCache::Observer
  void OnItemEvicted(const base::FilePath& fsp_path) override;

 private:
  const std::string GetFileSystemId() const;
  void OnTimer();
  void OnContentCacheInitialized(
      base::FileErrorOr<std::unique_ptr<ContentCache>> error_or_cache);
  // Attempts to add a watcher on the file with `file_path`. If the attempt
  // fails with `FILE_ERROR_SECURITY`, this could be because the FSP (extension)
  // is not ready yet to handle FSP calls. Try again every 2 seconds until the
  // max number of attempts is reached.
  void AddWatcherOnCachedFile(const base::FilePath& file_path);
  void AddWatcherOnCachedFileImpl(const base::FilePath& file_path,
                                  int attempts,
                                  base::File::Error result);
  void OnItemEvictedFromCache(const base::FilePath& file_path);
  // Called when opening a file is completed with either a success or an error.
  void OnOpenFileCompleted(const base::FilePath& file_path,
                           OpenFileMode mode,
                           OpenFileCallback callback,
                           int file_handle,
                           base::File::Error result,
                           std::unique_ptr<EntryMetadata> metadata);
  // Called when closing a file is completed with either a success or an error.
  void OnCloseFileCompleted(int file_handle,
                            storage::AsyncFileUtil::StatusCallback callback,
                            base::File::Error result);

  // Called when the get metadata request is completed with either a success or
  // an error.
  void OnGetMetadataCompleted(const base::FilePath& entry_path,
                              GetMetadataCallback callback,
                              std::unique_ptr<EntryMetadata> entry_metadata,
                              base::File::Error result);

  // When an attempt to read the file from disk completes, in the event it fails
  // ensure it gets delegated to the underlying FSP.
  void OnReadFileFromCacheCompleted(int file_handle,
                                    scoped_refptr<net::IOBuffer> buffer,
                                    int64_t offset,
                                    int length,
                                    ReadChunkReceivedCallback callback,
                                    int bytes_read,
                                    bool has_more,
                                    base::File::Error result);

  // When a `ReadFile` completes, attempt to cache the bytes on disk.
  void OnReadFileCompleted(int file_handle,
                           scoped_refptr<net::IOBuffer> buffer,
                           int64_t offset,
                           int length,
                           ReadChunkReceivedCallback callback,
                           int bytes_read,
                           bool has_more,
                           base::File::Error result);

  // Called when the write file request is completed with either a success or
  // an error.
  void OnWriteFileCompleted(int file_handle,
                            storage::AsyncFileUtil::StatusCallback callback,
                            base::File::Error result);

  // Called when the delete entry request is completed with either a success or
  // an error.
  void OnDeleteEntryCompleted(const base::FilePath& entry_path,
                              storage::AsyncFileUtil::StatusCallback callback,
                              base::File::Error result);

  // After the bytes have finished caching, invoke the `callback`. This is the
  // `ReadChunkReceivedCallback` above and will always be invoked as the FSP
  // successfully read the file, if the content cache fails to write the file to
  // disk this should not stop further FSP requests.
  void OnBytesWrittenToCache(
      const base::FilePath& file_path,
      base::RepeatingCallback<void()> readchunk_success_callback,
      base::File::Error result);

  // Verify that all invariants are satisfied to make an optimistic cache read.
  using OpenedCloudFileMap = std::map<int, OpenedCloudFile>;
  bool ShouldAttemptToServeReadFileFromCache(
      const OpenedCloudFileMap::const_iterator it);

  std::unique_ptr<ProvidedFileSystemInterface> file_system_;
  std::unique_ptr<ContentCache> content_cache_;

  base::MetronomeTimer timer_;
  int file_manager_watchers_ = 0;
  // File handle -> OpenedCloudFile.
  OpenedCloudFileMap opened_files_;

  base::WeakPtrFactory<CloudFileSystem> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CLOUD_FILE_SYSTEM_H_
