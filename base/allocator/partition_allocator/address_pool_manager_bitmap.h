// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_

#include <bitset>

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/synchronization/lock.h"

#if !defined(PA_HAS_64_BITS_POINTERS)

namespace base {

namespace internal {

// AddressPoolManagerBitmap is a set of bitmaps that track whether a given
// address is in a pool that supports BackupRefPtr, or in a pool that doesn't
// support it. All PartitionAlloc allocations must be in either of the pools.
//
// This code is specific to 32-bit systems.
class BASE_EXPORT AddressPoolManagerBitmap {
 public:
  static constexpr uint64_t kGiB = 1024 * 1024 * 1024ull;
  static constexpr uint64_t kAddressSpaceSize = 4ull * kGiB;
  // Non-BRP pool includes, among others, direct map allocations, which reserve
  // address space at PageAllocationGranularity(). BRP pool only supports normal
  // bucket allocations, which always reserve address space at 2MB granularity.
  static constexpr size_t kNonBRPPoolBits =
      kAddressSpaceSize / PageAllocationGranularity();
  static constexpr size_t kBRPPoolBits = kAddressSpaceSize / kSuperPageSize;

  static bool IsManagedByNonBRPPool(const void* address) {
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
    // It is safe to read |non_brp_pool_bits_| without a lock since the caller
    // is responsible for guaranteeing that the address is inside a valid
    // allocation and the deallocation call won't race with this call.
    return TS_UNCHECKED_READ(non_brp_pool_bits_)
        .test(address_as_uintptr / PageAllocationGranularity());
  }

  static bool IsManagedByBRPPool(const void* address) {
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
    // It is safe to read |brp_pool_bits_| without a lock since the caller
    // is responsible for guaranteeing that the address is inside a valid
    // allocation and the deallocation call won't race with this call.
    return TS_UNCHECKED_READ(brp_pool_bits_)
        .test(address_as_uintptr >> kSuperPageShift);
  }

 private:
  friend class AddressPoolManager;

  static Lock& GetLock();

  static std::bitset<kNonBRPPoolBits> non_brp_pool_bits_ GUARDED_BY(GetLock());
  static std::bitset<kBRPPoolBits> brp_pool_bits_ GUARDED_BY(GetLock());
};

}  // namespace internal

ALWAYS_INLINE bool IsManagedByPartitionAllocNonBRPPool(const void* address) {
  return internal::AddressPoolManagerBitmap::IsManagedByNonBRPPool(address);
}

ALWAYS_INLINE bool IsManagedByPartitionAllocBRPPool(const void* address) {
  return internal::AddressPoolManagerBitmap::IsManagedByBRPPool(address);
}

}  // namespace base

#endif  // !defined(PA_HAS_64_BITS_POINTERS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_
