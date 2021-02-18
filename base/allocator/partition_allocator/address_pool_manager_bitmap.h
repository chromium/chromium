// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_

#include <bitset>

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/partition_alloc_buildflags.h"
#include "base/synchronization/lock.h"

#if !defined(PA_HAS_64_BITS_POINTERS)

namespace base {

namespace internal {

// AddressPoolManagerBitmap is the bitmap that tracks whether a given address is
// managed by the direct map or normal buckets.
class BASE_EXPORT AddressPoolManagerBitmap {
 public:
  static constexpr uint64_t kGiB = 1024 * 1024 * 1024ull;
  static constexpr uint64_t kAddressSpaceSize = 4ull * kGiB;

#if BUILDFLAG(MAKE_GIGACAGE_GRANULARITY_PARTITION_PAGE_SIZE)
  static constexpr size_t kBitShiftOfNormalBucketBitmap = PartitionPageShift();
  static constexpr size_t kBytesPer1BitOfNormalBucketBitmap =
      PartitionPageSize();
  static constexpr size_t kGuardOffsetOfNormalBucketBitmap = 1;
  static constexpr size_t kGuardBitsOfNormalBucketBitmap = 2;
#else
  static constexpr size_t kBitShiftOfNormalBucketBitmap = kSuperPageShift;
  static constexpr size_t kBytesPer1BitOfNormalBucketBitmap = kSuperPageSize;
  static constexpr size_t kGuardOffsetOfNormalBucketBitmap = 0;
  static constexpr size_t kGuardBitsOfNormalBucketBitmap = 0;
#endif
  static constexpr size_t kNormalBucketBits =
      kAddressSpaceSize / kBytesPer1BitOfNormalBucketBitmap;
  static constexpr size_t kDirectMapBits =
      kAddressSpaceSize / PageAllocationGranularity();

  static bool IsManagedByDirectMapPool(const void* address) {
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
    // It is safe to read |directmap_bits_| without a lock since the caller is
    // responsible for guaranteeing that the address is inside a valid
    // allocation and the deallocation call won't race with this call.
    return TS_UNCHECKED_READ(directmap_bits_)
        .test(address_as_uintptr / PageAllocationGranularity());
  }

  static bool IsManagedByNormalBucketPool(const void* address) {
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
    // It is safe to read |normal_bucket_bits_| without a lock since the caller
    // is responsible for guaranteeing that the address is inside a valid
    // allocation and the deallocation call won't race with this call.
    return TS_UNCHECKED_READ(normal_bucket_bits_)
        .test(address_as_uintptr >> kBitShiftOfNormalBucketBitmap);
  }

 private:
  friend class AddressPoolManager;

  static Lock& GetLock();

  static std::bitset<kDirectMapBits> directmap_bits_ GUARDED_BY(GetLock());
  static std::bitset<kNormalBucketBits> normal_bucket_bits_
      GUARDED_BY(GetLock());
};

}  // namespace internal

ALWAYS_INLINE bool IsManagedByPartitionAllocDirectMap(const void* address) {
  return internal::AddressPoolManagerBitmap::IsManagedByDirectMapPool(address);
}

ALWAYS_INLINE bool IsManagedByPartitionAllocNormalBuckets(const void* address) {
  return internal::AddressPoolManagerBitmap::IsManagedByNormalBucketPool(
      address);
}

}  // namespace base

#endif  // !defined(PA_HAS_64_BITS_POINTERS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_
