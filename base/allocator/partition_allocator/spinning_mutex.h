// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SPINNING_MUTEX_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SPINNING_MUTEX_H_

#include <algorithm>
#include <atomic>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/yield_processor.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#include <pthread.h>
#endif

#if BUILDFLAG(IS_APPLE)

#include <os/lock.h>

// os_unfair_lock is available starting with OS X 10.12, and Chromium targets
// 10.11 at the minimum, so the symbols are not always available *at runtime*.
// But we build with a 11.x SDK, so it's always in the headers.
//
// However, since the majority of clients have at least 10.12 (released late
// 2016), we declare the symbols here, marking them weak. They will be nullptr
// on 10.11, and defined on more recent versions.

// Silence the compiler warning, here and below.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

#define PA_WEAK __attribute__((weak))

extern "C" {

PA_WEAK void os_unfair_lock_lock(os_unfair_lock_t lock);
PA_WEAK bool os_unfair_lock_trylock(os_unfair_lock_t lock);
PA_WEAK void os_unfair_lock_unlock(os_unfair_lock_t lock);
}

#pragma clang diagnostic pop

#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/sync/mutex.h>
#endif

namespace partition_alloc {

// The behavior of this class depends on whether PA_HAS_FAST_MUTEX is defined.
// 1. When it is defined:
//
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
//
// 2. Otherwise: This is a simple SpinLock, in the sense that it does not have
// any awareness of other threads' behavior. One exception: x86 macOS uses
// os_unfair_lock() if available, which is the case for macOS >= 10.12, that is
// most clients.
class LOCKABLE BASE_EXPORT SpinningMutex {
 public:
  inline constexpr SpinningMutex();
  ALWAYS_INLINE void Acquire() EXCLUSIVE_LOCK_FUNCTION();
  ALWAYS_INLINE void Release() UNLOCK_FUNCTION();
  ALWAYS_INLINE bool Try() EXCLUSIVE_TRYLOCK_FUNCTION(true);
  void AssertAcquired() const {}  // Not supported.
  void Reinit() UNLOCK_FUNCTION();

 private:
  void LockSlow() EXCLUSIVE_LOCK_FUNCTION();

  // See below, the latency of PA_YIELD_PROCESSOR can be as high as ~150
  // cycles. Meanwhile, sleeping costs a few us. Spinning 64 times at 3GHz would
  // cost 150 * 64 / 3e9 ~= 3.2us.
  //
  // This applies to Linux kernels, on x86_64. On ARM we might want to spin
  // more.
  static constexpr int kSpinCount = 64;

#if defined(PA_HAS_FAST_MUTEX)

#if defined(PA_HAS_LINUX_KERNEL)
  void FutexWait();
  void FutexWake();

  static constexpr int kUnlocked = 0;
  static constexpr int kLockedUncontended = 1;
  static constexpr int kLockedContended = 2;

  std::atomic<int32_t> state_{kUnlocked};
#elif BUILDFLAG(IS_WIN)
  CHROME_SRWLOCK lock_ = SRWLOCK_INIT;
#elif BUILDFLAG(IS_POSIX)
  pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER;
#elif BUILDFLAG(IS_FUCHSIA)
  sync_mutex lock_;
#endif

#else  // defined(PA_HAS_FAST_MUTEX)
  std::atomic<bool> lock_{false};

#if BUILDFLAG(IS_APPLE) && !defined(PA_NO_OS_UNFAIR_LOCK_CRBUG_1267256)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
  os_unfair_lock unfair_lock_ = OS_UNFAIR_LOCK_INIT;
#pragma clang diagnostic pop

#endif  // BUILDFLAG(IS_APPLE) && !defined(PA_NO_OS_UNFAIR_LOCK_CRBUG_1267256)

