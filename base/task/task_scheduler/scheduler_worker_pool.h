// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_H_
#define BASE_TASK_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_H_

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_scheduler/can_schedule_sequence_observer.h"
#include "base/task/task_scheduler/sequence.h"
#include "base/task/task_scheduler/task.h"
#include "base/task/task_scheduler/tracked_ref.h"

namespace base {
namespace internal {

class TaskTracker;

// Interface for a worker pool.
class BASE_EXPORT SchedulerWorkerPool : public CanScheduleSequenceObserver {
 public:
  // Delegate interface for SchedulerWorkerPool.
  class BASE_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked when a |sequence| is non-empty after the SchedulerWorkerPool has
    // run a task from it. The implementation must enqueue |sequence| in the
    // appropriate priority queue, depending on |sequence| traits.
    virtual void ReEnqueueSequence(scoped_refptr<Sequence> sequence) = 0;
  };

  ~SchedulerWorkerPool() override;

  // Posts |task| to be executed by this SchedulerWorkerPool as part of
  // |sequence|. This must only be called after |task| has gone through
  // TaskTracker::WillPostTask() and after |task|'s delayed run
  // time.
  void PostTaskWithSequenceNow(Task task, scoped_refptr<Sequence> sequence);

  // Registers the worker pool in TLS.
  void BindToCurrentThread();

  // Resets the worker pool in TLS.
  void UnbindFromCurrentThread();

  // Returns true if the worker pool is registered in TLS.
  bool IsBoundToCurrentThread() const;

  // Prevents new tasks from starting to run and waits for currently running
  // tasks to complete their execution. It is guaranteed that no thread will do
  // work on behalf of this SchedulerWorkerPool after this returns. It is
  // invalid to post a task once this is called. TaskTracker::Flush() can be
  // called before this to complete existing tasks, which might otherwise post a
  // task during JoinForTesting(). This can only be called once.
  virtual void JoinForTesting() = 0;

  // Enqueues |sequence| in the worker pool's priority queue, then wakes up a
  // worker if the worker pool is not bound to the current thread, i.e. if
  // |sequence| is changing pools.
  virtual void ReEnqueueSequence(scoped_refptr<Sequence> sequence) = 0;

 protected:
  SchedulerWorkerPool(TrackedRef<TaskTracker> task_tracker,
                      TrackedRef<Delegate> delegate);

  const TrackedRef<TaskTracker> task_tracker_;
  const TrackedRef<Delegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerPool);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_H_
