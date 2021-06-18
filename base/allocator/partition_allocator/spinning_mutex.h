// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SPINNING_MUTEX_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SPINNING_MUTEX_H_

#include <algorithm>
#include <atomic>

#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/yield_processor.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/windows_types.h"
#endif

#if defined(PA_HAS_SPINNING_MUTEX)
namespace base {
namespace internal {

// Simple spinning lock. It will spin in user space a set number of times before
// going into the kernel to sleep.
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
// the futex() syscall on Linux, and SRWLock on Windows. The main difference
// between this and a libc implementation is that it only supports the simplest
// path: private (to a process), non-recursive mutexes with no priority
// inheritance, no timed waits.
//
// As an interesting side-effect to be used in the allocator, this code does not
// make any allocations, locks are small with a constexpr constructor and no
// destructor.
class LOCKABLE BASE_EXPORT SpinningMutex {
 public:
  inline constexpr SpinningMutex();
  ALWAYS_INLINE void Acquire() EXCLUSIVE_LOCK_FUNCTION();
  ALWAYS_INLINE void Release() UNLOCK_FUNCTION();
  ALWAYS_INLINE bool Try() EXCLUSIVE_TRYLOCK_FUNCTION(true);
  void AssertAcquired() const {}  // Not supported.

 private:
  void LockSlow();

  // Same as SpinLock, not scientifically calibrated. Consider lowering later,
  // as the slow path has better characteristics than SpinLocks's.
  static constexpr int kSpinCount = 1000;

#if defined(PA_HAS_LINUX_KERNEL)
  void FutexWait();
  void FutexWake();

  static constexpr int kUnlocked = 0;
  static constexpr int kLockedUncontended = 1;
  static constexpr int kLockedContended = 2;

  std::atomic<int32_t> state_{kUnlocked};
#else
  CHROME_SRWLOCK lock_ = SRWLOCK_INIT;
#endif
};

ALWAYS_INLINE void SpinningMutex::Acquire() {
  int tries = 0;
  int backoff = 1;
  // Busy-waiting is inlined, which is fine as long as we have few callers. This
  // is only used for the partition lock, so this is the case.
  do {
    if (LIKELY(Try()))
      return;
    // Note: Per the intel optimization manual
    // (https://software.intel.com/content/dam/develop/public/us/en/documents/64-ia-32-architectures-optimization-manual.pdf),
    // the "pause" instruction is more costly on Skylake Client than on previous
    // (and subsequent?) architectures. The latency is found to be 141 cycles
    // there. This is not a big issue here as we don't spin long enough for this
    // to become a problem, as we spend a maximum of ~141k cycles ~= 47us at
    // 3GHz in "pause".
    //
    // Also, loop several times here, following the guidelines in section 2.3.4
    // of the manual, "Pause latency in Skylake Client Microarchitecture".
    for (int yields = 0; yields < backoff; yields++) {
      YIELD_PROCESSOR;
      tries++;
    }
    constexpr int kMaxBackoff = 64;
    backoff = std::min(kMaxBackoff, backoff << 1);
  } while (tries < kSpinCount);

  LockSlow();
}

inline constexpr SpinningMutex::SpinningMutex() = default;

#if defined(PA_HAS_LINUX_KERNEL)

ALWAYS_INLINE bool SpinningMutex::Try() {
  // Using the weak variant of compare_exchange(), which may fail spuriously. On
  // some architectures such as ARM, CAS is typically performed as a LDREX/STREX
  // pair, where the store may fail. In the strong version, there is a loop
  // inserted by the compiler to retry in these cases.
  //
  // Since we are retrying in Lock() anyway, there is no point having two nested
  // loops.
  int expected = kUnlocked;
  return (state_.load(std::memory_order_relaxed) == expected) &&
         state_.compare_exchange_weak(expected, kLockedUncontended,
                                      std::memory_order_acquire,
                                      std::memory_order_relaxed);
}

ALWAYS_INLINE void SpinningMutex::Release() {
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

#else

ALWAYS_INLINE bool SpinningMutex::Try() {
  return !!::TryAcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

ALWAYS_INLINE void SpinningMutex::Release() {
  ::ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

#endif

}  // namespace internal
}  // namespace base
#endif  // defined(PA_HAS_SPINNING_MUTEX)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SPINNING_MUTEX_H_
