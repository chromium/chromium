// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNCABLE_FILE_OPERATION_RUNNER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNCABLE_FILE_OPERATION_RUNNER_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

// This class must run only on IO thread.
// Owned by LocalFileSyncContext.
class SyncableFileOperationRunner
    : public base::SupportsWeakPtr<SyncableFileOperationRunner>,
      public LocalFileSyncStatus::Observer {
 public:
  // Represents an operation task (which usually wraps one FileSystemOperation).
  class Task {
   public:
    Task() {}
    virtual ~Task() {}

    // Only one of Run() or Cancel() is called.
    virtual void Run() = 0;
    virtual void Cancel() = 0;

   protected:
    // This is never called after Run() or Cancel() is called.
    virtual const std::vector<storage::FileSystemURL>& target_paths() const = 0;

   private:
    friend class SyncableFileOperationRunner;
    bool IsRunnable(LocalFileSyncStatus* status) const;
    void Start(LocalFileSyncStatus* status);

    DISALLOW_COPY_AND_ASSIGN(Task);
  };

  SyncableFileOperationRunner(int64_t max_inflight_tasks,
                              LocalFileSyncStatus* sync_status);
  ~SyncableFileOperationRunner() override;

  // LocalFileSyncStatus::Observer overrides.
  void OnSyncEnabled(const storage::FileSystemURL& url) override;
  void OnWriteEnabled(const storage::FileSystemURL& url) override;

  // Runs the given |task| if no sync operation is running on any of
  // its target_paths(). This also runs pending tasks that have become
  // runnable (before running the given operation).
  // If there're ongoing sync tasks on the target_paths this method
  // just queues up the |task|.
  // Pending tasks are cancelled when this class is destructed.
  void PostOperationTask(std::unique_ptr<Task> task);

  // Runs a next runnable task (if there's any).
  void RunNextRunnableTask();

  // Called when an operation is completed. This will make |target_paths|
  // writable and may start a next runnable task.
  void OnOperationCompleted(
      const std::vector<storage::FileSystemURL>& target_paths);

  LocalFileSyncStatus* sync_status() const { return sync_status_; }

  int64_t num_pending_tasks() const {
    return static_cast<int64_t>(pending_tasks_.size());
  }

  int64_t num_inflight_tasks() const { return num_inflight_tasks_; }

 private:
  // Returns true if we should start more tasks.
  bool ShouldStartMoreTasks() const;

  // Keeps track of the writing/syncing status. Not owned.
  LocalFileSyncStatus* const sync_status_;

  std::list<std::unique_ptr<Task>> pending_tasks_;

  const int64_t max_inflight_tasks_;
  int64_t num_inflight_tasks_;

  DISALLOW_COPY_AND_ASSIGN(SyncableFileOperationRunner);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNCABLE_FILE_OPERATION_RUNNER_H_
