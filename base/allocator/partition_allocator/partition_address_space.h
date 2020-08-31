// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_

#include <algorithm>

#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/base_export.h"
#include "base/bits.h"
#include "base/notreached.h"
#include "base/partition_alloc_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace base {

namespace internal {

// The feature is not applicable to 32-bit address space.
#if defined(PA_HAS_64_BITS_POINTERS)

// Reserves address space for PartitionAllocator.
class BASE_EXPORT PartitionAddressSpace {
 public:
  static ALWAYS_INLINE internal::pool_handle GetDirectMapPool() {
    return direct_map_pool_;
  }
  static ALWAYS_INLINE internal::pool_handle GetNormalBucketPool() {
    return normal_bucket_pool_;
  }

  static void Init();
  static void UninitForTesting();

  static ALWAYS_INLINE bool IsInitialized() {
    if (reserved_base_address_) {
      PA_DCHECK(direct_map_pool_ != 0);
      PA_DCHECK(normal_bucket_pool_ != 0);
      return true;
    }

    PA_DCHECK(direct_map_pool_ == 0);
    PA_DCHECK(normal_bucket_pool_ == 0);
    return false;
  }

  static ALWAYS_INLINE bool IsInDirectMapPool(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) & kDirectMapPoolBaseMask) ==
           direct_map_pool_base_address_;
  }
  static ALWAYS_INLINE bool IsInNormalBucketPool(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) & kNormalBucketPoolBaseMask) ==
           normal_bucket_pool_base_address_;
  }

  // PartitionAddressSpace is static_only class.
  PartitionAddressSpace() = delete;
  PartitionAddressSpace(const PartitionAddressSpace&) = delete;
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;

 private:
  // Partition Alloc Address Space
  // Reserves 32GiB address space for one direct map pool and one normal bucket
  // pool, 16GiB each.
  // TODO(bartekn): Look into devices with 39-bit address space that have 256GiB
  // user-mode space. Libraries loaded at random addresses may stand in the way
  // of reserving a contiguous 48GiB region (even though we're requesting only
  // 32GiB, AllocPages may under the covers reserve extra 16GiB to satisfy the
  // alignment requirements).
  //
  // +----------------+ reserved_base_address_ (16GiB aligned)
  // |   direct map   |     == direct_map_pool_base_address_
  // |     space      |
  // +----------------+ reserved_base_address_ + 16GiB
  // | normal bucket  |     == normal_bucket_pool_base_address_
  // |     space      |
  // +----------------+ reserved_base_address_ + 32GiB

  static constexpr size_t kGigaBytes = 1024 * 1024 * 1024;

  // Pool sizes are flexible, as long as each pool is aligned on its own size
  // boundary and the size is a power of two. The entire region is aligned on
  // the max pool size boundary, so the further pools only need to care about
  // the shift from the beginning of the region (for clarity, the pool sizes are
  // declared in the order the pools are allocated).
  //
  // For example, [16GiB,8GiB] would work, but [8GiB,16GiB] wouldn't (the 2nd
  // pool is aligned on 8GiB but needs 16GiB), and [8GiB,8GiB,16GiB,1GiB] would.
  static constexpr size_t kDirectMapPoolSize = 16 * kGigaBytes;
  static constexpr size_t kNormalBucketPoolSize = 16 * kGigaBytes;

  static constexpr size_t kDesiredAddressSpaceSize =
      kDirectMapPoolSize + kNormalBucketPoolSize;
  static constexpr size_t kReservedAddressSpaceAlignment =
      std::max(kDirectMapPoolSize, kNormalBucketPoolSize);

  static_assert(bits::IsPowerOfTwo(kDirectMapPoolSize) &&
                    bits::IsPowerOfTwo(kNormalBucketPoolSize),
                "Each pool size should be a power of two.");
  static_assert(bits::IsPowerOfTwo(kReservedAddressSpaceAlignment),
                "kReservedAddressSpaceAlignment should be a power of two.");
  static_assert(kReservedAddressSpaceAlignment >= kDirectMapPoolSize &&
                    kReservedAddressSpaceAlignment >= kNormalBucketPoolSize,
                "kReservedAddressSpaceAlignment should be larger or equal to "
                "each pool size.");
  static_assert(kReservedAddressSpaceAlignment % kDirectMapPoolSize == 0 &&
                    (kReservedAddressSpaceAlignment + kDirectMapPoolSize) %
                            kNormalBucketPoolSize ==
                        0,
                "Each pool should be aligned to its own size");

  // Masks used to easy determine belonging to a pool.
  static constexpr uintptr_t kDirectMapPoolOffsetMask =
      static_cast<uintptr_t>(kDirectMapPoolSize) - 1;
  static constexpr uintptr_t kDirectMapPoolBaseMask = ~kDirectMapPoolOffsetMask;
  static constexpr uintptr_t kNormalBucketPoolOffsetMask =
      static_cast<uintptr_t>(kNormalBucketPoolSize) - 1;
  static constexpr uintptr_t kNormalBucketPoolBaseMask =
      ~kNormalBucketPoolOffsetMask;

  // See the comment describing the address layout above.
  static uintptr_t reserved_base_address_;
  static uintptr_t direct_map_pool_base_address_;
  static uintptr_t normal_bucket_pool_base_address_;

  static internal::pool_handle direct_map_pool_;
  static internal::pool_handle normal_bucket_pool_;
};

ALWAYS_INLINE internal::pool_handle GetDirectMapPool() {
  // This file is included from checked_ptr.h. This will result in a cycle if it
  // includes partition_alloc_features.h where IsPartitionAllocGigaCageEnabled
  // resides, because it includes Finch headers which may include checked_ptr.h.
  // TODO(bartekn): Uncomment once Finch is no longer used there.
  // PA_DCHECK(IsPartitionAllocGigaCageEnabled());
  return PartitionAddressSpace::GetDirectMapPool();
}

ALWAYS_INLINE internal::pool_handle GetNormalBucketPool() {
  // TODO(bartekn): Uncomment once Finch is no longer used there (see above).
  // PA_DCHECK(IsPartitionAllocGigaCageEnabled());
  return PartitionAddressSpace::GetNormalBucketPool();
}

#else  // defined(PA_HAS_64_BITS_POINTERS)

ALWAYS_INLINE internal::pool_handle GetDirectMapPool() {
  NOTREACHED();
  return 0;
}

ALWAYS_INLINE internal::pool_handle GetNormalBucketPool() {
  NOTREACHED();
  return 0;
}

#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal

ALWAYS_INLINE bool IsManagedByPartitionAllocDirectMap(const void* address) {
#if defined(PA_HAS_64_BITS_POINTERS)
  return internal::PartitionAddressSpace::IsInDirectMapPool(address);
#else
  return false;
#endif
}

ALWAYS_INLINE bool IsManagedByPartitionAllocNormalBuckets(const void* address) {
#if defined(PA_HAS_64_BITS_POINTERS)
  return internal::PartitionAddressSpace::IsInNormalBucketPool(address);
#else
  return false;
#endif
}

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
