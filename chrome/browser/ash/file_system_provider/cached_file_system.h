// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CACHED_FILE_SYSTEM_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CACHED_FILE_SYSTEM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_system_provider/abort_callback.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"
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
}  // namespace base

class GURL;
class OperationRequestManager;

namespace ash::file_system_provider {

// A simple wrapper over a `ProvidedFileSystem` that adds additional logging,
// currently this is hidden behind the `FileSystemProviderContentCache` feature
// flag.
class CachedFileSystem : public ProvidedFileSystemInterface {
 public:
  CachedFileSystem(std::unique_ptr<ProvidedFileSystemInterface> file_system,
                   ContentCache* content_cache);

  CachedFileSystem(const CachedFileSystem&) = delete;
  CachedFileSystem& operator=(const CachedFileSystem&) = delete;

  ~CachedFileSystem() override;

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

 private:
  const std::string GetFileSystemId() const;
  std::unique_ptr<ProvidedFileSystemInterface> file_system_;
  raw_ptr<ContentCache> content_cache_;  // Not owned.

  base::WeakPtrFactory<CachedFileSystem> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CACHED_FILE_SYSTEM_H_
