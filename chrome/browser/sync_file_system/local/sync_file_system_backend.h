// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_FILE_SYSTEM_BACKEND_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_FILE_SYSTEM_BACKEND_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"

class Profile;

namespace sync_file_system {

class LocalFileChangeTracker;
class LocalFileSyncContext;

class SyncFileSystemBackend : public storage::FileSystemBackend {
 public:
  explicit SyncFileSystemBackend(Profile* profile);

  SyncFileSystemBackend(const SyncFileSystemBackend&) = delete;
  SyncFileSystemBackend& operator=(const SyncFileSystemBackend&) = delete;

  ~SyncFileSystemBackend() override;

  static std::unique_ptr<SyncFileSystemBackend> CreateForTesting();

  // FileSystemBackend overrides.
  bool CanHandleType(storage::FileSystemType type) const override;
  void Initialize(storage::FileSystemContext* context) override;
  void ResolveURL(const storage::FileSystemURL& url,
                  storage::OpenFileSystemMode mode,
                  ResolveURLCallback callback) override;
  storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) override;
  storage::WatcherManager* GetWatcherManager(
      storage::FileSystemType type) override;
  storage::CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      storage::FileSystemType type,
      base::File::Error* error_code) override;
  std::unique_ptr<storage::FileSystemOperation> CreateFileSystemOperation(
      storage::OperationType type,
      const storage::FileSystemURL& url,
      storage::FileSystemContext* context,
      base::File::Error* error_code) const override;
  bool SupportsStreaming(const storage::FileSystemURL& url) const override;
  bool HasInplaceCopyImplementation(
      storage::FileSystemType type) const override;
  std::unique_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access) const override;
  std::unique_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64_t offset,
      storage::FileSystemContext* context) const override;
  storage::FileSystemQuotaUtil* GetQuotaUtil() override;
  const storage::UpdateObserverList* GetUpdateObservers(
      storage::FileSystemType type) const override;
  const storage::ChangeObserverList* GetChangeObservers(
      storage::FileSystemType type) const override;
  const storage::AccessObserverList* GetAccessObservers(
      storage::FileSystemType type) const override;

  static SyncFileSystemBackend* GetBackend(
      const storage::FileSystemContext* context);

  LocalFileChangeTracker* change_tracker() { return change_tracker_.get(); }
  void SetLocalFileChangeTracker(
      std::unique_ptr<LocalFileChangeTracker> tracker);

  LocalFileSyncContext* sync_context() { return sync_context_.get(); }
  void set_sync_context(LocalFileSyncContext* sync_context);

 private:
  // Not owned.
  raw_ptr<storage::FileSystemContext, DanglingUntriaged> context_ = nullptr;

  std::unique_ptr<LocalFileChangeTracker> change_tracker_;
  scoped_refptr<LocalFileSyncContext> sync_context_;

  // |profile_| will initially be valid but may be destroyed before |this|, so
  // it should be checked before being accessed.
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;

  // A flag to skip the initialization sequence of SyncFileSystemService for
  // testing.
  bool skip_initialize_syncfs_service_for_testing_ = false;

  storage::SandboxFileSystemBackendDelegate* GetDelegate() const;

  void InitializeSyncFileSystemService(const GURL& origin_url,
                                       SyncStatusCallback callback);
  void DidInitializeSyncFileSystemService(storage::FileSystemContext* context,
                                          const GURL& origin_url,
                                          storage::FileSystemType type,
                                          storage::OpenFileSystemMode mode,
                                          ResolveURLCallback callback,
                                          SyncStatusCode status);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_FILE_SYSTEM_BACKEND_H_
