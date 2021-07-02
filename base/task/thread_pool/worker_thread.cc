// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/worker_thread.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/worker_thread_observer.h"
#include "base/threading/hang_watcher.h"
#include "base/time/time_override.h"
#include "base/trace_event/base_tracing.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_APPLE)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED)
#include "base/allocator/partition_allocator/thread_cache.h"
#endif

namespace base {
namespace internal {

constexpr TimeDelta WorkerThread::Delegate::kPurgeThreadCacheIdleDelay;

void WorkerThread::Delegate::WaitForWork(WaitableEvent* wake_up_event) {
  DCHECK(wake_up_event);
  const TimeDelta sleep_time = GetSleepTimeout();

  // When a thread goes to sleep, the memory retained by its thread cache is
  // trapped there for as long as the thread sleeps. To prevent that, we can
  // either purge the thread cache right before going to sleep, or after some
  // delay.
  //
  // Purging the thread cache incurs a cost on the next task, since its thread
  // cache will be empty and allocation performance initially lower. As a lot of
  // sleeps are very short, do not purge all the time (this would also make
  // sleep / wakeups cycles more costly).
  //
  // Instead, sleep for min(timeout, 1s). If the wait times out then purge at
  // that point, and go to sleep for the remaining of the time. This ensures
  // that we do no work for short sleeps, and that threads do not get awaken
  // many times.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED)
  bool was_signaled = wake_up_event->TimedWait(
      std::min(sleep_time, kPurgeThreadCacheIdleDelay));

  // Timed out.
  if (!was_signaled) {
    ThreadCache::PurgeCurrentThread();

    if (sleep_time > kPurgeThreadCacheIdleDelay) {
      wake_up_event->TimedWait(sleep_time.is_max()
                                   ? base::TimeDelta::Max()
                                   : sleep_time - kPurgeThreadCacheIdleDelay);
    }
  }
#else
  wake_up_event->TimedWait(sleep_time);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // defined(PA_THREAD_CACHE_SUPPORTED)
}

WorkerThread::WorkerThread(ThreadPriority priority_hint,
                           std::unique_ptr<Delegate> delegate,
                           TrackedRef<TaskTracker> task_tracker,
                           const CheckedLock* predecessor_lock)
    : thread_lock_(predecessor_lock),
      delegate_(std::move(delegate)),
      task_tracker_(std::move(task_tracker)),
      priority_hint_(priority_hint),
      current_thread_priority_(GetDesiredThreadPriority()) {
  DCHECK(delegate_);
  DCHECK(task_tracker_);
  DCHECK(CanUseBackgroundPriorityForWorkerThread() ||
         priority_hint_ != ThreadPriority::BACKGROUND);
  wake_up_event_.declare_only_used_while_idle();
}

bool WorkerThread::Start(WorkerThreadObserver* worker_thread_observer) {
  CheckedLock::AssertNoLockHeldOnCurrentThread();
  CheckedAutoLock auto_lock(thread_lock_);
  DCHECK(thread_handle_.is_null());

  if (should_exit_.IsSet() || join_called_for_testing_.IsSet())
    return true;

  DCHECK(!worker_thread_observer_);
  worker_thread_observer_ = worker_thread_observer;

  self_ = this;

  constexpr size_t kDefaultStackSize = 0;
  PlatformThread::CreateWithPriority(kDefaultStackSize, this, &thread_handle_,
                                     current_thread_priority_);

  if (thread_handle_.is_null()) {
    self_ = nullptr;
    return false;
  }

  return true;
}

void WorkerThread::WakeUp() {
  // Signalling an event can deschedule the current thread. Since being
  // descheduled while holding a lock is undesirable (https://crbug.com/890978),
  // assert that no lock is held by the current thread.
  CheckedLock::AssertNoLockHeldOnCurrentThread();
  // Calling WakeUp() after Cleanup() or Join() is wrong because the
  // WorkerThread cannot run more tasks.
  DCHECK(!join_called_for_testing_.IsSet());
  DCHECK(!should_exit_.IsSet());
  wake_up_event_.Signal();
}

void WorkerThread::JoinForTesting() {
  DCHECK(!join_called_for_testing_.IsSet());
  join_called_for_testing_.Set();
  wake_up_event_.Signal();

  PlatformThreadHandle thread_handle;

  {
    CheckedAutoLock auto_lock(thread_lock_);

    if (thread_handle_.is_null())
      return;

    thread_handle = thread_handle_;
    // Reset |thread_handle_| so it isn't joined by the destructor.
    thread_handle_ = PlatformThreadHandle();
  }

  PlatformThread::Join(thread_handle);
}

bool WorkerThread::ThreadAliveForTesting() const {
  CheckedAutoLock auto_lock(thread_lock_);
  return !thread_handle_.is_null();
}

WorkerThread::~WorkerThread() {
  CheckedAutoLock auto_lock(thread_lock_);

  // If |thread_handle_| wasn't joined, detach it.
  if (!thread_handle_.is_null()) {
    DCHECK(!join_called_for_testing_.IsSet());
    PlatformThread::Detach(thread_handle_);
  }
}

void WorkerThread::Cleanup() {
  DCHECK(!should_exit_.IsSet());
  should_exit_.Set();
  wake_up_event_.Signal();
}

void WorkerThread::BeginUnusedPeriod() {
  CheckedAutoLock auto_lock(thread_lock_);
  DCHECK(last_used_time_.is_null());
  last_used_time_ = subtle::TimeTicksNowIgnoringOverride();
}

void WorkerThread::EndUnusedPeriod() {
  CheckedAutoLock auto_lock(thread_lock_);
  DCHECK(!last_used_time_.is_null());
  last_used_time_ = TimeTicks();
}

TimeTicks WorkerThread::GetLastUsedTime() const {
  CheckedAutoLock auto_lock(thread_lock_);
  return last_used_time_;
}

bool WorkerThread::ShouldExit() const {
  // The ordering of the checks is important below. This WorkerThread may be
  // released and outlive |task_tracker_| in unit tests. However, when the
  // WorkerThread is released, |should_exit_| will be set, so check that
  // first.
  return should_exit_.IsSet() || join_called_for_testing_.IsSet() ||
         task_tracker_->IsShutdownComplete();
}

ThreadPriority WorkerThread::GetDesiredThreadPriority() const {
  // To avoid shutdown hangs, disallow a priority below NORMAL during shutdown
  if (task_tracker_->HasShutdownStarted())
    return ThreadPriority::NORMAL;

  return priority_hint_;
}

void WorkerThread::UpdateThreadPriority(
    ThreadPriority desired_thread_priority) {
  if (desired_thread_priority == current_thread_priority_)
    return;

  PlatformThread::SetCurrentThreadPriority(desired_thread_priority);
  current_thread_priority_ = desired_thread_priority;
}

void WorkerThread::ThreadMain() {
  if (priority_hint_ == ThreadPriority::BACKGROUND) {
    switch (delegate_->GetThreadLabel()) {
      case ThreadLabel::POOLED:
        RunBackgroundPooledWorker();
        return;
      case ThreadLabel::SHARED:
        RunBackgroundSharedWorker();
        return;
      case ThreadLabel::DEDICATED:
        RunBackgroundDedicatedWorker();
        return;
#if defined(OS_WIN)
      case ThreadLabel::SHARED_COM:
        RunBackgroundSharedCOMWorker();
        return;
      case ThreadLabel::DEDICATED_COM:
        RunBackgroundDedicatedCOMWorker();
        return;
#endif  // defined(OS_WIN)
    }
  }

  switch (delegate_->GetThreadLabel()) {
    case ThreadLabel::POOLED:
      RunPooledWorker();
      return;
    case ThreadLabel::SHARED:
      RunSharedWorker();
      return;
    case ThreadLabel::DEDICATED:
      RunDedicatedWorker();
      return;
#if defined(OS_WIN)
    case ThreadLabel::SHARED_COM:
      RunSharedCOMWorker();
      return;
    case ThreadLabel::DEDICATED_COM:
      RunDedicatedCOMWorker();
      return;
#endif  // defined(OS_WIN)
  }
}

NOINLINE void WorkerThread::RunPooledWorker() {
  RunWorker();
  NO_CODE_FOLDING();
}

NOINLINE void WorkerThread::RunBackgroundPooledWorker() {
  RunWorker();
  NO_CODE_FOLDING();
}

NOINLINE void WorkerThread::RunSharedWorker() {
  RunWorker();
  NO_CODE_FOLDING();
}

NOINLINE void WorkerThread::RunBackgroundSharedWorker() {
  RunWorker();
  NO_CODE_FOLDING();
}

NOINLINE void WorkerThread::RunDedicatedWorker() {
  RunWorker();
  NO_CODE_FOLDING();
}

NOINLINE void WorkerThread::RunBackgroundDedicatedWorker() {
  RunWorker();
  NO_CODE_FOLDING();
}

#if defined(OS_WIN)
NOINLINE void WorkerThread::RunSharedCOMWorker() {
  RunWorker();
  NO_CODE_FOLDING();
}

NOINLINE void WorkerThread::RunBackgroundSharedCOMWorker() {
  RunWorker();
  NO_CODE_FOLDING();
}

NOINLINE void WorkerThread::RunDedicatedCOMWorker() {
  RunWorker();
  NO_CODE_FOLDING();
}

NOINLINE void WorkerThread::RunBackgroundDedicatedCOMWorker() {
  RunWorker();
  NO_CODE_FOLDING();
}
#endif  // defined(OS_WIN)

void WorkerThread::RunWorker() {
  DCHECK_EQ(self_, this);
  TRACE_EVENT_INSTANT0("base", "WorkerThread born", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_BEGIN0("base", "WorkerThread active");

  if (worker_thread_observer_)
    worker_thread_observer_->OnWorkerThreadMainEntry();

  delegate_->OnMainEntry(this);

  // Background threads can take an arbitrary amount of time to complete, do not
  // watch them for hangs. Ignore priority boosting for now.
  const bool watch_for_hangs =
      base::HangWatcher::IsThreadPoolHangWatchingEnabled() &&
      GetDesiredThreadPriority() != ThreadPriority::BACKGROUND;

  // If this process has a HangWatcher register this thread for watching.
  base::ScopedClosureRunner unregister_for_hang_watching;
  if (watch_for_hangs) {
    unregister_for_hang_watching = base::HangWatcher::RegisterThread(
        base::HangWatcher::ThreadType::kThreadPoolThread);
  }

  // A WorkerThread starts out waiting for work.
  {
    TRACE_EVENT_END0("base", "WorkerThread active");
    delegate_->WaitForWork(&wake_up_event_);
    TRACE_EVENT_BEGIN0("base", "WorkerThread active");
  }

  while (!ShouldExit()) {
#if defined(OS_APPLE)
    mac::ScopedNSAutoreleasePool autorelease_pool;
#endif
    absl::optional<WatchHangsInScope> hang_watch_scope;
    if (watch_for_hangs)
      hang_watch_scope.emplace(base::WatchHangsInScope::kDefaultHangWatchTime);

    UpdateThreadPriority(GetDesiredThreadPriority());

    // Get the task source containing the next task to execute.
    RegisteredTaskSource task_source = delegate_->GetWork(this);
    if (!task_source) {
      // Exit immediately if GetWork() resulted in detaching this worker.
      if (ShouldExit())
        break;

      TRACE_EVENT_END0("base", "WorkerThread active");
      hang_watch_scope.reset();
      delegate_->WaitForWork(&wake_up_event_);
      TRACE_EVENT_BEGIN0("base", "WorkerThread active");
      continue;
    }

    // Alias pointer for investigation of memory corruption. crbug.com/1218384
    TaskSource* task_source_before_run = task_source.get();
    base::debug::Alias(&task_source_before_run);

    task_source = task_tracker_->RunAndPopNextTask(std::move(task_source));

    // Alias pointer for investigation of memory corruption. crbug.com/1218384
    TaskSource* task_source_before_move = task_source.get();
    base::debug::Alias(&task_source_before_move);

    delegate_->DidProcessTask(std::move(task_source));

    // Check that task_source is always cleared, to help investigation of memory
    // corruption where task_source is non-null after being moved.
    // crbug.com/1218384
    CHECK(!task_source);

    // Calling WakeUp() guarantees that this WorkerThread will run Tasks from
    // TaskSources returned by the GetWork() method of |delegate_| until it
    // returns nullptr. Resetting |wake_up_event_| here doesn't break this
    // invariant and avoids a useless loop iteration before going to sleep if
    // WakeUp() is called while this WorkerThread is awake.
    wake_up_event_.Reset();
  }

  // Important: It is unsafe to access unowned state (e.g. |task_tracker_|)
  // after invoking OnMainExit().

  delegate_->OnMainExit(this);

  if (worker_thread_observer_)
    worker_thread_observer_->OnWorkerThreadMainExit();

  // Release the self-reference to |this|. This can result in deleting |this|
  // and as such no more member accesses should be made after this point.
  self_ = nullptr;

  TRACE_EVENT_END0("base", "WorkerThread active");
  TRACE_EVENT_INSTANT0("base", "WorkerThread dead", TRACE_EVENT_SCOPE_THREAD);
}

}  // namespace internal
}  // namespace base
