// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"

using storage::FileSystemURL;

namespace sync_file_system {
namespace drive_backend {

namespace {

class SyncTaskAdapter : public ExclusiveTask {
 public:
  explicit SyncTaskAdapter(const SyncTaskManager::Task& task) : task_(task) {}
  ~SyncTaskAdapter() override {}

  void RunExclusive(const SyncStatusCallback& callback) override {
    task_.Run(callback);
  }

 private:
  SyncTaskManager::Task task_;

  DISALLOW_COPY_AND_ASSIGN(SyncTaskAdapter);
};

}  // namespace

SyncTaskManager::PendingTask::PendingTask() {}

SyncTaskManager::PendingTask::PendingTask(
    const base::Closure& task, Priority pri, int seq)
    : task(task), priority(pri), seq(seq) {}

SyncTaskManager::PendingTask::PendingTask(const PendingTask& other) = default;

SyncTaskManager::PendingTask::~PendingTask() {}

bool SyncTaskManager::PendingTaskComparator::operator()(
    const PendingTask& left,
    const PendingTask& right) const {
  if (left.priority != right.priority)
    return left.priority < right.priority;
  return left.seq > right.seq;
}

SyncTaskManager::SyncTaskManager(
    base::WeakPtr<Client> client,
    size_t maximum_background_task,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : client_(client),
      maximum_background_task_(maximum_background_task),
      pending_task_seq_(0),
      task_token_seq_(SyncTaskToken::kMinimumBackgroundTaskTokenID),
      task_runner_(task_runner) {}

SyncTaskManager::~SyncTaskManager() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  client_.reset();
  token_.reset();
}

void SyncTaskManager::Initialize(SyncStatusCode status) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(!token_);
  NotifyTaskDone(
      SyncTaskToken::CreateForForegroundTask(
          weak_ptr_factory_.GetWeakPtr(), task_runner_.get()),
      status);
}

void SyncTaskManager::ScheduleTask(const base::Location& from_here,
                                   const Task& task,
                                   Priority priority,
                                   const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ScheduleSyncTask(from_here,
                   std::unique_ptr<SyncTask>(new SyncTaskAdapter(task)),
                   priority, callback);
}

void SyncTaskManager::ScheduleSyncTask(const base::Location& from_here,
                                       std::unique_ptr<SyncTask> task,
                                       Priority priority,
                                       const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  std::unique_ptr<SyncTaskToken> token(GetToken(from_here, callback));
  if (!token) {
    PushPendingTask(
        base::Bind(&SyncTaskManager::ScheduleSyncTask,
                   weak_ptr_factory_.GetWeakPtr(), from_here,
                   base::Passed(&task), priority, callback),
        priority);
    return;
  }
  RunTask(std::move(token), std::move(task));
}

bool SyncTaskManager::ScheduleTaskIfIdle(const base::Location& from_here,
                                         const Task& task,
                                         const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  return ScheduleSyncTaskIfIdle(
      from_here, std::unique_ptr<SyncTask>(new SyncTaskAdapter(task)),
      callback);
}

bool SyncTaskManager::ScheduleSyncTaskIfIdle(
    const base::Location& from_here,
    std::unique_ptr<SyncTask> task,
    const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  std::unique_ptr<SyncTaskToken> token(GetToken(from_here, callback));
  if (!token)
    return false;
  RunTask(std::move(token), std::move(task));
  return true;
}

// static
void SyncTaskManager::NotifyTaskDone(std::unique_ptr<SyncTaskToken> token,
                                     SyncStatusCode status) {
  DCHECK(token);

  SyncTaskManager* manager = token->manager();
  if (token->token_id() == SyncTaskToken::kTestingTaskTokenID) {
    DCHECK(!manager);
    SyncStatusCallback callback = token->callback();
    token->clear_callback();
    callback.Run(status);
    return;
  }

  if (manager)
    manager->NotifyTaskDoneBody(std::move(token), status);
}

// static
void SyncTaskManager::UpdateTaskBlocker(
    std::unique_ptr<SyncTaskToken> current_task_token,
    std::unique_ptr<TaskBlocker> task_blocker,
    const Continuation& continuation) {
  DCHECK(current_task_token);

  SyncTaskManager* manager = current_task_token->manager();
  if (current_task_token->token_id() == SyncTaskToken::kTestingTaskTokenID) {
    DCHECK(!manager);
    continuation.Run(std::move(current_task_token));
    return;
  }

  if (!manager)
    return;

  std::unique_ptr<SyncTaskToken> foreground_task_token;
  std::unique_ptr<SyncTaskToken> background_task_token;
  std::unique_ptr<TaskLogger::TaskLog> task_log =
      current_task_token->PassTaskLog();
  if (current_task_token->token_id() == SyncTaskToken::kForegroundTaskTokenID)
    foreground_task_token = std::move(current_task_token);
  else
    background_task_token = std::move(current_task_token);

  manager->UpdateTaskBlockerBody(
      std::move(foreground_task_token), std::move(background_task_token),
      std::move(task_log), std::move(task_blocker), continuation);
}

bool SyncTaskManager::IsRunningTask(int64_t token_id) const {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // If the client is gone, all task should be aborted.
  if (!client_)
    return false;

  if (token_id == SyncTaskToken::kForegroundTaskTokenID)
    return true;

  return running_background_tasks_.find(token_id) !=
         running_background_tasks_.end();
}

void SyncTaskManager::DetachFromSequence() {
  sequence_checker_.DetachFromSequence();
}

void SyncTaskManager::NotifyTaskDoneBody(std::unique_ptr<SyncTaskToken> token,
                                         SyncStatusCode status) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(token);

