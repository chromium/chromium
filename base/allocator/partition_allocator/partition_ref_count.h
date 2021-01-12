// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_REF_COUNT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_REF_COUNT_H_

#include <atomic>

#include "base/allocator/partition_allocator/checked_ptr_support.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/base_export.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"

namespace base {

namespace internal {

#if ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR

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
    PA_CHECK(count_.fetch_add(2, std::memory_order_relaxed) > 0);
  }

  // Returns true if the allocation should be reclaimed.
  ALWAYS_INLINE bool Release() {
    if (count_.fetch_sub(2, std::memory_order_release) == 2) {
      // In most thread-safe reference count implementations, an acquire
      // barrier is required so that all changes made to an object from other
      // threads are visible to its destructor. In our case, the destructor
      // finishes before the final `Release` call, so it shouldn't be a problem.
      // However, we will keep it as a precautionary measure.
      std::atomic_thread_fence(std::memory_order_acquire);
      return true;
    }

    return false;
  }

  // Returns true if the allocation should be reclaimed.
  // This function should be called by the allocator during Free().
  ALWAYS_INLINE bool ReleaseFromAllocator() {
    int32_t old_count = count_.fetch_sub(1, std::memory_order_release);
    PA_CHECK(old_count & 1);  // double-free detection
    if (old_count == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
      return true;
    }

    return false;
  }

  ALWAYS_INLINE bool HasOneRef() {
    return count_.load(std::memory_order_acquire) == 1;
  }

  ALWAYS_INLINE bool IsAlive() {
    return count_.load(std::memory_order_relaxed) & 1;
  }

 private:
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
  void* padding_;  // TODO(crbug.com/1164636): This "workaround" is meant to
                   // reduce the number of freelist corruption crashes we see in
                   // experiments. Remove once root cause has been found.
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  std::atomic<int32_t> count_{1};
};

ALWAYS_INLINE PartitionRefCount::PartitionRefCount() = default;

// Allocate extra space for the reference count to satisfy the alignment
// requirement.
static constexpr size_t kInSlotRefCountBufferSize = kAlignment;
static_assert(sizeof(PartitionRefCount) <= kInSlotRefCountBufferSize,
              "PartitionRefCount should fit into the in-slot buffer.");

#if DCHECK_IS_ON()
static constexpr size_t kPartitionRefCountOffset =
    kInSlotRefCountBufferSize + kCookieSize;
#else
static constexpr size_t kPartitionRefCountOffset = kInSlotRefCountBufferSize;
#endif

ALWAYS_INLINE PartitionRefCount* PartitionRefCountPointer(void* slot_start) {
  DCheckGetSlotOffsetIsZero(slot_start);
  return reinterpret_cast<PartitionRefCount*>(slot_start);
}

ALWAYS_INLINE PartitionRefCount* PartitionRefCountPointerNoDCheck(
    void* slot_start) {
  return reinterpret_cast<PartitionRefCount*>(slot_start);
}

#else  // ENABLE_REF_COUNTER_FOR_BACKUP_REF_PTR

static constexpr size_t kInSlotRefCountBufferSize = 0;

#endif  // ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR

constexpr size_t kPartitionRefCountSizeAdjustment = kInSlotRefCountBufferSize;
constexpr size_t kPartitionRefCountOffsetAdjustment = kInSlotRefCountBufferSize;

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_REF_COUNT_H_
