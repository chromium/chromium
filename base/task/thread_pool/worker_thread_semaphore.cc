// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/worker_thread_semaphore.h"

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/task/thread_pool/semaphore.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/worker_thread_observer.h"
#include "base/threading/hang_watcher.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

namespace base::internal {

bool WorkerThreadSemaphore::Delegate::TimedWait(TimeDelta timeout) {
  const bool signaled = semaphore_->TimedWait(timeout);
  timed_out_ = !signaled;
  return signaled;
}

WorkerThreadSemaphore::Delegate::Delegate(Semaphore* semaphore,
                                          AtomicFlag* join_called_for_testing)
    : semaphore_(semaphore),
      join_called_for_testing_(join_called_for_testing) {}

WorkerThreadSemaphore::WorkerThreadSemaphore(
    ThreadType thread_type_hint,
    std::unique_ptr<Delegate> delegate,
    TrackedRef<TaskTracker> task_tracker,
    size_t sequence_num,
    const CheckedLock* predecessor_lock,
    void* flow_terminator)
    : WorkerThread(thread_type_hint,
                   task_tracker,
                   sequence_num,
                   predecessor_lock,
                   flow_terminator),
      delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

WorkerThreadSemaphore::~WorkerThreadSemaphore() {
  Destroy();
}

WorkerThreadSemaphore::Delegate* WorkerThreadSemaphore::delegate() {
  return delegate_.get();
}

bool WorkerThreadSemaphore::join_called_for_testing() const {
  return delegate_->join_called_for_testing_->IsSet();
}

void WorkerThreadSemaphore::JoinForTesting() {
  // join_called_for_testing_ is shared between semaphore workers and must be
  // set before entering this function.
  CHECK(delegate_->join_called_for_testing_->IsSet());

  PlatformThreadHandle thread_handle;
  {
    CheckedAutoLock auto_lock(thread_lock_);

    if (thread_handle_.is_null()) {
      return;
    }

    thread_handle = thread_handle_;
    // Reset |thread_handle_| so it isn't joined by the destructor.
    thread_handle_ = PlatformThreadHandle();
  }

  PlatformThread::Join(thread_handle);
}

void WorkerThreadSemaphore::Cleanup() {
  DCHECK(!should_exit_.IsSet());
  should_exit_.Set();
  // The semaphore is not signaled here (contrasted with
  // WorkerThreadWaitableEvent), because when this is called (in
  // GetWork/SwapProcessedTask) the worker is awake and won't sleep without
  // checking ShouldExit().
}

}  // namespace base::internal
