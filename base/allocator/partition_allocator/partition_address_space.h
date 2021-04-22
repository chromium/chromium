// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_

#include <algorithm>

#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
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
  // BRP stands for BackupRefPtr. GigaCage is split into pools, one which
  // supports BackupRefPtr and one that doesn't.
  static ALWAYS_INLINE internal::pool_handle GetNonBRPPool() {
    return non_brp_pool_;
  }
  static ALWAYS_INLINE internal::pool_handle GetBRPPool() { return brp_pool_; }

  static ALWAYS_INLINE constexpr uintptr_t BRPPoolBaseMask() {
    return kBRPPoolBaseMask;
  }

  static void Init();
  static void UninitForTesting();

  static ALWAYS_INLINE bool IsInitialized() {
    if (reserved_base_address_) {
      PA_DCHECK(non_brp_pool_ != 0);
      PA_DCHECK(brp_pool_ != 0);
      return true;
    }

    PA_DCHECK(non_brp_pool_ == 0);
    PA_DCHECK(brp_pool_ == 0);
    return false;
  }

  // Returns false for nullptr.
  static ALWAYS_INLINE bool IsInNonBRPPool(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) & kNonBRPPoolBaseMask) ==
           non_brp_pool_base_address_;
  }
  // Returns false for nullptr.
  static ALWAYS_INLINE bool IsInBRPPool(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) & kBRPPoolBaseMask) ==
           brp_pool_base_address_;
  }

  static ALWAYS_INLINE uintptr_t BRPPoolBase() {
    return brp_pool_base_address_;
  }

  // PartitionAddressSpace is static_only class.
  PartitionAddressSpace() = delete;
  PartitionAddressSpace(const PartitionAddressSpace&) = delete;
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;

 private:
  // Partition Alloc Address Space
  // Reserves 16GiB address space for one pool that supports BackupRefPtr and
  // one that doesn't, 8GiB each.
  // TODO(bartekn): Look into devices with 39-bit address space that have 256GiB
  // user-mode space (most ARM64 Android devices as of 2021). Libraries loaded
  // at random addresses may stand in the way of reserving a contiguous 24GiB
  // region (even though we're requesting only 16GiB, AllocPages may under the
  // covers reserve extra 8GiB to satisfy the alignment requirements).
  //
  // +----------------+ reserved_base_address_ (8GiB aligned)
  // |    non-BRP     |     == non_brp_pool_base_address_
  // |      pool      |
  // +----------------+ reserved_base_address_ + 8GiB
  // |      BRP       |     == brp_pool_base_address_
  // |      pool      |
  // +----------------+ reserved_base_address_ + 16GiB
  //
  // NOTE! On 64-bit systems with BackupRefPtr enabled, the non-BRP pool must
  // precede the BRP pool. This is to prevent a pointer immediately past a
  // non-GigaCage allocation from falling into the BRP pool, thus triggering
  // BackupRefPtr mechanism and likely crashing.

  static constexpr size_t kGigaBytes = 1024 * 1024 * 1024;

  // Pool sizes are flexible, as long as each pool is aligned on its own size
  // boundary and the size is a power of two. The entire region is aligned on
  // the max pool size boundary, so the further pools only need to care about
  // the shift from the beginning of the region (for clarity, the pool sizes are
  // declared in the order the pools are allocated).
  //
  // For example, [8GiB,4GiB,4GiB], [8GiB,4GiB,2GiB,1GiB] would work, but
  // [4GiB,8GiB] wouldn't (the 2nd pool is aligned on 4GiB but needs 8GiB),
  // and [4GiB,4GiB,8GiB] would.
  static constexpr size_t kNonBRPPoolSize = 8 * kGigaBytes;
  static constexpr size_t kBRPPoolSize = 8 * kGigaBytes;

  static constexpr size_t kDesiredAddressSpaceSize =
      kNonBRPPoolSize + kBRPPoolSize;
  static constexpr size_t kReservedAddressSpaceAlignment =
      std::max(kNonBRPPoolSize, kBRPPoolSize);

  static_assert(bits::IsPowerOfTwo(kNonBRPPoolSize) &&
                    bits::IsPowerOfTwo(kBRPPoolSize),
                "Each pool size should be a power of two.");
  static_assert(bits::IsPowerOfTwo(kReservedAddressSpaceAlignment),
                "kReservedAddressSpaceAlignment should be a power of two.");
  static_assert(kReservedAddressSpaceAlignment >= kNonBRPPoolSize &&
                    kReservedAddressSpaceAlignment >= kBRPPoolSize,
                "kReservedAddressSpaceAlignment should be larger or equal to "
                "each pool size.");
  static_assert(kReservedAddressSpaceAlignment % kNonBRPPoolSize == 0 &&
                    (kReservedAddressSpaceAlignment + kNonBRPPoolSize) %
                            kBRPPoolSize ==
                        0,
                "Each pool should be aligned to its own size");

  // Masks used to easy determine belonging to a pool.
  static constexpr uintptr_t kNonBRPPoolOffsetMask =
      static_cast<uintptr_t>(kNonBRPPoolSize) - 1;
  static constexpr uintptr_t kNonBRPPoolBaseMask = ~kNonBRPPoolOffsetMask;
  static constexpr uintptr_t kBRPPoolOffsetMask =
      static_cast<uintptr_t>(kBRPPoolSize) - 1;
  static constexpr uintptr_t kBRPPoolBaseMask = ~kBRPPoolOffsetMask;

  // See the comment describing the address layout above.
  static uintptr_t reserved_base_address_;
  static uintptr_t non_brp_pool_base_address_;
  static uintptr_t brp_pool_base_address_;

  static internal::pool_handle non_brp_pool_;
  static internal::pool_handle brp_pool_;
};

ALWAYS_INLINE internal::pool_handle GetNonBRPPool() {
  // This file is included from checked_ptr.h. This will result in a cycle if it
  // includes partition_alloc_features.h where IsPartitionAllocGigaCageEnabled
  // resides, because it includes Finch headers which may include checked_ptr.h.
  // TODO(bartekn): Uncomment once Finch is no longer used there.
  // PA_DCHECK(IsPartitionAllocGigaCageEnabled());
  return PartitionAddressSpace::GetNonBRPPool();
}

ALWAYS_INLINE internal::pool_handle GetBRPPool() {
  // TODO(bartekn): Uncomment once Finch is no longer used there (see above).
  // PA_DCHECK(IsPartitionAllocGigaCageEnabled());
  return PartitionAddressSpace::GetBRPPool();
}

#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal

#if defined(PA_HAS_64_BITS_POINTERS)
// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAlloc(const void* address) {
  // Currently even when BUILDFLAG(USE_BACKUP_REF_PTR) is off, BRP pool is used
  // for non-BRP allocations, so we have to check both pools regardless of
  // BUILDFLAG(USE_BACKUP_REF_PTR).
  return internal::PartitionAddressSpace::IsInNonBRPPool(address) ||
         internal::PartitionAddressSpace::IsInBRPPool(address);
}

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAllocNonBRPPool(const void* address) {
  return internal::PartitionAddressSpace::IsInNonBRPPool(address);
}

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAllocBRPPool(const void* address) {
  return internal::PartitionAddressSpace::IsInBRPPool(address);
}
#endif

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
