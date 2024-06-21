// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/synchronization/lock.h"

#include <cstdint>

#if DCHECK_IS_ON()
#include <array>

#include "base/synchronization/lock_subtle.h"
#include "base/threading/platform_thread.h"

namespace base {

namespace {

// List of locks held by a thread.
//
// As of May 2024, no more than 5 locks were held simultaneously by a thread in
// a test browsing session or while running the CQ (% locks acquired in unit
// tests "WaitSetTest.NoStarvation" and
// "MessagePipeTest.DataPipeConsumerHandlePingPong"). An array of size 10 is
// therefore considered sufficient to track all locks held by a thread. A
// dynamic-size array (e.g. owned by a `ThreadLocalOwnedPointer`) would require
// handling reentrancy issues with allocator shims that use `base::Lock`.
constexpr int kHeldLocksCapacity = 10;
thread_local std::array<uintptr_t, kHeldLocksCapacity>
    g_tracked_locks_held_by_thread;

// Number of non-nullptr elements in `g_tracked_locks_held_by_thread`.
thread_local size_t g_num_tracked_locks_held_by_thread = 0;

}  // namespace

Lock::~Lock() {
  DCHECK(owning_thread_ref_.is_null());
}

void Lock::Acquire(subtle::LockTracking tracking) {
  lock_.Lock();
  if (tracking == subtle::LockTracking::kEnabled) {
    AddToLocksHeldOnCurrentThread();
  }
  CheckUnheldAndMark();
}

void Lock::Release() {
  CheckHeldAndUnmark();
  if (in_tracked_locks_held_by_current_thread_) {
    RemoveFromLocksHeldOnCurrentThread();
  }
  lock_.Unlock();
}

bool Lock::Try(subtle::LockTracking tracking) {
  const bool rv = lock_.Try();
  if (rv) {
    if (tracking == subtle::LockTracking::kEnabled) {
      AddToLocksHeldOnCurrentThread();
    }
    CheckUnheldAndMark();
  }
  return rv;
}

void Lock::AssertAcquired() const {
  DCHECK_EQ(owning_thread_ref_, PlatformThread::CurrentRef());
}

void Lock::AssertNotHeld() const {
  DCHECK(owning_thread_ref_.is_null());
}

void Lock::CheckHeldAndUnmark() {
  DCHECK_EQ(owning_thread_ref_, PlatformThread::CurrentRef());
  owning_thread_ref_ = PlatformThreadRef();
}

void Lock::CheckUnheldAndMark() {
  DCHECK(owning_thread_ref_.is_null());
  owning_thread_ref_ = PlatformThread::CurrentRef();
}

void Lock::AddToLocksHeldOnCurrentThread() {
  CHECK(!in_tracked_locks_held_by_current_thread_);

  // Check if capacity is exceeded.
  if (g_num_tracked_locks_held_by_thread >= kHeldLocksCapacity) {
    CHECK(false)
        << "This thread holds more than " << kHeldLocksCapacity
        << " tracked locks simultaneously. Reach out to //base OWNERS to "
           "determine whether `kHeldLocksCapacity` should be increased.";
  }

  // Add to the list of held locks.
  g_tracked_locks_held_by_thread[g_num_tracked_locks_held_by_thread] =
      reinterpret_cast<uintptr_t>(this);
  ++g_num_tracked_locks_held_by_thread;
  in_tracked_locks_held_by_current_thread_ = true;
}

void Lock::RemoveFromLocksHeldOnCurrentThread() {
  CHECK(in_tracked_locks_held_by_current_thread_);
  for (size_t i = 0; i < g_num_tracked_locks_held_by_thread; ++i) {
    // Traverse from the end since locks are typically acquired and released in
    // opposite order.
    const size_t index = g_num_tracked_locks_held_by_thread - i - 1;
    if (g_tracked_locks_held_by_thread[index] ==
        reinterpret_cast<uintptr_t>(this)) {
      g_tracked_locks_held_by_thread[index] =
          g_tracked_locks_held_by_thread[g_num_tracked_locks_held_by_thread -
                                         1];
      g_tracked_locks_held_by_thread[g_num_tracked_locks_held_by_thread - 1] =
          reinterpret_cast<uintptr_t>(nullptr);
      --g_num_tracked_locks_held_by_thread;
      break;
    }
  }
  in_tracked_locks_held_by_current_thread_ = false;
}

namespace subtle {

span<const uintptr_t> GetTrackedLocksHeldByCurrentThread() {
  return span<const uintptr_t>(g_tracked_locks_held_by_thread.begin(),
                               g_num_tracked_locks_held_by_thread);
}

}  // namespace subtle

}  // namespace base

#endif  // DCHECK_IS_ON()
