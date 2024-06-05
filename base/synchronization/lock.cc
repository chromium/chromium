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
thread_local std::array<uintptr_t, kHeldLocksCapacity> g_locks_held_by_thread;

// Number of non-nullptr elements in `g_locks_held_by_thread`.
thread_local size_t g_num_locks_held_by_thread = 0;

// Whether a lock is added to `g_locks_held_by_thread` when acquired.
thread_local bool g_track_locks = true;

}  // namespace

Lock::~Lock() {
  DCHECK(owning_thread_ref_.is_null());
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

  // Remove from the list of held locks.
  for (size_t i = 0; i < g_num_locks_held_by_thread; ++i) {
    // Traverse from the end since locks are typically acquired and released in
    // opposite order.
    const size_t index = g_num_locks_held_by_thread - i - 1;
    if (g_locks_held_by_thread[index] == reinterpret_cast<uintptr_t>(this)) {
      g_locks_held_by_thread[index] =
          g_locks_held_by_thread[g_num_locks_held_by_thread - 1];
      g_locks_held_by_thread[g_num_locks_held_by_thread - 1] =
          reinterpret_cast<uintptr_t>(nullptr);
      --g_num_locks_held_by_thread;
      break;
    }
  }
}

void Lock::CheckUnheldAndMark() {
  DCHECK(owning_thread_ref_.is_null());
  owning_thread_ref_ = PlatformThread::CurrentRef();

  if (g_track_locks) {
    // Check if capacity is exceeded.
    if (g_num_locks_held_by_thread >= kHeldLocksCapacity) {
      // Disable tracking of locks since logging may acquire a lock.
      subtle::DoNotTrackLocks do_not_track_locks;
      CHECK(false)
          << "This thread holds more than " << kHeldLocksCapacity
          << " locks simultaneously. Reach out to //base OWNERS to determine "
             "whether it's preferable to increase `kHeldLocksCapacity` or to "
             "use `DoNotTrackLocks` in this scope.";
    }

    // Add to the list of held locks.
    g_locks_held_by_thread[g_num_locks_held_by_thread] =
        reinterpret_cast<uintptr_t>(this);
    ++g_num_locks_held_by_thread;
  }
}

namespace subtle {

span<const uintptr_t> GetLocksHeldByCurrentThread() {
  return span<const uintptr_t>(g_locks_held_by_thread.begin(),
                               g_num_locks_held_by_thread);
}

DoNotTrackLocks::DoNotTrackLocks() : auto_reset_(&g_track_locks, false) {}

DoNotTrackLocks::~DoNotTrackLocks() = default;

}  // namespace subtle

}  // namespace base

#endif  // DCHECK_IS_ON()
