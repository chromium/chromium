// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_MANAGER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "chrome/browser/sync_file_system/task_logger.h"

namespace base {
class Location;
class SequencedTaskRunner;
}

namespace sync_file_system {
namespace drive_backend {

class SyncTask;
class SyncTaskToken;
struct TaskBlocker;

// This class manages asynchronous tasks for Sync FileSystem.  Each task must be
// either a Task or a SyncTask.
// The instance runs single task as the foreground task, and multiple tasks as
// background tasks.  Running background task has a TaskBlocker that
// describes which task can run in parallel.  When a task start running as a
// background task, SyncTaskManager checks if any running background task
// doesn't block the new background task, and queues it up if it can't run.
class SyncTaskManager {
 public:
  using Task = base::OnceCallback<void(SyncStatusCallback callback)>;
  using Continuation =
      base::OnceCallback<void(std::unique_ptr<SyncTaskToken> token)>;

  enum Priority {
    PRIORITY_LOW,
    PRIORITY_MED,
    PRIORITY_HIGH,
  };

  class Client {
   public:
    virtual ~Client() {}

    // Called when the manager is idle.
    virtual void MaybeScheduleNextTask() = 0;

    // Called when the manager is notified a task is done.
    virtual void NotifyLastOperationStatus(
        SyncStatusCode last_operation_status,
        bool last_operation_used_network) = 0;

    virtual void RecordTaskLog(
        std::unique_ptr<TaskLogger::TaskLog> task_log) = 0;
  };

  // Runs at most |maximum_background_tasks| parallel as background tasks.
  // If |maximum_background_tasks| is zero, all task runs as foreground task.
  SyncTaskManager(base::WeakPtr<Client> client,
                  size_t maximum_background_task,
                  const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  SyncTaskManager(const SyncTaskManager&) = delete;
  SyncTaskManager& operator=(const SyncTaskManager&) = delete;

  virtual ~SyncTaskManager();

  // This needs to be called to start task scheduling.
  // If |status| is not SYNC_STATUS_OK calling this may change the
  // service status. This should not be called more than once.
  void Initialize(SyncStatusCode status);

  // Schedules a task at the given priority.
  void ScheduleTask(const base::Location& from_here,
                    Task task,
                    Priority priority,
                    SyncStatusCallback callback);
  void ScheduleSyncTask(const base::Location& from_here,
                        std::unique_ptr<SyncTask> task,
                        Priority priority,
                        SyncStatusCallback callback);

  // Runs the posted task only when we're idle.  Returns true if that task is
  // scheduled.
  bool ScheduleTaskIfIdle(const base::Location& from_here,
                          Task task,
                          SyncStatusCallback callback);
  bool ScheduleSyncTaskIfIdle(const base::Location& from_here,
                              std::unique_ptr<SyncTask> task,
                              SyncStatusCallback callback);

  // Notifies SyncTaskManager that the task associated to |token| has finished
  // with |status|.
  static void NotifyTaskDone(std::unique_ptr<SyncTaskToken> token,
                             SyncStatusCode status);

  // Updates |task_blocker| associated to the current task by specified
  // |task_blocker| and turns the current task to a background task if
  // the current task is running as a foreground task.
  // If specified |task_blocker| is blocked by any other blocking factor
  // associated to an existing background task, this function waits for the
  // existing background task to finish.
  // Upon the task is ready to run as a background task, calls |continuation|
  // with new SyncTaskToken.
  // Note that this function once releases previous |task_blocker| before
  // applying new |task_blocker|.  So, any other task may be run before
  // invocation of |continuation|.
  static void UpdateTaskBlocker(
      std::unique_ptr<SyncTaskToken> current_task_token,
      std::unique_ptr<TaskBlocker> task_blocker,
      Continuation continuation);

  bool IsRunningTask(int64_t task_token_id) const;

  void DetachFromSequence();

 private:
  struct PendingTask {
    base::OnceClosure closure;
    Priority priority;
    int64_t seq;

    PendingTask();
    PendingTask(base::OnceClosure task, Priority pri, int seq);
    ~PendingTask();

    PendingTask(PendingTask&& other);
    PendingTask& operator=(PendingTask&& other);
  };

  struct PendingTaskComparator {
    bool operator()(const PendingTask& left,
                    const PendingTask& right) const;
  };

  // Non-static version of NotifyTaskDone.
  void NotifyTaskDoneBody(std::unique_ptr<SyncTaskToken> token,
                          SyncStatusCode status);

  // Non-static version of UpdateTaskBlocker.
  void UpdateTaskBlockerBody(
      std::unique_ptr<SyncTaskToken> foreground_task_token,
      std::unique_ptr<SyncTaskToken> background_task_token,
      std::unique_ptr<TaskLogger::TaskLog> task_log,
      std::unique_ptr<TaskBlocker> task_blocker,
      Continuation continuation);

  // This should be called when an async task needs to get a task token.
  // SyncTasksToken::UpdateTask must be called manually on the returned token.
  std::unique_ptr<SyncTaskToken> GetUnupdatedToken();

  std::unique_ptr<SyncTaskToken> GetTokenForBackgroundTask(
      const base::Location& from_here,
      SyncStatusCallback callback,
      std::unique_ptr<TaskBlocker> task_blocker);

  void PushPendingTask(base::OnceClosure closure, Priority priority);

  void RunTask(std::unique_ptr<SyncTaskToken> token,
               std::unique_ptr<SyncTask> task);

  // Runs a pending task as a foreground task if possible.
  // If |token| is non-nullptr, put |token| back to |token_| beforehand.
  void MaybeStartNextForegroundTask(std::unique_ptr<SyncTaskToken> token);

  base::WeakPtr<Client> client_;

  // Owns running SyncTask to cancel the task on SyncTaskManager deletion.
  std::unique_ptr<SyncTask> running_foreground_task_;

  // Owns running backgrounded SyncTask to cancel the task on SyncTaskManager
  // deletion.
  std::unordered_map<int64_t, std::unique_ptr<SyncTask>>
      running_background_tasks_;

  size_t maximum_background_task_;

  // Holds pending continuation to move task to background.
  base::OnceClosure pending_backgrounding_task_;

  std::priority_queue<PendingTask, std::vector<PendingTask>,
                      PendingTaskComparator> pending_tasks_;
  int64_t pending_task_seq_;
  int64_t task_token_seq_;

  // Absence of |token_| implies a task is running. Incoming tasks should
  // wait for the task to finish in |pending_tasks_| if |token_| is null.
  // Each task must take TaskToken instance from |token_| and must hold it
  // until it finished. And the task must return the instance through
  // NotifyTaskDone when the task finished.
  std::unique_ptr<SyncTaskToken> token_;

  TaskDependencyManager dependency_manager_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SyncTaskManager> weak_ptr_factory_{this};
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_MANAGER_H_
