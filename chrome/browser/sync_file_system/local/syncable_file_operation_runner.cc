// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/syncable_file_operation_runner.h"

#include <stddef.h>

#include <algorithm>

#include "base/functional/callback.h"
#include "content/public/browser/browser_thread.h"

using storage::FileSystemURL;

namespace sync_file_system {

// SyncableFileOperationRunner::Task -------------------------------------------

bool SyncableFileOperationRunner::Task::IsRunnable(
    LocalFileSyncStatus* status) const {
  for (size_t i = 0; i < target_paths().size(); ++i) {
    if (!status->IsWritable(target_paths()[i]))
      return false;
  }
  return true;
}

void SyncableFileOperationRunner::Task::Start(LocalFileSyncStatus* status) {
  for (size_t i = 0; i < target_paths().size(); ++i) {
    DCHECK(status->IsWritable(target_paths()[i]));
    status->StartWriting(target_paths()[i]);
  }
  Run();
}

// SyncableFileOperationRunner -------------------------------------------------

SyncableFileOperationRunner::SyncableFileOperationRunner(
    int64_t max_inflight_tasks,
    LocalFileSyncStatus* sync_status)
    : sync_status_(sync_status), max_inflight_tasks_(max_inflight_tasks) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  sync_status_->AddObserver(this);
}

SyncableFileOperationRunner::~SyncableFileOperationRunner() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  for (auto& task : pending_tasks_)
    task->Cancel();
  pending_tasks_.clear();
}

void SyncableFileOperationRunner::OnSyncEnabled(const FileSystemURL& url) {
}

void SyncableFileOperationRunner::OnWriteEnabled(const FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  RunNextRunnableTask();
}

void SyncableFileOperationRunner::PostOperationTask(
    std::unique_ptr<Task> task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  pending_tasks_.push_back(std::move(task));
  RunNextRunnableTask();
}

void SyncableFileOperationRunner::RunNextRunnableTask() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  for (auto iter = pending_tasks_.begin();
       iter != pending_tasks_.end() && ShouldStartMoreTasks();) {
    if ((*iter)->IsRunnable(sync_status())) {
      ++num_inflight_tasks_;
      DCHECK_GE(num_inflight_tasks_, 1);
      std::unique_ptr<Task> task = std::move(*iter);
      pending_tasks_.erase(iter++);
      task->Start(sync_status());
      continue;
    }
    ++iter;
  }
}

void SyncableFileOperationRunner::OnOperationCompleted(
    const std::vector<FileSystemURL>& target_paths) {
  --num_inflight_tasks_;
  DCHECK_GE(num_inflight_tasks_, 0);
  for (size_t i = 0; i < target_paths.size(); ++i) {
    DCHECK(sync_status()->IsWriting(target_paths[i]));
    sync_status()->EndWriting(target_paths[i]);
  }
  RunNextRunnableTask();
}

bool SyncableFileOperationRunner::ShouldStartMoreTasks() const {
  return num_inflight_tasks_ < max_inflight_tasks_;
}

}  // namespace sync_file_system