  // Spinlock-like, fallback.
  ALWAYS_INLINE bool TrySpinLock();
  ALWAYS_INLINE void ReleaseSpinLock();
  void LockSlowSpinLock();

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
    // architectures. The latency is found to be 141 cycles
    // there (from ~10 on previous ones, nice 14x).
    //
    // According to Agner Fog's instruction tables, the latency is still >100
    // cycles on Ice Lake, and from other sources, seems to be high as well on
    // Adler Lake. Separately, it is (from
    // https://agner.org/optimize/instruction_tables.pdf) also high on AMD Zen 3
    // (~65). So just assume that it's this way for most x86_64 architectures.
    //
    // Also, loop several times here, following the guidelines in section 2.3.4
    // of the manual, "Pause latency in Skylake Client Microarchitecture".
    for (int yields = 0; yields < backoff; yields++) {
      PA_YIELD_PROCESSOR;
      tries++;
    }
    constexpr int kMaxBackoff = 16;
    backoff = std::min(kMaxBackoff, backoff << 1);
  } while (tries < kSpinCount);

  LockSlow();
}

inline constexpr SpinningMutex::SpinningMutex() = default;

#if defined(PA_HAS_FAST_MUTEX)

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

#elif BUILDFLAG(IS_WIN)

ALWAYS_INLINE bool SpinningMutex::Try() {
  return !!::TryAcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

ALWAYS_INLINE void SpinningMutex::Release() {
  ::ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

#elif BUILDFLAG(IS_POSIX)

ALWAYS_INLINE bool SpinningMutex::Try() {
  int retval = pthread_mutex_trylock(&lock_);
  PA_DCHECK(retval == 0 || retval == EBUSY);
  return retval == 0;
}

ALWAYS_INLINE void SpinningMutex::Release() {
  int retval = pthread_mutex_unlock(&lock_);
  PA_DCHECK(retval == 0);
}

#elif BUILDFLAG(IS_FUCHSIA)

ALWAYS_INLINE bool SpinningMutex::Try() {
  return sync_mutex_trylock(&lock_) == ZX_OK;
}

ALWAYS_INLINE void SpinningMutex::Release() {
  sync_mutex_unlock(&lock_);
}

#endif

#else  // defined(PA_HAS_FAST_MUTEX)

ALWAYS_INLINE bool SpinningMutex::TrySpinLock() {
  // Possibly faster than CAS. The theory is that if the cacheline is shared,
  // then it can stay shared, for the contended case.
  return !lock_.load(std::memory_order_relaxed) &&
         !lock_.exchange(true, std::memory_order_acquire);
}

ALWAYS_INLINE void SpinningMutex::ReleaseSpinLock() {
  lock_.store(false, std::memory_order_release);
}

#if BUILDFLAG(IS_APPLE)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

ALWAYS_INLINE bool SpinningMutex::Try() {
  // ARM64 macOS is macOS 11.x at least, guaranteed to have os_unfair_lock().
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
  return os_unfair_lock_trylock(&unfair_lock_);
#else
  if (LIKELY(os_unfair_lock_trylock))
    return os_unfair_lock_trylock(&unfair_lock_);

  return TrySpinLock();
#endif  // BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
}

ALWAYS_INLINE void SpinningMutex::Release() {
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
  return os_unfair_lock_unlock(&unfair_lock_);
#else
  // Always testing trylock(), since the definitions are all or nothing.
  if (LIKELY(os_unfair_lock_trylock))
    return os_unfair_lock_unlock(&unfair_lock_);

  return ReleaseSpinLock();
#endif  // BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
}

ALWAYS_INLINE void SpinningMutex::LockSlow() {
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
  return os_unfair_lock_lock(&unfair_lock_);
#else
  if (LIKELY(os_unfair_lock_trylock))
    return os_unfair_lock_lock(&unfair_lock_);

  return LockSlowSpinLock();
#endif  // BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
}

#pragma clang diagnostic pop

#else
ALWAYS_INLINE bool SpinningMutex::Try() {
  return TrySpinLock();
}

ALWAYS_INLINE void SpinningMutex::Release() {
  return ReleaseSpinLock();
}

ALWAYS_INLINE void SpinningMutex::LockSlow() {
  return LockSlowSpinLock();
}

#endif  // BUILDFLAG(IS_APPLE)

#endif  // defined(PA_HAS_FAST_MUTEX)

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SPINNING_MUTEX_H_
