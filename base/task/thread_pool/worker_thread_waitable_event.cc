// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/worker_thread_waitable_event.h"

#include "base/debug/alias.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/worker_thread_observer.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED)
#include "base/allocator/partition_allocator/src/partition_alloc/thread_cache.h"
#endif

namespace base::internal {

bool WorkerThreadWaitableEvent::Delegate::TimedWait(TimeDelta timeout) {
  return wake_up_event_.TimedWait(timeout);
}

WorkerThreadWaitableEvent::WorkerThreadWaitableEvent(
    ThreadType thread_type_hint,
    std::unique_ptr<Delegate> delegate,
    TrackedRef<TaskTracker> task_tracker,
    size_t sequence_num,
    const CheckedLock* predecessor_lock)
    : WorkerThread(thread_type_hint,
                   task_tracker,
                   sequence_num,
                   predecessor_lock),
      delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  delegate_->wake_up_event_.declare_only_used_while_idle();
}

WorkerThreadWaitableEvent::~WorkerThreadWaitableEvent() = default;

void WorkerThreadWaitableEvent::JoinForTesting() {
  DCHECK(!join_called_for_testing_.IsSet());
  join_called_for_testing_.Set();
  delegate_->wake_up_event_.Signal();

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

void WorkerThreadWaitableEvent::Cleanup() {
  DCHECK(!should_exit_.IsSet());
  should_exit_.Set();
  delegate_->wake_up_event_.Signal();
}

void WorkerThreadWaitableEvent::WakeUp() {
  // Signalling an event can deschedule the current thread. Since being
  // descheduled while holding a lock is undesirable (https://crbug.com/890978),
  // assert that no lock is held by the current thread.
  CheckedLock::AssertNoLockHeldOnCurrentThread();
  // Calling WakeUp() after Cleanup() or Join() is wrong because the
  // WorkerThread cannot run more tasks.
  DCHECK(!join_called_for_testing_.IsSet());
  DCHECK(!should_exit_.IsSet());
  TRACE_EVENT_INSTANT("wakeup.flow", "WorkerThreadWaitableEvent::WakeUp",
                      perfetto::Flow::FromPointer(this));

  delegate_->wake_up_event_.Signal();
}

WorkerThreadWaitableEvent::Delegate* WorkerThreadWaitableEvent::delegate() {
  return delegate_.get();
}

}  // namespace base::internal
