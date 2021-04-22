// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_

#include <array>
#include <atomic>
#include <bitset>

#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/partition_alloc_buildflags.h"
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

  // BRP pool includes only normal buckets. 2MB granularity is used, unless
  // MAKE_GIGACAGE_GRANULARITY_PARTITION_PAGE_SIZE is on, in which case we need
  // to lower granularity down to partition page level to eliminate the guard
  // pages at the end. This is needed so that pointers immediately past an
  // allocation that immediately precede a super page in BRP pool don't
  // accidentally fall into that pool.
#if BUILDFLAG(MAKE_GIGACAGE_GRANULARITY_PARTITION_PAGE_SIZE)
  static constexpr size_t kBitShiftOfBRPPoolBitmap = PartitionPageShift();
  static constexpr size_t kBytesPer1BitOfBRPPoolBitmap = PartitionPageSize();
  static constexpr size_t kGuardOffsetOfBRPPoolBitmap = 1;
  static constexpr size_t kGuardBitsOfBRPPoolBitmap = 2;
#else
  static constexpr size_t kBitShiftOfBRPPoolBitmap = kSuperPageShift;
  static constexpr size_t kBytesPer1BitOfBRPPoolBitmap = kSuperPageSize;
  static constexpr size_t kGuardOffsetOfBRPPoolBitmap = 0;
  static constexpr size_t kGuardBitsOfBRPPoolBitmap = 0;
#endif
  static constexpr size_t kBRPPoolBits =
      kAddressSpaceSize / kBytesPer1BitOfBRPPoolBitmap;

  // Non-BRP pool includes both normal bucket and direct map allocations, so
  // PageAllocationGranularity() has to be used. No need to eliminate guard
  // pages at the ends in the MAKE_GIGACAGE_GRANULARITY_PARTITION_PAGE_SIZE
  // case, as this is a BackupRefPtr-specific concern.
  static constexpr size_t kNonBRPPoolBits =
      kAddressSpaceSize / PageAllocationGranularity();

  // Returns false for nullptr.
  static bool IsManagedByNonBRPPool(const void* address) {
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
    // It is safe to read |non_brp_pool_bits_| without a lock since the caller
    // is responsible for guaranteeing that the address is inside a valid
    // allocation and the deallocation call won't race with this call.
    return TS_UNCHECKED_READ(non_brp_pool_bits_)
        .test(address_as_uintptr / PageAllocationGranularity());
  }

  // Returns false for nullptr.
  static bool IsManagedByBRPPool(const void* address) {
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
    // It is safe to read |brp_pool_bits_| without a lock since the caller
    // is responsible for guaranteeing that the address is inside a valid
    // allocation and the deallocation call won't race with this call.
    return TS_UNCHECKED_READ(brp_pool_bits_)
        .test(address_as_uintptr >> kBitShiftOfBRPPoolBitmap);
  }

#if BUILDFLAG(USE_BRP_POOL_BLOCKLIST)
  static void IncrementOutsideOfBRPPoolPtrRefCount(const void* address) {
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);

#if BUILDFLAG(NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST)
    brp_forbidden_super_page_map_[address_as_uintptr >> kSuperPageShift].store(
        true, std::memory_order_relaxed);
#else
    super_page_refcount_map_[address_as_uintptr >> kSuperPageShift].fetch_add(
        1, std::memory_order_relaxed);
#endif
  }

  static void DecrementOutsideOfBRPPoolPtrRefCount(const void* address) {
#if BUILDFLAG(NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST)
    // No-op. In this mode, we only use one bit per super-page and, therefore,
    // can't tell if there's more than one associated CheckedPtr at a given
    // time. There's a small risk is that we may exhaust the entire address
    // space. On the other hand, a single relaxed store (in the above function)
    // is much less expensive than two CAS operations.
#else
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);

    super_page_refcount_map_[address_as_uintptr >> kSuperPageShift].fetch_sub(
        1, std::memory_order_relaxed);
#endif
  }

  static bool IsAllowedSuperPageForBRPPool(const void* address) {
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);

    // The only potentially dangerous scenario, in which this check is used, is
    // when the assignment of the first |CheckedPtr| object for a non-GigaCage
    // address is racing with the allocation of a new GigCage super-page at the
    // same address. We assume that if |CheckedPtr| is being initialized with a
    // raw pointer, the associated allocation is "alive"; otherwise, the issue
    // should be fixed by rewriting the raw pointer variable as |CheckedPtr|.
    // In the worst case, when such a fix is impossible, we should just undo the
    // |raw pointer -> CheckedPtr| rewrite of the problematic field. If the
    // above assumption holds, the existing allocation will prevent us from
    // reserving the super-page region and, thus, having the race condition.
    // Since we rely on that external synchronization, the relaxed memory
    // ordering should be sufficient.
#if BUILDFLAG(NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST)
    return !brp_forbidden_super_page_map_[address_as_uintptr >> kSuperPageShift]
                .load(std::memory_order_relaxed);
#else
    return super_page_refcount_map_[address_as_uintptr >> kSuperPageShift].load(
               std::memory_order_relaxed) == 0;
#endif
  }
#endif  // BUILDFLAG(USE_BRP_POOL_BLOCKLIST)

 private:
  friend class AddressPoolManager;

  static Lock& GetLock();

  static std::bitset<kNonBRPPoolBits> non_brp_pool_bits_ GUARDED_BY(GetLock());
  static std::bitset<kBRPPoolBits> brp_pool_bits_ GUARDED_BY(GetLock());
#if BUILDFLAG(USE_BRP_POOL_BLOCKLIST)
#if BUILDFLAG(NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST)
  static std::array<std::atomic_bool, kAddressSpaceSize / kSuperPageSize>
      brp_forbidden_super_page_map_;
#endif
  static std::array<std::atomic_uint32_t, kAddressSpaceSize / kSuperPageSize>
      super_page_refcount_map_;
#endif  // BUILDFLAG(USE_BRP_POOL_BLOCKLIST)
};

}  // namespace internal

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAlloc(const void* address) {
  // Currently even when BUILDFLAG(USE_BACKUP_REF_PTR) is off, BRP pool is used
  // for non-BRP allocations, so we have to check both pools regardless of
  // BUILDFLAG(USE_BACKUP_REF_PTR).
  return internal::AddressPoolManagerBitmap::IsManagedByNonBRPPool(address) ||
         internal::AddressPoolManagerBitmap::IsManagedByBRPPool(address);
}

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAllocNonBRPPool(const void* address) {
  return internal::AddressPoolManagerBitmap::IsManagedByNonBRPPool(address);
}

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAllocBRPPool(const void* address) {
  return internal::AddressPoolManagerBitmap::IsManagedByBRPPool(address);
}

}  // namespace base

#endif  // !defined(PA_HAS_64_BITS_POINTERS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_
