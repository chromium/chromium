// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_WORKER_THREAD_SEMAPHORE_H_
#define BASE_TASK_THREAD_POOL_WORKER_THREAD_SEMAPHORE_H_

#include "base/task/thread_pool/worker_thread.h"

#include "base/task/thread_pool/semaphore.h"
#include "base/task/thread_pool/task_tracker.h"

namespace base::internal {

class TaskTracker;

class BASE_EXPORT WorkerThreadSemaphore : public WorkerThread {
 public:
  class BASE_EXPORT Delegate : public WorkerThread::Delegate {
   protected:
    friend WorkerThreadSemaphore;
    explicit Delegate(Semaphore* semaphore,
                      AtomicFlag* join_called_for_testing);

    bool TimedWait(TimeDelta timeout) override;

    // Common semaphore to wake up threads managed by the WorkerThreads sharing
    // this semaphore.
    raw_ptr<Semaphore> semaphore_;
    raw_ptr<AtomicFlag> join_called_for_testing_;

    // Whether the worker timed out last wakeup. Set in TimedWait().
    bool timed_out_;
  };

  // Everything is passed to WorkerThread's constructor, except the Delegate.
  WorkerThreadSemaphore(ThreadType thread_type_hint,
                        std::unique_ptr<Delegate> delegate,
                        TrackedRef<TaskTracker> task_tracker,
                        size_t sequence_num,
                        const CheckedLock* predecessor_lock = nullptr,
                        void* flow_terminator = nullptr);

  WorkerThreadSemaphore(const WorkerThread&) = delete;
  WorkerThreadSemaphore& operator=(const WorkerThread&) = delete;

  // Joins this WorkerThread. This function must be called after the caller has
  // set Delegate::join_called_for_testing_ and signaled the semaphore. Note
  // that this implementation is different than WorkerThreadWaitableEvent,
  // because this worker joins on a per-group basis rather than a per-worker
  // basis, given that the workers share the wakeup mechanism.
  //
  // Note: A thread that detaches before JoinForTesting() is called may still be
  // running after JoinForTesting() returns. However, it can't run tasks after
  // JoinForTesting() returns.
  void JoinForTesting();

  // WorkerThread:
  void Cleanup() override;
  Delegate* delegate() override;
  bool join_called_for_testing() const override;

 private:
  const std::unique_ptr<Delegate> delegate_;

  ~WorkerThreadSemaphore() override;
};

}  // namespace base::internal

#endif  // BASE_TASK_THREAD_POOL_WORKER_THREAD_SEMAPHORE_H_
