// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SPINNING_FUTEX_LINUX_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SPINNING_FUTEX_LINUX_H_

#include <atomic>

#include "base/allocator/partition_allocator/yield_processor.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if !(defined(OS_LINUX) || defined(OS_ANDROID))
#error "Not supported"
#endif

namespace base {
namespace internal {

// Simple spinning futex lock. It will spin in user space a set number of times
// before going into the kernel to sleep.
//
// This is intended to give "the best of both worlds" between a SpinLock and
// base::Lock:
// - SpinLock: Inlined fast path, no external function calls, just
//   compare-and-swap. Short waits do not go into the kernel. Good behavior in
//   low contention cases.
// - base::Lock: Good behavior in case of contention.
//
// We don't rely on base::Lock which we could make spin (by calling Try() in a
// loop), as performance is below a custom spinlock as seen on high-level
// benchmarks. Instead this implements a simple non-recursive mutex on top of
// the futex() syscall. The main difference between this and a libc
// implementation is that it only supports the simplest path: private (to a
// process), non-recursive mutexes with no priority inheritance, no timed waits.
//
// As an interesting side-effect to be used in the allocator, this code does not
// make any allocations, locks are small with a constexpr constructor and no
// destructor.
class BASE_EXPORT SpinningFutex {
 public:
  inline constexpr SpinningFutex();
  ALWAYS_INLINE void Acquire();
  ALWAYS_INLINE void Release();
  void AssertAcquired() const {}  // Not supported.

 private:
  void LockSlow();
  void FutexWait();
  void FutexWake();

  static constexpr int kUnlocked = 0;
  static constexpr int kLockedUncontended = 1;
  static constexpr int kLockedContended = 2;

  // Same as SpinLock, not scientifically calibrated.
  static constexpr int kSpinCount = 10;

  std::atomic<int32_t> state_{kUnlocked};
};

ALWAYS_INLINE void SpinningFutex::Acquire() {
  int tries = 0;
  // Busy-waiting is inlined, which is fine as long as we have few callers. This
  // is only used for the partition lock, so this is the case.
  do {
    int expected = kUnlocked;
    if (LIKELY(state_.compare_exchange_strong(expected, kLockedUncontended,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed))) {
      return;
    }
    YIELD_PROCESSOR;
    tries++;
  } while (tries < kSpinCount);

  LockSlow();
}

inline constexpr SpinningFutex::SpinningFutex() = default;

ALWAYS_INLINE void SpinningFutex::Release() {
  if (UNLIKELY(state_.exchange(kUnlocked, std::memory_order_release) ==
               kLockedContended)) {
    // |kLockedContended|: there is a waiter to wake up.
    //
    // Here there is a window where the lock is unlocked, since we just set it
    // to |kUnlocked| above. Meaning that another thread can grab the lock
    // in-between now and |FutexWake()| waking up a waiter. Aside from
    // potentially fairness, this is not an issue, as the newly-awaken thread
    // will check that the lock is still free.
    //
    // There is a small pessimization here though: if we have a single waiter,
    // then when it wakes up, the lock will be set to |kLockedContended|, so
    // when this waiter releases the lock, it will needlessly call
    // |FutexWake()|, even though there are no waiters. This is supported by the
    // kernel, and is what bionic (Android's libc) also does.
    FutexWake();
  }
}

}  // namespace internal
}  // namespace base
#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SPINNING_FUTEX_LINUX_H_
