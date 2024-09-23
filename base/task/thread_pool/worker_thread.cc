// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/worker_thread.h"

#include <stddef.h>

#include <algorithm>
#include <atomic>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/worker_thread_observer.h"
#include "base/threading/hang_watcher.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_config.h"

#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED)
#include "partition_alloc/thread_cache.h"
#endif

namespace base::internal {

constexpr TimeDelta WorkerThread::Delegate::kPurgeThreadCacheIdleDelay;

WorkerThread::ThreadLabel WorkerThread::Delegate::GetThreadLabel() const {
  return WorkerThread::ThreadLabel::POOLED;
}

bool WorkerThread::Delegate::TimedWait(TimeDelta timeout) {
  return wake_up_event_.TimedWait(timeout);
}

void WorkerThread::Delegate::WaitForWork() {
  const TimeDelta sleep_duration_before_worker_reclaim = GetSleepTimeout();

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
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED)
  const TimeDelta sleep_duration_before_purge =
      GetSleepDurationBeforePurge(base::TimeTicks::Now());

  const bool was_signaled = TimedWait(std::min(
      sleep_duration_before_purge, sleep_duration_before_worker_reclaim));
  // Timed out.
  if (!was_signaled) {
    partition_alloc::ThreadCache::PurgeCurrentThread();

    // The thread woke up to purge before its standard reclaim time. Sleep for
    // what's remaining until then.
    if (sleep_duration_before_worker_reclaim > sleep_duration_before_purge) {
      TimedWait(sleep_duration_before_worker_reclaim.is_max()
                    ? TimeDelta::Max()
                    : sleep_duration_before_worker_reclaim -
                          sleep_duration_before_purge);
    }
  }
#else
  TimedWait(sleep_duration_before_worker_reclaim);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED)
}

bool WorkerThread::Delegate::IsDelayFirstWorkerSleepEnabled() {
  static bool state = FeatureList::IsEnabled(kDelayFirstWorkerWake);
  return state;
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED)
TimeDelta WorkerThread::Delegate::GetSleepDurationBeforePurge(TimeTicks now) {
  base::TimeDelta sleep_duration_before_purge = kPurgeThreadCacheIdleDelay;

  if (!IsDelayFirstWorkerSleepEnabled()) {
    return sleep_duration_before_purge;
  }

  // Use the first time a worker goes to sleep in this process as an
  // approximation of the process creation time.
  static const TimeTicks first_sleep_time = now;
  const TimeTicks first_sleep_time_to_use =
      !first_sleep_time_for_testing_.is_null() ? first_sleep_time_for_testing_
                                               : first_sleep_time;
  const base::TimeTicks first_wake_time =
      first_sleep_time_to_use + kFirstSleepDurationBeforePurge;

  // A sleep that occurs within `kFirstSleepDurationBeforePurge` of the
  // first sleep lasts at least `kFirstSleepDurationBeforePurge`.
  if (now <= first_wake_time) {
    // Avoid sleeping for less than `sleep_duration_before_purge` since that is
    // the shortest expected duration to wait for a purge.
    sleep_duration_before_purge =
        std::max(kFirstSleepDurationBeforePurge, sleep_duration_before_purge);
  }

  // Align wakeups for purges to reduce the chances of taking the CPU out of
  // sleep multiple times for these operations. This can happen if many workers
  // in the same process scheduled wakeups. This can create a situation where
  // any one worker wakes every `kPurgeThreadCacheIdleDelay` / N where N is the
  // number of workers.
  const TimeTicks snapped_purge_time =
      (now + sleep_duration_before_purge)
          .SnappedToNextTick(TimeTicks(), kPurgeThreadCacheIdleDelay);

  return snapped_purge_time - now;
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED)

WorkerThread::WorkerThread(ThreadType thread_type_hint,
                           std::unique_ptr<Delegate> delegate,
                           TrackedRef<TaskTracker> task_tracker,
                           size_t sequence_num,
                           const CheckedLock* predecessor_lock,
                           void* flow_terminator)
    : thread_lock_(predecessor_lock),
      task_tracker_(std::move(task_tracker)),
      thread_type_hint_(thread_type_hint),
      current_thread_type_(GetDesiredThreadType()),
      sequence_num_(sequence_num),
      flow_terminator_(flow_terminator == nullptr
                           ? reinterpret_cast<intptr_t>(this)
                           : reinterpret_cast<intptr_t>(flow_terminator)),
      delegate_(std::move(delegate)) {
  DCHECK(task_tracker_);
  DCHECK(CanUseBackgroundThreadTypeForWorkerThread() ||
         thread_type_hint_ != ThreadType::kBackground);
  DCHECK(CanUseUtilityThreadTypeForWorkerThread() ||
         thread_type_hint != ThreadType::kUtility);
  DCHECK(delegate_);
  delegate_->wake_up_event_.declare_only_used_while_idle();
}