  DVLOG(3) << "NotifyTaskDone: " << "finished with status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " " << token->location().ToString();

  if (token->task_blocker()) {
    dependency_manager_.Erase(token->task_blocker());
    token->clear_task_blocker();
  }

  if (client_) {
    if (token->has_task_log()) {
      token->FinalizeTaskLog(SyncStatusCodeToString(status));
      client_->RecordTaskLog(token->PassTaskLog());
    }
  }

  std::unique_ptr<SyncTask> task;
  SyncStatusCallback callback = token->callback();
  token->clear_callback();
  if (token->token_id() == SyncTaskToken::kForegroundTaskTokenID) {
    token_ = std::move(token);
    task = std::move(running_foreground_task_);
  } else {
    task = std::move(running_background_tasks_[token->token_id()]);
    running_background_tasks_.erase(token->token_id());
  }

  // Acquire the token to prevent a new task to jump into the queue.
  token = std::move(token_);

  bool task_used_network = false;
  if (task)
    task_used_network = task->used_network();

  if (client_)
    client_->NotifyLastOperationStatus(status, task_used_network);

  if (!callback.is_null())
    callback.Run(status);

  // Post MaybeStartNextForegroundTask rather than calling it directly to avoid
  // making the call-chaing longer.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncTaskManager::MaybeStartNextForegroundTask,
                     weak_ptr_factory_.GetWeakPtr(), std::move(token)));
}

