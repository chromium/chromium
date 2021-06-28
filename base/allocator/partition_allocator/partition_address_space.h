// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_

#include <algorithm>
#include <array>
#include <limits>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/base_export.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace base {

namespace internal {

// The feature is not applicable to 32-bit address space.
#if defined(PA_HAS_64_BITS_POINTERS)

struct GigaCageProperties {
  size_t size;
  size_t alignment;
  size_t alignment_offset;
};

template <size_t N>
GigaCageProperties CalculateGigaCageProperties(
    const std::array<size_t, N>& pool_sizes) {
  size_t size_sum = 0;
  size_t alignment = 0;
  size_t alignment_offset;
  // The goal is to find properties such that each pool's start address is
  // aligned to its own size. To achieve that, the largest pool will serve
  // as an anchor (the first one, if there are more) and it'll be used to
  // determine the core alignment. The sizes of pools before the anchor will
  // determine the offset within the core alignment at which the GigaCage will
  // start.
  // If this algorithm doesn't find the proper alignment, it means such an
  // alignment doesn't exist.
  for (size_t pool_size : pool_sizes) {
    PA_CHECK(bits::IsPowerOfTwo(pool_size));
    if (pool_size > alignment) {
      alignment = pool_size;
      // This may underflow, leading to a very high value, so use modulo
      // |alignment| to bring it down.
      alignment_offset = (alignment - size_sum) & (alignment - 1);
    }
    size_sum += pool_size;
  }
  // Use PA_CHECK because we can't correctly proceed if any pool's start address
  // isn't aligned to its own size. Exact initial value of |sample_address|
  // doesn't matter as long as |address % alignment == alignment_offset|.
  uintptr_t sample_address = alignment_offset + 7 * alignment;
  for (size_t pool_size : pool_sizes) {
    PA_CHECK(!(sample_address & (pool_size - 1)));
    sample_address += pool_size;
  }
  return GigaCageProperties{size_sum, alignment, alignment_offset};
}

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

  static ALWAYS_INLINE uintptr_t BRPPoolEnd() {
    return brp_pool_base_address_ + kBRPPoolSize;
  }

  static ALWAYS_INLINE uintptr_t GigaCageOffset(uintptr_t address) {
    PA_DCHECK(address >= reserved_base_address_);
    PA_DCHECK(address < reserved_base_address_ + kTotalSize);
    return address - reserved_base_address_;
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

  // Pool sizes have to be the power of two. Each pool will be aligned at its
  // own size boundary.
  //
  // There are a couple reasons why pools ought to be allocated next to each
  // other:
  //  1. Due to the above restriction, BRP pool has to be preceded by another
  //     pool. Alternatively it could be any region that guarantess to not have
  //     allocations extending to its very end, but it's just easier to have
  //     non-BRP pool there.
  //  2. The ReservationOffsetTable covers the entire GigaCage.
  //
  // Care has to be taken when choosing sizes, if more than 2 pools are needed.
  // For example, with sizes [8GiB,4GiB,8GiB], it'd be impossible to align each
  // pool at its own size boundary while keeping them next to each other.
  // CalculateGigaCageProperties() has non-debug run-time checks to ensure that.
  static constexpr size_t kNonBRPPoolSize = kPoolMaxSize;
  static constexpr size_t kBRPPoolSize = kPoolMaxSize;
  static constexpr size_t kTotalSize = kNonBRPPoolSize + kBRPPoolSize;
  static constexpr std::array<size_t, 2> kPoolSizes = {kNonBRPPoolSize,
                                                       kBRPPoolSize};
  static_assert(bits::IsPowerOfTwo(kNonBRPPoolSize) &&
                    bits::IsPowerOfTwo(kBRPPoolSize),
                "Each pool size should be a power of two.");

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

  static pool_handle non_brp_pool_;
  static pool_handle brp_pool_;

  friend class ReservationOffsetTable;
};

ALWAYS_INLINE pool_handle GetNonBRPPool() {
  return PartitionAddressSpace::GetNonBRPPool();
}

ALWAYS_INLINE pool_handle GetBRPPool() {
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
#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