bool WorkerThread::Start(
    scoped_refptr<SingleThreadTaskRunner> io_thread_task_runner,
    WorkerThreadObserver* worker_thread_observer) {
  CheckedLock::AssertNoLockHeldOnCurrentThread();

  // Prime kDelayFirstWorkerWake's feature state right away on thread creation
  // instead of looking it up for the first time later on thread as this avoids
  // a data race in tests that may ~FeatureList while the first worker thread
  // is still initializing (the first WorkerThread will be started on the main
  // thread as part of ThreadPoolImpl::Start() so doing it then avoids this
  // race), crbug.com/1344573.
  // Note 1: the feature state is always available at this point as
  // ThreadPoolInstance::Start() contractually happens-after FeatureList
  // initialization.
  // Note 2: This is done on Start instead of in the constructor as construction
  // happens under a ThreadGroup lock which precludes calling into
  // FeatureList (as that can also use a lock).
  delegate()->IsDelayFirstWorkerSleepEnabled();

  CheckedAutoLock auto_lock(thread_lock_);
  DCHECK(thread_handle_.is_null());

#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
  DCHECK(io_thread_task_runner);
  io_thread_task_runner_ = std::move(io_thread_task_runner);
#endif

  if (should_exit_.IsSet() || join_called_for_testing_.IsSet()) {
    return true;
  }

  DCHECK(!worker_thread_observer_);
  worker_thread_observer_ = worker_thread_observer;

  self_ = this;

  constexpr size_t kDefaultStackSize = 0;
  PlatformThread::CreateWithType(kDefaultStackSize, this, &thread_handle_,
                                 current_thread_type_);

  if (thread_handle_.is_null()) {
    self_ = nullptr;
    return false;
  }

  return true;
}

void WorkerThread::Destroy() {
  CheckedAutoLock auto_lock(thread_lock_);

  // If |thread_handle_| wasn't joined, detach it.
  if (!thread_handle_.is_null()) {
    PlatformThread::Detach(thread_handle_);
  }
}

bool WorkerThread::ThreadAliveForTesting() const {
  CheckedAutoLock auto_lock(thread_lock_);
  return !thread_handle_.is_null();
}

