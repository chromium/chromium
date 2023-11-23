// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_JOB_TASK_SOURCE_H_
#define BASE_TASK_THREAD_POOL_JOB_TASK_SOURCE_H_

#include <stddef.h>

#include <atomic>
#include <cstdint>
#include <utility>

#include "base/base_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/common/checked_lock.h"
#include "base/task/common/task_annotator.h"
#include "base/task/post_job.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/job_task_source_interface.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/task_source.h"
#include "base/task/thread_pool/task_source_sort_key.h"

namespace base::internal {

class PooledTaskRunnerDelegate;

// A JobTaskSource generates many Tasks from a single RepeatingClosure.
//
// Derived classes control the intended concurrency with GetMaxConcurrency().
class BASE_EXPORT JobTaskSourceNew : public JobTaskSource {
 public:
  JobTaskSourceNew(const Location& from_here,
                   const TaskTraits& traits,
                   RepeatingCallback<void(JobDelegate*)> worker_task,
                   MaxConcurrencyCallback max_concurrency_callback,
                   PooledTaskRunnerDelegate* delegate);
  JobTaskSourceNew(const JobTaskSource&) = delete;
  JobTaskSourceNew& operator=(const JobTaskSourceNew&) = delete;

  // Called before the task source is enqueued to initialize task metadata.
  void WillEnqueue(int sequence_num, TaskAnnotator& annotator) override;

  // Notifies this task source that max concurrency increased. Returns false iff
  // there was an unsuccessful attempt to enqueue the task source.
  bool NotifyConcurrencyIncrease() override;

  // Informs this JobTaskSource that the current thread would like to join and
  // contribute to running |worker_task|. Returns true if the joining thread can
  // contribute (RunJoinTask() can be called), or false if joining was completed
  // and all other workers returned because either there's no work remaining or
  // Job was cancelled.
  bool WillJoin() override;

  // Contributes to running |worker_task| and returns true if the joining thread
  // can contribute again (RunJoinTask() can be called again), or false if
  // joining was completed and all other workers returned because either there's
  // no work remaining or Job was cancelled. This should be called only after
  // WillJoin() or RunJoinTask() previously returned true.
  bool RunJoinTask() override;

  // Cancels this JobTaskSource, causing all workers to yield and WillRunTask()
  // to return RunStatus::kDisallowed.
  void Cancel(TaskSource::Transaction* transaction = nullptr) override;

  // TaskSource:
  ExecutionEnvironment GetExecutionEnvironment() override;
  size_t GetRemainingConcurrency() const override;
  TaskSourceSortKey GetSortKey() const override;
  TimeTicks GetDelayedSortKey() const override;
  bool HasReadyTasks(TimeTicks now) const override;

  bool IsActive() const override;
  size_t GetWorkerCount() const override;

  // Returns the maximum number of tasks from this TaskSource that can run
  // concurrently.
  size_t GetMaxConcurrency() const override;

  uint8_t AcquireTaskId() override;
  void ReleaseTaskId(uint8_t task_id) override;

  // Returns true if a worker should return from the worker task on the current
  // thread ASAP.
  bool ShouldYield() override;

  PooledTaskRunnerDelegate* GetDelegate() const override;

 private:
  // Atomic variable to track job state.
  class State {
   public:
    // When set, the job is canceled.
    static constexpr uint32_t kCanceledMask = 1 << 0;
    // When set, the Join()'ing thread wants to be signaled when worker count
    // is decremented or capacity is created by a max concurrency increase.
    static constexpr uint32_t kSignalJoinMask = 1 << 1;
    // When set, the job is queued. Note: The job may be queued when this is not
    // set, see details in JobTaskSource::State::ExitWillRunTask().
    static constexpr uint32_t kQueuedMask = 1 << 2;
    // When set, WillRunTask() is not running *or* WillRunTask() is running and
    // there was a request to keep the job queued (via
    // `ShouldQueueUponCapacityIncrease()` or `WillReenqueue()`).
    static constexpr uint32_t kOutsideWillRunTaskOrMustReenqueueMask = 1 << 3;
    // Offset for the number of workers running the job.
    static constexpr int kWorkerCountBitOffset = 4;
    static constexpr uint32_t kWorkerCountIncrement = 1
                                                      << kWorkerCountBitOffset;

