// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_LOCK_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_LOCK_H_

#include <atomic>
#include <type_traits>

#include "base/allocator/buildflags.h"
#include "base/no_destructor.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "base/allocator/partition_allocator/spinning_futex_linux.h"
#endif

namespace base {
namespace internal {

template <bool thread_safe>
class LOCKABLE MaybeSpinLock {
 public:
  void Lock() EXCLUSIVE_LOCK_FUNCTION() {}
  void Unlock() UNLOCK_FUNCTION() {}
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {}
};

template <bool thread_safe>
class SCOPED_LOCKABLE ScopedGuard {
 public:
  explicit ScopedGuard(MaybeSpinLock<thread_safe>& lock)
      EXCLUSIVE_LOCK_FUNCTION(lock)
      : lock_(lock) {
    lock_.Lock();
  }
  ~ScopedGuard() UNLOCK_FUNCTION() { lock_.Unlock(); }

 private:
  MaybeSpinLock<thread_safe>& lock_;
};

template <bool thread_safe>
class SCOPED_LOCKABLE ScopedUnlockGuard {
 public:
  explicit ScopedUnlockGuard(MaybeSpinLock<thread_safe>& lock)
      UNLOCK_FUNCTION(lock)
      : lock_(lock) {
    lock_.Unlock();
  }
  ~ScopedUnlockGuard() EXCLUSIVE_LOCK_FUNCTION() { lock_.Lock(); }

 private:
  MaybeSpinLock<thread_safe>& lock_;
};

// Spinlock. Do not use, to be removed. crbug.com/1061437.
class BASE_EXPORT SpinLock {
 public:
  SpinLock() = default;
  ~SpinLock() = default;

  ALWAYS_INLINE void Acquire() {
    if (LIKELY(!lock_.exchange(true, std::memory_order_acquire)))
      return;
    AcquireSlow();
  }

  ALWAYS_INLINE bool Try() {
    // Faster than simple CAS.
    return !lock_.load(std::memory_order_relaxed) &&
           !lock_.exchange(true, std::memory_order_acquire);
  }

  ALWAYS_INLINE void Release() {
    lock_.store(false, std::memory_order_release);
  }

  // Not supported.
  void AssertAcquired() const {}

 private:
  // This is called if the initial attempt to acquire the lock fails. It's
  // slower, but has a much better scheduling and power consumption behavior.
  void AcquireSlow();

  std::atomic_int lock_{0};
};

template <>
class LOCKABLE MaybeSpinLock<true> {
 public:
  MaybeSpinLock() : lock_() {}
  void Lock() EXCLUSIVE_LOCK_FUNCTION() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && DCHECK_IS_ON()
    // When PartitionAlloc is malloc(), it can easily become reentrant. For
    // instance, a DCHECK() triggers in external code (such as
    // base::Lock). DCHECK() error message formatting allocates, which triggers
    // PartitionAlloc, and then we get reentrancy, and in this case infinite
    // recursion.
    //
    // To avoid that, crash quickly when the code becomes reentrant.
    PlatformThreadRef current_thread = PlatformThread::CurrentRef();
    if (!lock_->Try()) {
      // The lock wasn't free when we tried to acquire it. This can be because
      // another thread or *this* thread was holding it.
      //
      // If it's this thread holding it, then it cannot have become free in the
      // meantime, and the current value of |owning_thread_ref_| is valid, as it
      // was set by this thread. Assuming that writes to |owning_thread_ref_|
      // are atomic, then if it's us, we are trying to recursively acquire a
      // non-recursive lock.
      //
      // Note that we don't rely on a DCHECK() in base::Lock(), as it would
      // itself allocate. Meaning that without this code, a reentrancy issue
      // hangs on Linux.
      if (UNLIKELY(TS_UNCHECKED_READ(owning_thread_ref_.load(
                       std::memory_order_relaxed)) == current_thread)) {
        // Trying to acquire lock while it's held by this thread: reentrancy
        // issue.
        IMMEDIATE_CRASH();
      }
      lock_->Acquire();
    }
    owning_thread_ref_.store(current_thread, std::memory_order_relaxed);
#else
    lock_->Acquire();
#endif
  }

  void Unlock() UNLOCK_FUNCTION() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && DCHECK_IS_ON()
    owning_thread_ref_.store(PlatformThreadRef(), std::memory_order_relaxed);
#endif
    lock_->Release();
  }
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {
    lock_->AssertAcquired();
  }

 private:
#if defined(OS_LINUX) || defined(OS_ANDROID)
  base::NoDestructor<SpinningFutex> lock_;
#else
  // base::Lock is slower on the fast path than SpinLock, hence we still use it
  // on non-DCHECK() builds. crbug.com/1125999
  base::NoDestructor<SpinLock> lock_;
  // base::NoDestructor is here to use the same code elsewhere, we are not
  // leaking anything.
  static_assert(std::is_trivially_destructible<SpinLock>::value, "");
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && DCHECK_IS_ON()
  std::atomic<PlatformThreadRef> owning_thread_ref_ GUARDED_BY(lock_);
#endif
};

template <>
class LOCKABLE MaybeSpinLock<false> {
 public:
  void Lock() EXCLUSIVE_LOCK_FUNCTION() {}
  void Unlock() UNLOCK_FUNCTION() {}
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {}

  char padding_[sizeof(MaybeSpinLock<true>)];
};

static_assert(
    sizeof(MaybeSpinLock<true>) == sizeof(MaybeSpinLock<false>),
    "Sizes should be equal to enseure identical layout of PartitionRoot");

}  // namespace internal
}  // namespace base
#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_LOCK_H_
