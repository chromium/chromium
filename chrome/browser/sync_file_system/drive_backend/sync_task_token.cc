// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"

namespace sync_file_system {
namespace drive_backend {

const int64_t SyncTaskToken::kTestingTaskTokenID = -1;
const int64_t SyncTaskToken::kForegroundTaskTokenID = 0;
const int64_t SyncTaskToken::kMinimumBackgroundTaskTokenID = 1;

// static
std::unique_ptr<SyncTaskToken> SyncTaskToken::CreateForTesting(
    SyncStatusCallback callback) {
  return base::WrapUnique(new SyncTaskToken(
      base::WeakPtr<SyncTaskManager>(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), kTestingTaskTokenID,
      nullptr,  // task_blocker
      std::move(callback)));
}

// static
std::unique_ptr<SyncTaskToken> SyncTaskToken::CreateForForegroundTask(
    const base::WeakPtr<SyncTaskManager>& manager,
    base::SequencedTaskRunner* task_runner) {
  return base::WrapUnique(new SyncTaskToken(manager, task_runner,
                                            kForegroundTaskTokenID,
                                            nullptr,  // task_blocker
                                            SyncStatusCallback()));
}

// static
std::unique_ptr<SyncTaskToken> SyncTaskToken::CreateForBackgroundTask(
    const base::WeakPtr<SyncTaskManager>& manager,
    base::SequencedTaskRunner* task_runner,
    int64_t token_id,
    std::unique_ptr<TaskBlocker> task_blocker) {
  return base::WrapUnique(new SyncTaskToken(manager, task_runner, token_id,
                                            std::move(task_blocker),
                                            SyncStatusCallback()));
}

void SyncTaskToken::UpdateTask(const base::Location& location,
                               SyncStatusCallback callback) {
  DCHECK(callback_.is_null());
  location_ = location;
  callback_ = std::move(callback);
  DVLOG(2) << "Token updated: " << location_.ToString();
}

SyncTaskToken::~SyncTaskToken() {}

// static
SyncStatusCallback SyncTaskToken::WrapToCallback(
    std::unique_ptr<SyncTaskToken> token) {
  return base::BindOnce(&SyncTaskManager::NotifyTaskDone, std::move(token));
}

void SyncTaskToken::set_task_blocker(
    std::unique_ptr<TaskBlocker> task_blocker) {
  task_blocker_ = std::move(task_blocker);
}

const TaskBlocker* SyncTaskToken::task_blocker() const {
  return task_blocker_.get();
}

void SyncTaskToken::clear_task_blocker() {
  task_blocker_.reset();
}

void SyncTaskToken::InitializeTaskLog(const std::string& task_description) {
  task_log_ = std::make_unique<TaskLogger::TaskLog>();
  task_log_->start_time = base::TimeTicks::Now();
  task_log_->task_description = task_description;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      TRACE_DISABLED_BY_DEFAULT("SyncFileSystem"), "SyncTask",
      TRACE_ID_LOCAL(task_log_->log_id), "task_description", task_description);
}

void SyncTaskToken::FinalizeTaskLog(const std::string& result_description) {
  TRACE_EVENT_NESTABLE_ASYNC_END1(TRACE_DISABLED_BY_DEFAULT("SyncFileSystem"),
                                  "SyncTask", TRACE_ID_LOCAL(task_log_->log_id),
                                  "result_description", result_description);

  DCHECK(task_log_);
  task_log_->result_description = result_description;
  task_log_->end_time = base::TimeTicks::Now();
}

void SyncTaskToken::RecordLog(const std::string& message) {
  DCHECK(task_log_);
  task_log_->details.push_back(message);
}

void SyncTaskToken::SetTaskLog(std::unique_ptr<TaskLogger::TaskLog> task_log) {
  task_log_ = std::move(task_log);
}

std::unique_ptr<TaskLogger::TaskLog> SyncTaskToken::PassTaskLog() {
  return std::move(task_log_);
}

SyncTaskToken::SyncTaskToken(
    const base::WeakPtr<SyncTaskManager>& manager,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    int64_t token_id,
    std::unique_ptr<TaskBlocker> task_blocker,
    SyncStatusCallback callback)
    : manager_(manager),
      task_runner_(task_runner),
      token_id_(token_id),
      callback_(std::move(callback)),
      task_blocker_(std::move(task_blocker)) {}

}  // namespace drive_backend
}  // namespace sync_file_system
