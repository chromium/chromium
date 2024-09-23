// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FAKE_PROVIDED_FILE_SYSTEM_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FAKE_PROVIDED_FILE_SYSTEM_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ash/file_system_provider/abort_callback.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_observer.h"
#include "chrome/browser/ash/file_system_provider/watcher.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/watcher_manager.h"
#include "url/gurl.h"

namespace base {
class Time;
}  // namespace base

namespace net {
class IOBuffer;
}  // namespace net

namespace ash::file_system_provider {

class OperationRequestManager;

// Path of a sample fake file, which is added to the fake file system by
// default.
extern const base::FilePath::CharType kFakeFilePath[];
extern const char kFakeFileText[];
extern const char kFakeFileVersionTag[];

// Represents a file or a directory on a fake file system.
struct FakeEntry {
  FakeEntry();
  FakeEntry(std::unique_ptr<EntryMetadata> metadata,
            const std::string& contents);

  FakeEntry(const FakeEntry&) = delete;
  FakeEntry& operator=(const FakeEntry&) = delete;

  ~FakeEntry();

  std::unique_ptr<EntryMetadata> metadata;
  std::string contents;
  // Used when the file is open for writing.
  std::optional<std::string> write_buffer;
};

// Fake provided file system implementation. Does not communicate with target
// extensions. Used for unit tests.
class FakeProvidedFileSystem : public ProvidedFileSystemInterface {
 public:
  explicit FakeProvidedFileSystem(
      const ProvidedFileSystemInfo& file_system_info);

  FakeProvidedFileSystem(const FakeProvidedFileSystem&) = delete;
  FakeProvidedFileSystem& operator=(const FakeProvidedFileSystem&) = delete;

  ~FakeProvidedFileSystem() override;

  // Adds a fake entry to the fake file system.
  void AddEntry(const base::FilePath& entry_path,
                bool is_directory,
                const std::string& name,
                int64_t size,
                base::Time modification_time,
                std::string mime_type,
                std::unique_ptr<CloudFileInfo> cloud_file_info,
                std::string contents);

  // Fetches a pointer to a fake entry registered in the fake file system. If
  // not found, then returns NULL. The returned pointes is owned by
  // FakeProvidedFileSystem.
  FakeEntry* GetEntry(const base::FilePath& entry_path) const;

  // Copies or moves an entry from `source_path` to `target_path`.
  base::File::Error CopyOrMoveEntry(const base::FilePath& source_path,
                                    const base::FilePath& target_path,
                                    bool is_move);

  void SetFlushRequired(bool required) { flush_required_ = required; }

  // ProvidedFileSystemInterface overrides.
  AbortCallback RequestUnmount(
      storage::AsyncFileUtil::StatusCallback callback) override;
  AbortCallback GetMetadata(
      const base::FilePath& entry_path,
      ProvidedFileSystemInterface::MetadataFieldMask fields,
      ProvidedFileSystemInterface::GetMetadataCallback callback) override;
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

  base::WeakPtr<FakeProvidedFileSystem> GetFakeWeakPtr();

 private:
  using Entries = std::map<base::FilePath, std::unique_ptr<FakeEntry>>;

  base::File::Error DoDeleteEntry(const base::FilePath& entry_path,
                                  bool recursive);

  // Utility function for posting a task which can be aborted by calling the
  // returned callback.
  AbortCallback PostAbortableTask(base::OnceClosure callback);

  // Aborts a request. |task_id| refers to a posted callback returning a
  // response for the operation, which will be cancelled, hence not called.
  void Abort(int task_id);

  // Aborts a request. |task_ids| refers to a vector of posted callbacks
  // returning a response for the operation, which will be cancelled, hence not
  // called.
  void AbortMany(const std::vector<int>& task_ids);

  ProvidedFileSystemInfo file_system_info_;
  Entries entries_;
  OpenedFiles opened_files_;
  int last_file_handle_;
  base::CancelableTaskTracker tracker_;
  base::ObserverList<ProvidedFileSystemObserver>::Unchecked observers_;
  Watchers watchers_;
  bool flush_required_ = false;

  base::WeakPtrFactory<FakeProvidedFileSystem> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FAKE_PROVIDED_FILE_SYSTEM_H_
