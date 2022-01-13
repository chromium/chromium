// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_LOCK_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_LOCK_H_

#include <atomic>
#include <type_traits>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/spinning_mutex.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

namespace partition_alloc {
class LOCKABLE Lock {
 public:
  inline constexpr Lock();
  void Acquire() EXCLUSIVE_LOCK_FUNCTION() {
#if DCHECK_IS_ON()
    // When PartitionAlloc is malloc(), it can easily become reentrant. For
    // instance, a DCHECK() triggers in external code (such as
    // base::Lock). DCHECK() error message formatting allocates, which triggers
    // PartitionAlloc, and then we get reentrancy, and in this case infinite
    // recursion.
    //
    // To avoid that, crash quickly when the code becomes reentrant.
    base::PlatformThreadRef current_thread = base::PlatformThread::CurrentRef();
    if (!lock_.Try()) {
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
      if (UNLIKELY(owning_thread_ref_.load(std::memory_order_acquire) ==
                   current_thread)) {
        // Trying to acquire lock while it's held by this thread: reentrancy
        // issue.
        IMMEDIATE_CRASH();
      }
      lock_.Acquire();
    }
    owning_thread_ref_.store(current_thread, std::memory_order_release);
#else
    lock_.Acquire();
#endif
  }

  void Release() UNLOCK_FUNCTION() {
#if DCHECK_IS_ON()
    owning_thread_ref_.store(base::PlatformThreadRef(),
                             std::memory_order_release);
#endif
    lock_.Release();
  }
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {
    lock_.AssertAcquired();
#if DCHECK_IS_ON()
    PA_DCHECK(owning_thread_ref_.load(std ::memory_order_acquire) ==
              base::PlatformThread::CurrentRef());
#endif
  }

  void Reinit() UNLOCK_FUNCTION() {
    lock_.AssertAcquired();
#if DCHECK_IS_ON()
    owning_thread_ref_.store(base::PlatformThreadRef(),
                             std::memory_order_release);
#endif
    lock_.Reinit();
  }

 private:
  SpinningMutex lock_;

#if DCHECK_IS_ON()
  // Should in theory be protected by |lock_|, but we need to read it to detect
  // recursive lock acquisition (and thus, the allocator becoming reentrant).
  std::atomic<base::PlatformThreadRef> owning_thread_ref_{};
#endif
};

class SCOPED_LOCKABLE ScopedGuard {
 public:
  explicit ScopedGuard(Lock& lock) EXCLUSIVE_LOCK_FUNCTION(lock) : lock_(lock) {
    lock_.Acquire();
  }
  ~ScopedGuard() UNLOCK_FUNCTION() { lock_.Release(); }

 private:
  Lock& lock_;
};

namespace internal {

class SCOPED_LOCKABLE ScopedUnlockGuard {
 public:
  explicit ScopedUnlockGuard(Lock& lock) UNLOCK_FUNCTION(lock) : lock_(lock) {
    lock_.Release();
  }
  ~ScopedUnlockGuard() EXCLUSIVE_LOCK_FUNCTION() { lock_.Acquire(); }

 private:
  Lock& lock_;
};

}  // namespace internal

constexpr Lock::Lock() = default;

// We want PartitionRoot to not have a global destructor, so this should not
// have one.
static_assert(std::is_trivially_destructible<Lock>::value, "");

}  // namespace partition_alloc

namespace base {
namespace internal {

using PartitionLock = ::partition_alloc::Lock;
using PartitionAutoLock = ::partition_alloc::ScopedGuard;

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_LOCK_H_
