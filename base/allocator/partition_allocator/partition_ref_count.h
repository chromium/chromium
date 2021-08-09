// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_REF_COUNT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_REF_COUNT_H_

#include <atomic>
#include <cstdint>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "build/build_config.h"

namespace base {

namespace internal {

#if BUILDFLAG(USE_BACKUP_REF_PTR)

namespace {

[[noreturn]] NOINLINE NOT_TAIL_CALLED void DoubleFreeOrCorruptionDetected() {
  NO_CODE_FOLDING();
  IMMEDIATE_CRASH();
}

}  // namespace

// Special-purpose atomic reference count class used by BackupRefPtrImpl.
// The least significant bit of the count is reserved for tracking the liveness
// state of an allocation: it's set when the allocation is created and cleared
// on free(). So the count can be:
//
// 1 for an allocation that is just returned from Alloc()
// 2 * k + 1 for a "live" allocation with k references
// 2 * k for an allocation with k dangling references after Free()
//
// This protects against double-free's, as we check whether the reference count
// is odd in |ReleaseFromAllocator()|, and if not we have a double-free.
class BASE_EXPORT PartitionRefCount {
 public:
  PartitionRefCount();

  // Incrementing the counter doesn't imply any visibility about modified
  // memory, hence relaxed atomics. For decrement, visibility is required before
  // the memory gets freed, necessitating an acquire/release barrier before
  // freeing the memory.

  // For details, see base::AtomicRefCount, which has the same constraints and
  // characteristics.
  ALWAYS_INLINE void Acquire() {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CheckCookie();
#endif

    PA_CHECK(count_.fetch_add(2, std::memory_order_relaxed) > 0);
  }

  // Returns true if the allocation should be reclaimed.
  ALWAYS_INLINE bool Release() {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CheckCookie();
#endif

    if (count_.fetch_sub(2, std::memory_order_release) == 2) {
      // In most thread-safe reference count implementations, an acquire
      // barrier is required so that all changes made to an object from other
      // threads are visible to its destructor. In our case, the destructor
      // finishes before the final `Release` call, so it shouldn't be a problem.
      // However, we will keep it as a precautionary measure.
      std::atomic_thread_fence(std::memory_order_acquire);
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      // The allocation is about to get freed, so clear the cookie.
      brp_cookie_ = 0;
#endif
      return true;
    }

    return false;
  }

  // Returns true if the allocation should be reclaimed.
  // This function should be called by the allocator during Free().
  ALWAYS_INLINE bool ReleaseFromAllocator() {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CheckCookie();
#endif

    int32_t old_count = count_.fetch_sub(1, std::memory_order_release);
    if (UNLIKELY(!(old_count & 1)))
      DoubleFreeOrCorruptionDetected();
    if (old_count == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      // The allocation is about to get freed, so clear the cookie.
      brp_cookie_ = 0;
#endif
      return true;
    }

    return false;
  }

  // "IsAlive" means is allocated and not freed. "KnownRefs" refers to
  // raw_ptr<T> references. There may be other references from raw pointers or
  // unique_ptr, but we have no way of tracking them, so we hope for the best.
  // To summarize, the function returns whether we believe the allocation can be
  // safely freed.
  ALWAYS_INLINE bool IsAliveWithNoKnownRefs() {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CheckCookie();
#endif

    return count_.load(std::memory_order_acquire) == 1;
  }

  ALWAYS_INLINE bool IsAlive() {
    bool alive = count_.load(std::memory_order_relaxed) & 1;
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if (alive)
      CheckCookie();
#endif
    return alive;
  }

 private:
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  // The cookie helps us ensure that:
  // 1) The reference count pointer calculation is correct.
  // 2) The returned allocation slot is not freed.
  ALWAYS_INLINE void CheckCookie() {
    PA_CHECK(brp_cookie_ == CalculateCookie());
  }

