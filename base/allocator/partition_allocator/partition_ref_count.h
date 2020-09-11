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

class BASE_EXPORT PartitionRefCount {
 public:
  // PartitionRefCount should never be constructed directly.
  PartitionRefCount() = delete;

  ALWAYS_INLINE void Init() { count_.store(1, std::memory_order_relaxed); }

  // Incrementing the counter doesn't imply any visibility about modified
  // memory, hence relaxed atomics. For decrement, visibility is required before
  // the memory gets freed, necessitating an acquire/release barrier before
  // freeing the memory.

  // For details, see base::AtomicRefCount, which has the same constraints and
  // characteristics.
  ALWAYS_INLINE void AddRef() {
    CHECK_GT(count_.fetch_add(1, std::memory_order_relaxed), 0);
  }

  ALWAYS_INLINE void Release() {
    if (count_.fetch_sub(1, std::memory_order_release) == 1) {
      // In most thread-safe reference count implementations, an acquire
      // barrier is required so that all changes made to an object from other
      // threads are visible to its destructor. In our case, the destructor
      // finishes before the final `Release` call, so it shouldn't be a problem.
      // However, we will keep it as a precautionary measure.
      std::atomic_thread_fence(std::memory_order_acquire);
      Free();
    }
  }

  ALWAYS_INLINE bool HasOneRef() {
    return count_.load(std::memory_order_acquire) == 1;
  }

 private:
  void Free();

  std::atomic<int32_t> count_;
};

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

ALWAYS_INLINE size_t PartitionRefCountSizeAdjustAdd(size_t size) {
  PA_DCHECK(size + kInSlotRefCountBufferSize > size);
  return size + kInSlotRefCountBufferSize;
}

ALWAYS_INLINE size_t PartitionRefCountSizeAdjustSubtract(size_t size) {
  PA_DCHECK(size >= kInSlotRefCountBufferSize);
  return size - kInSlotRefCountBufferSize;
}

ALWAYS_INLINE PartitionRefCount* PartitionRefCountPointer(void* ptr) {
  return reinterpret_cast<PartitionRefCount*>(reinterpret_cast<char*>(ptr) -
                                              PartitionAllocGetSlotOffset(ptr) -
                                              kPartitionRefCountOffset);
}

// This function can only be used when we are certain that `ptr` points to the
// beginning of the allocation slot.
ALWAYS_INLINE PartitionRefCount* PartitionRefCountPointerNoOffset(void* ptr) {
  return reinterpret_cast<PartitionRefCount*>(reinterpret_cast<char*>(ptr) -
                                              kPartitionRefCountOffset);
}

ALWAYS_INLINE void* PartitionRefCountPointerAdjustSubtract(void* ptr) {
  return reinterpret_cast<void*>(reinterpret_cast<char*>(ptr) -
                                 kInSlotRefCountBufferSize);
}

ALWAYS_INLINE void* PartitionRefCountPointerAdjustAdd(void* ptr) {
  return reinterpret_cast<void*>(reinterpret_cast<char*>(ptr) +
                                 kInSlotRefCountBufferSize);
}

#else  // ENABLE_REF_COUNTER_FOR_BACKUP_REF_PTR

static constexpr size_t kInSlotRefCountBufferSize = 0;

ALWAYS_INLINE size_t PartitionRefCountSizeAdjustAdd(size_t size) {
  return size;
}

ALWAYS_INLINE size_t PartitionRefCountSizeAdjustSubtract(size_t size) {
  return size;
}

ALWAYS_INLINE void* PartitionRefCountPointerAdjustSubtract(void* ptr) {
  return ptr;
}

ALWAYS_INLINE void* PartitionRefCountPointerAdjustAdd(void* ptr) {
  return ptr;
}

#endif  // ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_REF_COUNT_H_
