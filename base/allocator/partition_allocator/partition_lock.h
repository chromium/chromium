// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_LOCK_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_LOCK_H_

#include <atomic>

#include "base/allocator/partition_allocator/spin_lock.h"
#include "base/no_destructor.h"
#include "base/partition_alloc_buildflags.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

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

#if DCHECK_IS_ON()
template <>
class LOCKABLE MaybeSpinLock<true> {
 public:
  MaybeSpinLock() : lock_() {}
  void Lock() EXCLUSIVE_LOCK_FUNCTION() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
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
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    owning_thread_ref_.store(PlatformThreadRef(), std::memory_order_relaxed);
#endif
    lock_->Release();
  }
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {
    lock_->AssertAcquired();
  }

 private:
  // NoDestructor to avoid issues with the "static destruction order fiasco".
  //
  // This also means that for DCHECK_IS_ON() builds we leak a lock when a
  // partition is destructed. This will in practice only show in some tests, as
  // partitions are not destructed in regular use. In addition, on most
  // platforms, base::Lock doesn't allocate memory and neither does the OS
  // library, and the destructor is a no-op.
  base::NoDestructor<base::Lock> lock_;

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  std::atomic<PlatformThreadRef> owning_thread_ref_ GUARDED_BY(lock_);
#endif
};

#else   // DCHECK_IS_ON()
template <>
class LOCKABLE MaybeSpinLock<true> {
 public:
  void Lock() EXCLUSIVE_LOCK_FUNCTION() { lock_.lock(); }
  void Unlock() UNLOCK_FUNCTION() { lock_.unlock(); }
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {
    // Not supported by subtle::SpinLock.
  }

 private:
  subtle::SpinLock lock_;
};
#endif  // DCHECK_IS_ON()

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
