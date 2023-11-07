// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_LOCK_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_LOCK_H_

#include <atomic>
#include <type_traits>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/immediate_crash.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/thread_annotations.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_check.h"
#include "base/allocator/partition_allocator/src/partition_alloc/spinning_mutex.h"
#include "base/allocator/partition_allocator/src/partition_alloc/thread_isolation/thread_isolation.h"
#include "build/build_config.h"

namespace partition_alloc::internal {

class PA_LOCKABLE Lock {
 public:
  inline constexpr Lock();
  void Acquire() PA_EXCLUSIVE_LOCK_FUNCTION() {
#if BUILDFLAG(PA_DCHECK_IS_ON)
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
    LiftThreadIsolationScope lift_thread_isolation_restrictions;
#endif

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
      if (PA_UNLIKELY(owning_thread_ref_.load(std::memory_order_acquire) ==
                      current_thread)) {
        // Trying to acquire lock while it's held by this thread: reentrancy
        // issue.
        PA_IMMEDIATE_CRASH();
      }
      lock_.Acquire();
    }
    owning_thread_ref_.store(current_thread, std::memory_order_release);
#else
    lock_.Acquire();
#endif
  }

  void Release() PA_UNLOCK_FUNCTION() {
#if BUILDFLAG(PA_DCHECK_IS_ON)
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
    LiftThreadIsolationScope lift_thread_isolation_restrictions;
#endif
    owning_thread_ref_.store(base::PlatformThreadRef(),
                             std::memory_order_release);
#endif
    lock_.Release();
  }
  void AssertAcquired() const PA_ASSERT_EXCLUSIVE_LOCK() {
    lock_.AssertAcquired();
#if BUILDFLAG(PA_DCHECK_IS_ON)
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
    LiftThreadIsolationScope lift_thread_isolation_restrictions;
#endif
    PA_DCHECK(owning_thread_ref_.load(std ::memory_order_acquire) ==
              base::PlatformThread::CurrentRef());
#endif
  }

  void Reinit() PA_UNLOCK_FUNCTION() {
    lock_.AssertAcquired();
#if BUILDFLAG(PA_DCHECK_IS_ON)
    owning_thread_ref_.store(base::PlatformThreadRef(),
                             std::memory_order_release);
#endif
    lock_.Reinit();
  }

 private:
  SpinningMutex lock_;

#if BUILDFLAG(PA_DCHECK_IS_ON)
  // Should in theory be protected by |lock_|, but we need to read it to detect
  // recursive lock acquisition (and thus, the allocator becoming reentrant).
  std::atomic<base::PlatformThreadRef> owning_thread_ref_ =
      base::PlatformThreadRef();
#endif
};

class PA_SCOPED_LOCKABLE ScopedGuard {
 public:
  explicit ScopedGuard(Lock& lock) PA_EXCLUSIVE_LOCK_FUNCTION(lock)
      : lock_(lock) {
    lock_.Acquire();
  }
  ~ScopedGuard() PA_UNLOCK_FUNCTION() { lock_.Release(); }

 private:
  Lock& lock_;
};

class PA_SCOPED_LOCKABLE ScopedUnlockGuard {
 public:
  explicit ScopedUnlockGuard(Lock& lock) PA_UNLOCK_FUNCTION(lock)
      : lock_(lock) {
    lock_.Release();
  }
  ~ScopedUnlockGuard() PA_EXCLUSIVE_LOCK_FUNCTION() { lock_.Acquire(); }

 private:
  Lock& lock_;
};

constexpr Lock::Lock() = default;

// We want PartitionRoot to not have a global destructor, so this should not
// have one.
static_assert(std::is_trivially_destructible_v<Lock>, "");

}  // namespace partition_alloc::internal

namespace base {
namespace internal {

using PartitionLock = ::partition_alloc::internal::Lock;
using PartitionAutoLock = ::partition_alloc::internal::ScopedGuard;

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_LOCK_H_