  ALWAYS_INLINE uint32_t CalculateCookie() {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) ^
           kCookieSalt;
  }

  static constexpr uint32_t kCookieSalt = 0xc01dbeef;
  volatile uint32_t brp_cookie_;
#endif  // DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)

  std::atomic<int32_t> count_{1};
};

ALWAYS_INLINE PartitionRefCount::PartitionRefCount()
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    : brp_cookie_(CalculateCookie())
#endif
{
}

#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)

static_assert(base::kAlignment % alignof(PartitionRefCount) == 0,
              "kAlignment must be multiples of alignof(PartitionRefCount).");

// Allocate extra space for the reference count to satisfy the alignment
// requirement.
static constexpr size_t kInSlotRefCountBufferSize = sizeof(PartitionRefCount);
constexpr size_t kPartitionRefCountOffsetAdjustment = 0;
constexpr size_t kPartitionPastAllocationAdjustment = 0;

constexpr size_t kPartitionRefCountIndexMultiplier =
    SystemPageSize() /
    (sizeof(PartitionRefCount) * (kSuperPageSize / SystemPageSize()));

static_assert((sizeof(PartitionRefCount) * (kSuperPageSize / SystemPageSize()) *
                   kPartitionRefCountIndexMultiplier <=
               SystemPageSize()),
              "PartitionRefCount Bitmap size must be smaller than or equal to "
              "<= SystemPageSize().");

ALWAYS_INLINE PartitionRefCount* PartitionRefCountPointer(void* slot_start) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  CheckThatSlotOffsetIsZero(slot_start);
#endif
  uintptr_t slot_start_as_uintptr = reinterpret_cast<uintptr_t>(slot_start);
  if (LIKELY(slot_start_as_uintptr & SystemPageOffsetMask())) {
    uintptr_t refcount_ptr_as_uintptr =
        slot_start_as_uintptr - sizeof(PartitionRefCount);
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    PA_CHECK(refcount_ptr_as_uintptr % alignof(PartitionRefCount) == 0);
#endif
    return reinterpret_cast<PartitionRefCount*>(refcount_ptr_as_uintptr);
  } else {
    PartitionRefCount* bitmap_base = reinterpret_cast<PartitionRefCount*>(
        (slot_start_as_uintptr & kSuperPageBaseMask) + SystemPageSize() * 2);
    size_t index =
        ((slot_start_as_uintptr & kSuperPageOffsetMask) >> SystemPageShift()) *
        kPartitionRefCountIndexMultiplier;
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    PA_CHECK(sizeof(PartitionRefCount) * index <= SystemPageSize());
#endif
    return bitmap_base + index;
  }
}

#else  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)

// Allocate extra space for the reference count to satisfy the alignment
// requirement.
static constexpr size_t kInSlotRefCountBufferSize = base::kAlignment;
constexpr size_t kPartitionRefCountOffsetAdjustment = kInSlotRefCountBufferSize;

// This is for adjustment of pointers right past the allocation, which may point
// to the next slot. First subtract 1 to bring them to the intended slot, and
// only then we'll be able to find ref-count in that slot.
constexpr size_t kPartitionPastAllocationAdjustment = 1;

ALWAYS_INLINE PartitionRefCount* PartitionRefCountPointer(void* slot_start) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  CheckThatSlotOffsetIsZero(slot_start);
#endif
  return reinterpret_cast<PartitionRefCount*>(slot_start);
}

#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)

static_assert(sizeof(PartitionRefCount) <= kInSlotRefCountBufferSize,
              "PartitionRefCount should fit into the in-slot buffer.");

#else  // BUILDFLAG(USE_BACKUP_REF_PTR)

static constexpr size_t kInSlotRefCountBufferSize = 0;
constexpr size_t kPartitionRefCountOffsetAdjustment = 0;

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

constexpr size_t kPartitionRefCountSizeAdjustment = kInSlotRefCountBufferSize;

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_REF_COUNT_H_