void WorkerThread::JoinForTesting() {
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

void WorkerThread::Cleanup() {
  DCHECK(!should_exit_.IsSet());
  should_exit_.Set();
  delegate_->wake_up_event_.Signal();
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
  TRACE_EVENT_INSTANT("wakeup.flow", "WorkerThread::WakeUp",
                      perfetto::Flow::FromPointer(this));

  delegate_->wake_up_event_.Signal();
}

WorkerThread::Delegate* WorkerThread::delegate() {
  return delegate_.get();
}

WorkerThread::~WorkerThread() {
  Destroy();
}

void WorkerThread::MaybeUpdateThreadType() {
  UpdateThreadType(GetDesiredThreadType());
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

ThreadType WorkerThread::GetDesiredThreadType() const {
  // To avoid shutdown hangs, disallow a type below kNormal during shutdown
  if (task_tracker_->HasShutdownStarted())
    return ThreadType::kDefault;

  return thread_type_hint_;
}

void WorkerThread::UpdateThreadType(ThreadType desired_thread_type) {
  if (desired_thread_type == current_thread_type_)
    return;

  PlatformThread::SetCurrentThreadType(desired_thread_type);
  current_thread_type_ = desired_thread_type;
}

void WorkerThread::ThreadMain() {
#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)) || BUILDFLAG(IS_FUCHSIA)
  DCHECK(io_thread_task_runner_);
  FileDescriptorWatcher file_descriptor_watcher(io_thread_task_runner_);
#endif

  if (thread_type_hint_ == ThreadType::kBackground) {
    switch (delegate()->GetThreadLabel()) {
      case ThreadLabel::POOLED:
        RunBackgroundPooledWorker();
        return;
      case ThreadLabel::SHARED:
        RunBackgroundSharedWorker();
        return;
      case ThreadLabel::DEDICATED:
        RunBackgroundDedicatedWorker();
        return;
#if BUILDFLAG(IS_WIN)
      case ThreadLabel::SHARED_COM:
        RunBackgroundSharedCOMWorker();
        return;
      case ThreadLabel::DEDICATED_COM:
        RunBackgroundDedicatedCOMWorker();
        return;
#endif  // BUILDFLAG(IS_WIN)
    }
  }

  switch (delegate()->GetThreadLabel()) {
    case ThreadLabel::POOLED:
      RunPooledWorker();
      return;
    case ThreadLabel::SHARED:
      RunSharedWorker();
      return;
    case ThreadLabel::DEDICATED:
      RunDedicatedWorker();
      return;
#if BUILDFLAG(IS_WIN)
    case ThreadLabel::SHARED_COM:
      RunSharedCOMWorker();
      return;
    case ThreadLabel::DEDICATED_COM:
      RunDedicatedCOMWorker();
      return;
#endif  // BUILDFLAG(IS_WIN)
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

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)

void WorkerThread::RunWorker() {
  DCHECK_EQ(self_, this);
  TRACE_EVENT_INSTANT0("base", "WorkerThread born", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_BEGIN0("base", "WorkerThread active");

  if (worker_thread_observer_) {
    worker_thread_observer_->OnWorkerThreadMainEntry();
  }

  delegate()->OnMainEntry(this);

  // Background threads can take an arbitrary amount of time to complete, do not
  // watch them for hangs. Ignore priority boosting for now.
  const bool watch_for_hangs =
      base::HangWatcher::IsThreadPoolHangWatchingEnabled() &&
      GetDesiredThreadType() != ThreadType::kBackground;

  // If this process has a HangWatcher register this thread for watching.
  base::ScopedClosureRunner unregister_for_hang_watching;
  if (watch_for_hangs) {
    unregister_for_hang_watching = base::HangWatcher::RegisterThread(
        base::HangWatcher::ThreadType::kThreadPoolThread);
  }

  while (!ShouldExit()) {
#if BUILDFLAG(IS_APPLE)
    apple::ScopedNSAutoreleasePool autorelease_pool;
#endif
    std::optional<WatchHangsInScope> hang_watch_scope;

    TRACE_EVENT_END0("base", "WorkerThread active");
    hang_watch_scope.reset();
    delegate()->WaitForWork();
    TRACE_EVENT_BEGIN("base", "WorkerThread active",
                      perfetto::TerminatingFlow::FromPointer(
                          reinterpret_cast<void*>(flow_terminator_)));

    // Don't GetWork() in the case where we woke up for Cleanup().
    if (ShouldExit()) {
      break;
    }

    if (watch_for_hangs) {
      hang_watch_scope.emplace();
    }

    // Thread type needs to be updated before GetWork.
    UpdateThreadType(GetDesiredThreadType());

    // Get the task source containing the first task to execute.
    RegisteredTaskSource task_source = delegate()->GetWork(this);

    // If acquiring work failed and the worker's still alive,
    // record that this is an unnecessary wakeup.
    if (!task_source && !ShouldExit()) {
      delegate()->RecordUnnecessaryWakeup();
    }

    while (task_source) {
      // Alias pointer for investigation of memory corruption. crbug.com/1218384
      TaskSource* task_source_before_run = task_source.get();
      base::debug::Alias(&task_source_before_run);

      task_source = task_tracker_->RunAndPopNextTask(std::move(task_source));
      // Alias pointer for investigation of memory corruption. crbug.com/1218384
      TaskSource* task_source_before_move = task_source.get();
      base::debug::Alias(&task_source_before_move);

      // We emplace the hang_watch_scope here so that each hang watch scope
      // covers one GetWork (or SwapProcessedTask) as well as one
      // RunAndPopNextTask.
      if (watch_for_hangs) {
        hang_watch_scope.emplace();
      }

      RegisteredTaskSource new_task_source =
          delegate()->SwapProcessedTask(std::move(task_source), this);

      UpdateThreadType(GetDesiredThreadType());

      // Check that task_source is always cleared, to help investigation of
      // memory corruption where task_source is non-null after being moved.
      // crbug.com/1218384
      CHECK(!task_source);
      task_source = std::move(new_task_source);
    }
  }

  // Important: It is unsafe to access unowned state (e.g. |task_tracker_|)
  // after invoking OnMainExit().

  delegate()->OnMainExit(this);

  if (worker_thread_observer_) {
    worker_thread_observer_->OnWorkerThreadMainExit();
  }

  // Release the self-reference to |this|. This can result in deleting |this|
  // and as such no more member accesses should be made after this point.
  self_ = nullptr;

  TRACE_EVENT_END0("base", "WorkerThread active");
  TRACE_EVENT_INSTANT0("base", "WorkerThread dead", TRACE_EVENT_SCOPE_THREAD);
}

}  // namespace base::internal
