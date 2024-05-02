// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_THREAD_GROUP_WORKER_DELEGATE_H_
#define BASE_TASK_THREAD_POOL_THREAD_GROUP_WORKER_DELEGATE_H_

#include <optional>

#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_group.h"
#include "base/task/thread_pool/worker_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/thread_checker.h"

namespace base::internal {

// This class is not a WorkerThread::Delegate, instead implementing
// functionality which is common to all WorkerThread::Delegates present in a
// ThreadGroup.
class ThreadGroup::ThreadGroupWorkerDelegate : public BlockingObserver {
 public:
  explicit ThreadGroupWorkerDelegate(TrackedRef<ThreadGroup> outer,
                                     bool is_excess);
  ThreadGroupWorkerDelegate(const ThreadGroupWorkerDelegate&) = delete;
  ThreadGroupWorkerDelegate& operator=(const ThreadGroupWorkerDelegate&) =
      delete;

  ~ThreadGroupWorkerDelegate() override;

  TimeDelta ThreadPoolSleepTimeout();

  // BlockingObserver:
  void BlockingStarted(BlockingType blocking_type) override;
  void BlockingTypeUpgraded() override;
  void BlockingEnded() override;

  // Notifies the worker of shutdown, possibly marking the running task as
  // MAY_BLOCK.
  void OnShutdownStartedLockRequired(BaseScopedCommandsExecutor* executor)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Increments max [best effort] tasks iff this worker has been within a
  // ScopedBlockingCall for more than |may_block_threshold|.
  void MaybeIncrementMaxTasksLockRequired()
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Increments max [best effort] tasks.
  void IncrementMaxTasksLockRequired() EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  TaskPriority current_task_priority_lock_required() const
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
    return *read_any().current_task_priority;
  }

  // Exposed for AnnotateAcquiredLockAlias.
  const CheckedLock& lock() const LOCK_RETURNED(outer_->lock_) {
    return outer_->lock_;
  }

 protected:
  // Returns true if |worker| is allowed to cleanup and remove itself from the
  // thread group. Called from GetWork() when no work is available.
  virtual bool CanCleanupLockRequired(const WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) = 0;

  // Returns true iff the worker can get work. Cleans up the worker or puts it
  // on the idle set if it can't get work.
  virtual bool CanGetWorkLockRequired(BaseScopedCommandsExecutor* executor,
                                      WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) = 0;

  RegisteredTaskSource GetWorkLockRequired(BaseScopedCommandsExecutor* executor,
                                           WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Calls cleanup on |worker| and removes it from the thread group. Called from
  // GetWork() when no work is available and CanCleanupLockRequired() returns
  // true.
  virtual void CleanupLockRequired(BaseScopedCommandsExecutor* executor,
                                   WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) = 0;

  // Called in GetWork() when a worker becomes idle.
  virtual void OnWorkerBecomesIdleLockRequired(
      BaseScopedCommandsExecutor* executor,
      WorkerThread* worker) EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) = 0;

  // See worker_thread.h for documentation.
  void OnMainEntryImpl(WorkerThread* worker);
  void RecordUnnecessaryWakeupImpl();

  // Only used in DCHECKs.
  template <typename Worker>
  bool ContainsWorker(const std::vector<scoped_refptr<Worker>>& workers,
                      const WorkerThread* worker) {
    auto it = ranges::find_if(
        workers,
        [worker](const scoped_refptr<Worker>& i) { return i.get() == worker; });
    return it != workers.end();
  }

  // Accessed only from the worker thread.
  struct WorkerOnly {
    WorkerOnly();
    ~WorkerOnly();
    // Associated WorkerThread, if any, initialized in OnMainEntry().
    raw_ptr<WorkerThread> worker_thread_;

#if BUILDFLAG(IS_WIN)
    std::unique_ptr<win::ScopedWindowsThreadEnvironment> win_thread_environment;
#endif  // BUILDFLAG(IS_WIN)
  } worker_only_;

  // Writes from the worker thread protected by |outer_->lock_|. Reads from any
  // thread, protected by |outer_->lock_| when not on the worker thread.
  struct WriteWorkerReadAny {
    // The priority of the task the worker is currently running if any.
    std::optional<TaskPriority> current_task_priority;
    // The shutdown behavior of the task the worker is currently running if any.
    std::optional<TaskShutdownBehavior> current_shutdown_behavior;

    // Time when MayBlockScopeEntered() was last called. Reset when
    // BlockingScopeExited() is called.
    TimeTicks blocking_start_time;

    // Whether the worker is currently running a task (i.e. GetWork() has
    // returned a non-empty task source and DidProcessTask() hasn't been called
    // yet).
    bool is_running_task() const { return !!current_shutdown_behavior; }
  } write_worker_read_any_;

  WorkerOnly& worker_only() {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return worker_only_;
  }

  WriteWorkerReadAny& write_worker() EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return write_worker_read_any_;
  }

  const WriteWorkerReadAny& read_any() const
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
    return write_worker_read_any_;
  }

  const WriteWorkerReadAny& read_worker() const {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return write_worker_read_any_;
  }

  const TrackedRef<ThreadGroup> outer_;

  // Whether the worker is in excess. This must be decided at worker creation
  // time to prevent unnecessarily discarding TLS state, as well as any behavior
  // the OS has learned about a given thread.
  const bool is_excess_;

  // Whether |outer_->max_tasks_|/|outer_->max_best_effort_tasks_| were
  // incremented due to a ScopedBlockingCall on the thread.
  bool incremented_max_tasks_since_blocked_ GUARDED_BY(outer_->lock_) = false;
  bool incremented_max_best_effort_tasks_since_blocked_
      GUARDED_BY(outer_->lock_) = false;
  // Whether |outer_->max_tasks_| and |outer_->max_best_effort_tasks_| was
  // incremented due to running CONTINUE_ON_SHUTDOWN on the thread during
  // shutdown.
  bool incremented_max_tasks_for_shutdown_ GUARDED_BY(outer_->lock_) = false;

  // Verifies that specific calls are always made from the worker thread.
  THREAD_CHECKER(worker_thread_checker_);
};

}  // namespace base::internal

#endif  // BASE_TASK_THREAD_POOL_THREAD_GROUP_WORKER_DELEGATE_H_
