// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_tracker.h"

#include "base/check.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/threading/thread_restrictions.h"

namespace base::sequence_manager::internal {

SyncWorkAuthorization::SyncWorkAuthorization(SyncWorkAuthorization&& other)
    : tracker_(other.tracker_) {
  other.tracker_ = nullptr;
}

SyncWorkAuthorization& SyncWorkAuthorization::operator=(
    SyncWorkAuthorization&& other) {
  tracker_ = other.tracker_;
  other.tracker_ = nullptr;
  return *this;
}

SyncWorkAuthorization::~SyncWorkAuthorization() {
  if (!tracker_) {
    return;
  }

  {
    base::internal::CheckedAutoLock auto_lock(tracker_->active_sync_work_lock_);
    uint32_t prev = tracker_->state_.fetch_and(
        ~WorkTracker::kActiveSyncWork, WorkTracker::kMemoryReleaseAllowWork);
    DCHECK(prev & WorkTracker::kActiveSyncWork);
  }

  tracker_->active_sync_work_cv_.Signal();
}

SyncWorkAuthorization::SyncWorkAuthorization(WorkTracker* state)
    : tracker_(state) {}

WorkTracker::WorkTracker() {
  DETACH_FROM_THREAD(thread_checker_);
}

WorkTracker::~WorkTracker() = default;

void WorkTracker::SetRunTaskSynchronouslyAllowed(
    bool can_run_tasks_synchronously) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (can_run_tasks_synchronously) {
    state_.fetch_or(kSyncWorkSupported, kMemoryReleaseAllowWork);
  } else {
    // After this returns, non-sync work may run without being tracked by
    // `this`. Ensures that such work is correctly sequenced with sync work by:
    //  - Waiting until sync work is complete.
    //  - Acquiring memory written by sync work (`kMemoryAcquireBeforeWork` here
    //    is paired with `kMemoryReleaseAllowWork` in `~SyncWorkAuthorization`).
    uint32_t prev =
        state_.fetch_and(~kSyncWorkSupported, kMemoryAcquireBeforeWork);
    if (prev & kActiveSyncWork) {
      WaitNoSyncWork();
    }
  }
}

void WorkTracker::WaitNoSyncWork() {
  // Do not process new PostTasks, defer them. Tracing can call PostTask, but
  // it will try to grab locks that are not allowed here.
  ScopedDeferTaskPosting disallow_task_posting;
  ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow;
  // `std::memory_order_relaxed` instead of `kMemoryAcquireBeforeWork` because
  // the lock implicitly acquires memory released by `~SyncWorkAuthorization`.
  base::internal::CheckedAutoLock auto_lock(active_sync_work_lock_);
  uint32_t prev = state_.load(std::memory_order_relaxed);
  while (prev & kActiveSyncWork) {
    active_sync_work_cv_.Wait();
    prev = state_.load(std::memory_order_relaxed);
  }
}

void WorkTracker::WillRequestReloadImmediateWorkQueue() {
  // May be called from any thread.

  // Sync work is disallowed until `WillReloadImmediateWorkQueues()` and
  // `OnIdle()` are called.
  state_.fetch_or(kImmediateWorkQueueNeedsReload,
                  kMemoryRelaxedNotAllowOrBeforeWork);
}

void WorkTracker::WillReloadImmediateWorkQueues() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Sync work is disallowed until `OnIdle()` is called.
  state_.fetch_and(
      ~(kImmediateWorkQueueNeedsReload | kWorkQueuesEmptyAndNoWorkRunning),
      kMemoryRelaxedNotAllowOrBeforeWork);
}

void WorkTracker::OnBeginWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t prev = state_.fetch_and(~kWorkQueuesEmptyAndNoWorkRunning,
                                   kMemoryAcquireBeforeWork);
  if (prev & kActiveSyncWork) {
    DCHECK(prev & kSyncWorkSupported);
    WaitNoSyncWork();
  }
}

void WorkTracker::OnIdle() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This may allow sync work. "release" so that sync work that runs after this
  // sees all writes issued by previous sequenced work.
  state_.fetch_or(kWorkQueuesEmptyAndNoWorkRunning, std::memory_order_release);
}

SyncWorkAuthorization WorkTracker::TryAcquireSyncWorkAuthorization() {
  // May be called from any thread.

  uint32_t state = state_.load(std::memory_order_relaxed);
  // "acquire" so that sync work sees writes issued by sequenced work that
  // precedes it.
  if (state == (kSyncWorkSupported | kWorkQueuesEmptyAndNoWorkRunning) &&
      state_.compare_exchange_strong(state, state | kActiveSyncWork,
                                     std::memory_order_acquire,
                                     std::memory_order_relaxed)) {
    return SyncWorkAuthorization(this);
  }

  return SyncWorkAuthorization(nullptr);
}

void WorkTracker::AssertHasWork() {
  CHECK(!(state_.load(std::memory_order_relaxed) &
          kWorkQueuesEmptyAndNoWorkRunning));
}

}  // namespace base::sequence_manager::internal