    struct Value {
      uint8_t worker_count() const {
        return static_cast<uint8_t>(value >> kWorkerCountBitOffset);
      }
      bool canceled() const { return value & kCanceledMask; }
      bool signal_join() const { return value & kSignalJoinMask; }
      bool queued() const { return value & kQueuedMask; }
      bool outside_will_run_task_or_must_reenqueue() const {
        return value & kOutsideWillRunTaskOrMustReenqueueMask;
      }

      uint32_t value;
    };

    State();
    ~State();

    // Sets as canceled. Returns the state before the operation.
    Value Cancel();

    // Increments the worker count by 1. Returns the state before the operation.
    //
    // This requires holding `increment_worker_count_lock()`, to allow
    // WaitForParticipationOpportunity() to check worker count and apply changes
    // with a guarantee that it hasn't been incremented in between (worker count
    // could still be decremented while the lock is held).
    Value IncrementWorkerCount()
        EXCLUSIVE_LOCKS_REQUIRED(increment_worker_count_lock());

    // Decrements the worker count by 1. Returns the state before the operation.
    Value DecrementWorkerCount();

    // Requests to signal the Join()'ing thread when worker count is
    // decremented or capacity is created by increasing "max concurrency".
    // Returns the state before the operation.
    Value RequestSignalJoin();

    // Returns whether the Join()'ing thread should be signaled when worker
    // count is decremented or capacity is created by increasing "max
    // concurrency". Resets the bit so that this won't return true until
    // `RequestSignalJoin()` is called again.
    bool FetchAndResetRequestSignalJoin();

    // Indicates that max capacity was increased above the number of workers.
    // Returns true iff the job should be queued.
    bool ShouldQueueUponCapacityIncrease();

    // Indicates that WillRunTask() was entered. Returns the previous state.
    Value EnterWillRunTask();

    // Indicates that WillRunTask() will exit. `saturated` is true iff
    // `WillRunTask()` determined that max concurrency is reached. Returns true
    // iff `ShouldQueueUponCapacityIncrease()` or `WillQueue()` was invoked
    // since `EnterWillRunTask()`.
    bool ExitWillRunTask(bool saturated);

    // Indicates that `DidProcessTask()` decided to re-enqueue the job. If this
    // returns false, the job shouldn't re-enqueue the job (another worker
    // currently in `WillRunTask()` will request that it remains in the queue).
    bool WillReenqueue();

    // Loads and returns the state.
    Value Load() const;

    // Returns a lock that must be held to call `IncrementWorkerCount()`.
    CheckedLock& increment_worker_count_lock() {
      return increment_worker_count_lock_;
    }

   private:
    std::atomic<uint32_t> value_{kOutsideWillRunTaskOrMustReenqueueMask};
    CheckedLock increment_worker_count_lock_{UniversalSuccessor()};
  };

  ~JobTaskSourceNew() override;

  // Called from the joining thread. Waits for the worker count to be below or
  // equal to max concurrency (may happen when "max concurrency" increases or
  // the worker count is decremented). Returns true if the joining thread should
  // run a task, or false if joining was completed and all other workers
  // returned because either there's no work remaining or Job was cancelled.
  bool WaitForParticipationOpportunity();

  size_t GetMaxConcurrency(size_t worker_count) const;

  // TaskSource:
  RunStatus WillRunTask() override;
  Task TakeTask(TaskSource::Transaction* transaction) override;
  absl::optional<Task> Clear(TaskSource::Transaction* transaction) override;
  bool DidProcessTask(TaskSource::Transaction* transaction) override;
  bool WillReEnqueue(TimeTicks now,
                     TaskSource::Transaction* transaction) override;
  bool OnBecomeReady() override;

  State state_;

  // Signaled when the joining thread wants to particpate and capacity is
  // created by increasing "max concurrency" or decrementing the worker count.
  WaitableEvent join_event_{WaitableEvent::ResetPolicy::AUTOMATIC};

  std::atomic<uint32_t> assigned_task_ids_{0};

  RepeatingCallback<size_t(size_t)> max_concurrency_callback_;

  // Worker task set by the job owner.
  RepeatingCallback<void(JobDelegate*)> worker_task_;
  // Task returned from TakeTask(), that calls |worker_task_| internally.
  RepeatingClosure primary_task_;

  TaskMetadata task_metadata_;

  const TimeTicks ready_time_;
  raw_ptr<PooledTaskRunnerDelegate, LeakedDanglingUntriaged> delegate_;
};

}  // namespace base::internal

#endif  // BASE_TASK_THREAD_POOL_JOB_TASK_SOURCE_H_