void SyncTaskManager::UpdateTaskBlockerBody(
    std::unique_ptr<SyncTaskToken> foreground_task_token,
    std::unique_ptr<SyncTaskToken> background_task_token,
    std::unique_ptr<TaskLogger::TaskLog> task_log,
    std::unique_ptr<TaskBlocker> task_blocker,
    const Continuation& continuation) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // Run the task directly if the parallelization is disabled.
  if (!maximum_background_task_) {
    DCHECK(foreground_task_token);
    DCHECK(!background_task_token);
    foreground_task_token->SetTaskLog(std::move(task_log));
    continuation.Run(std::move(foreground_task_token));
    return;
  }

  // Clear existing |task_blocker| from |dependency_manager_| before
  // getting |foreground_task_token|, so that we can avoid dead lock.
  if (background_task_token && background_task_token->task_blocker()) {
    dependency_manager_.Erase(background_task_token->task_blocker());
    background_task_token->clear_task_blocker();
  }

  // Try to get |foreground_task_token|.  If it's not available, wait for
  // current foreground task to finish.
  if (!foreground_task_token) {
    DCHECK(background_task_token);
    foreground_task_token = GetToken(background_task_token->location(),
                                     SyncStatusCallback());
    if (!foreground_task_token) {
      PushPendingTask(
          base::Bind(&SyncTaskManager::UpdateTaskBlockerBody,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&foreground_task_token),
                     base::Passed(&background_task_token),
                     base::Passed(&task_log),
                     base::Passed(&task_blocker),
                     continuation),
          PRIORITY_HIGH);
      MaybeStartNextForegroundTask(nullptr);
      return;
    }
  }

  // Check if the task can run as a background task now.
  // If there are too many task running or any other task blocks current
  // task, wait for any other task to finish.
  bool task_number_limit_exceeded =
      !background_task_token &&
      running_background_tasks_.size() >= maximum_background_task_;
  if (task_number_limit_exceeded ||
      !dependency_manager_.Insert(task_blocker.get())) {
    DCHECK(!running_background_tasks_.empty());
    DCHECK(pending_backgrounding_task_.is_null());

    // Wait for NotifyTaskDone to release a |task_blocker|.
    pending_backgrounding_task_ =
        base::Bind(&SyncTaskManager::UpdateTaskBlockerBody,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&foreground_task_token),
                   base::Passed(&background_task_token),
                   base::Passed(&task_log),
                   base::Passed(&task_blocker),
                   continuation);
    return;
  }

  if (background_task_token) {
    background_task_token->set_task_blocker(std::move(task_blocker));
  } else {
    base::Location from_here = foreground_task_token->location();
    SyncStatusCallback callback = foreground_task_token->callback();
    foreground_task_token->clear_callback();

    background_task_token = SyncTaskToken::CreateForBackgroundTask(
        weak_ptr_factory_.GetWeakPtr(), task_runner_.get(), task_token_seq_++,
        std::move(task_blocker));
    background_task_token->UpdateTask(from_here, callback);
    running_background_tasks_[background_task_token->token_id()] =
        std::move(running_foreground_task_);
  }

  token_ = std::move(foreground_task_token);
  MaybeStartNextForegroundTask(nullptr);
  background_task_token->SetTaskLog(std::move(task_log));
  continuation.Run(std::move(background_task_token));
}

std::unique_ptr<SyncTaskToken> SyncTaskManager::GetToken(
    const base::Location& from_here,
    const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  if (!token_)
    return nullptr;
  token_->UpdateTask(from_here, callback);
  return std::move(token_);
}

void SyncTaskManager::PushPendingTask(
    const base::Closure& closure, Priority priority) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  pending_tasks_.push(PendingTask(closure, priority, pending_task_seq_++));
}

void SyncTaskManager::RunTask(std::unique_ptr<SyncTaskToken> token,
                              std::unique_ptr<SyncTask> task) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(!running_foreground_task_);

  running_foreground_task_ = std::move(task);
  running_foreground_task_->RunPreflight(std::move(token));
}

void SyncTaskManager::MaybeStartNextForegroundTask(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  if (token) {
    DCHECK(!token_);
    token_ = std::move(token);
  }

  if (!pending_backgrounding_task_.is_null()) {
    base::Closure closure = pending_backgrounding_task_;
    pending_backgrounding_task_.Reset();
    closure.Run();
    return;
  }

  if (!token_)
    return;

  if (!pending_tasks_.empty()) {
    base::Closure closure = pending_tasks_.top().task;
    pending_tasks_.pop();
    closure.Run();
    return;
  }

  if (client_)
    client_->MaybeScheduleNextTask();
}

}  // namespace drive_backend
}  // namespace sync_file_system
