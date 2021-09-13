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
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"
#include "base/base_export.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
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

  static ALWAYS_INLINE constexpr uintptr_t NonBRPPoolBaseMask() {
    return kNonBRPPoolBaseMask;
  }

  static ALWAYS_INLINE internal::pool_handle GetBRPPool() { return brp_pool_; }

  // The Configurable Pool can be created inside an existing mapping and so will
  // be located outside PartitionAlloc's GigaCage.
  static ALWAYS_INLINE internal::pool_handle GetConfigurablePool() {
    return configurable_pool_;
  }

  static ALWAYS_INLINE std::pair<pool_handle, uintptr_t> GetPoolAndOffset(
      const void* address) {
    // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
#if !BUILDFLAG(USE_BACKUP_REF_PTR)
    PA_DCHECK(!IsInBRPPool(address));
#endif
    pool_handle pool = 0;
    uintptr_t base = 0;
    if (IsInNonBRPPool(address)) {
      pool = GetNonBRPPool();
      base = non_brp_pool_base_address_;
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    } else if (IsInBRPPool(address)) {
      pool = GetBRPPool();
      base = brp_pool_base_address_;
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
    } else if (IsInConfigurablePool(address)) {
      pool = GetConfigurablePool();
      base = configurable_pool_base_address_;
    } else {
      PA_NOTREACHED();
    }
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
    return std::make_pair(pool, address_as_uintptr - base);
  }
  static ALWAYS_INLINE constexpr size_t ConfigurablePoolReservationSize() {
    return kConfigurablePoolSize;
  }

  // Initialize the GigaCage and the Pools inside of it.
  // This function must only be called from the main thread.
  static void Init();
  // Initialize the ConfigurablePool at the given address.
  // The address must be aligned to the size of the pool. Currently, the size of
  // the pool must always be ConfigurablePoolReservationSize(). In general, the
  // size must be less than or equal to kPoolMaxSize and must be a power of two.
  // This function must only be called from the main thread.
  static void InitConfigurablePool(void* address, size_t size);
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

  static ALWAYS_INLINE bool IsConfigurablePoolInitialized() {
    return configurable_pool_base_address_ != kConfigurablePoolOffsetMask;
  }

  // Returns false for nullptr.
  static ALWAYS_INLINE bool IsInNonBRPPool(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) & kNonBRPPoolBaseMask) ==
           non_brp_pool_base_address_;
  }

  static ALWAYS_INLINE uintptr_t NonBRPPoolBase() {
    return non_brp_pool_base_address_;
  }

  // Returns false for nullptr.
  static ALWAYS_INLINE bool IsInBRPPool(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) & kBRPPoolBaseMask) ==
           brp_pool_base_address_;
  }
  // Returns false for nullptr.
  static ALWAYS_INLINE bool IsInConfigurablePool(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) & kConfigurablePoolBaseMask) ==
           configurable_pool_base_address_;
  }

  static ALWAYS_INLINE uintptr_t ConfigurablePoolBase() {
    return configurable_pool_base_address_;
  }

  static ALWAYS_INLINE uintptr_t OffsetInBRPPool(const void* address) {
    PA_DCHECK(IsInBRPPool(address));
    return reinterpret_cast<uintptr_t>(address) - brp_pool_base_address_;
  }

  // PartitionAddressSpace is static_only class.
  PartitionAddressSpace() = delete;
  PartitionAddressSpace(const PartitionAddressSpace&) = delete;
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;

 private:
  // On 64-bit systems, GigaCage is split into two pools, one with allocations
  // that have a BRP ref-count, and one with allocations that don't.
  //   +----------------+ reserved_base_address_ (8GiB aligned)
  //   |    non-BRP     |     == non_brp_pool_base_address_
  //   |      pool      |
  //   +----------------+ reserved_base_address_ + 8GiB
  //   |      BRP       |     == brp_pool_base_address_
  //   |      pool      |
  //   +----------------+ reserved_base_address_ + 16GiB
  //
  // Pool sizes have to be the power of two. Each pool will be aligned at its
  // own size boundary.
  //
  // NOTE! The BRP pool must be preceded by a reserved region, where allocations
  // are forbidden. This is to prevent a pointer immediately past a non-GigaCage
  // allocation from falling into the BRP pool, thus triggering BRP mechanism
  // and likely crashing. One way to implement this is to place another
  // PartitionAlloc pool right before, because trailing guard pages there will
  // fulfill this guarantee. Alternatively, it could be any region that
  // guarantess to not have allocations extending to its very end. But it's just
  // easier to have non-BRP pool there.
  //
  // If more than 2 consecutive pools are ever needed, care will have to be
  // taken when choosing sizes. For example, for sizes [8GiB,4GiB,8GiB], it'd be
  // impossible to align each pool at its own size boundary while keeping them
  // next to each other. CalculateGigaCageProperties() has non-debug, run-time
  // checks to assert that.
  //
  // The ConfigurablePool is an optional Pool that can be created inside an
  // existing mapping by the embedder, and so will be outside of the GigaCage.
  // This Pool can be used when certain PA allocations must be located inside a
  // given virtual address region. One use case for this Pool is V8's virtual
  // memory cage, which requires that ArrayBuffers be located inside of it.
  static constexpr size_t kNonBRPPoolSize = kPoolMaxSize;
  static constexpr size_t kBRPPoolSize = kPoolMaxSize;
  static constexpr std::array<size_t, 2> kGigaCagePoolSizes = {kNonBRPPoolSize,
                                                               kBRPPoolSize};
  static constexpr size_t kConfigurablePoolSize = 4 * kGiB;
  static_assert(
      kConfigurablePoolSize <= kPoolMaxSize,
      "The Configurable Pool must not be larger than the maximum pool size");
  static_assert(bits::IsPowerOfTwo(kNonBRPPoolSize) &&
                    bits::IsPowerOfTwo(kBRPPoolSize) &&
                    bits::IsPowerOfTwo(kConfigurablePoolSize),
                "Each pool size should be a power of two.");

  // Masks used to easy determine belonging to a pool.
  static constexpr uintptr_t kNonBRPPoolOffsetMask =
      static_cast<uintptr_t>(kNonBRPPoolSize) - 1;
  static constexpr uintptr_t kNonBRPPoolBaseMask = ~kNonBRPPoolOffsetMask;
  static constexpr uintptr_t kBRPPoolOffsetMask =
      static_cast<uintptr_t>(kBRPPoolSize) - 1;
  static constexpr uintptr_t kBRPPoolBaseMask = ~kBRPPoolOffsetMask;
  static constexpr uintptr_t kConfigurablePoolOffsetMask =
      static_cast<uintptr_t>(kConfigurablePoolSize) - 1;
  static constexpr uintptr_t kConfigurablePoolBaseMask =
      ~kConfigurablePoolOffsetMask;

  // See the comment describing the address layout above.
  static uintptr_t reserved_base_address_;
  static uintptr_t non_brp_pool_base_address_;
  static uintptr_t brp_pool_base_address_;
  static uintptr_t configurable_pool_base_address_;

  static pool_handle non_brp_pool_;
  static pool_handle brp_pool_;
  static pool_handle configurable_pool_;
};

ALWAYS_INLINE std::pair<pool_handle, uintptr_t> GetPoolAndOffset(
    const void* address) {
  return PartitionAddressSpace::GetPoolAndOffset(address);
}

ALWAYS_INLINE pool_handle GetPool(const void* address) {
  return std::get<0>(GetPoolAndOffset(address));
}

ALWAYS_INLINE uintptr_t OffsetInBRPPool(const void* address) {
  return PartitionAddressSpace::OffsetInBRPPool(address);
}

#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal

#if defined(PA_HAS_64_BITS_POINTERS)
// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAlloc(const void* address) {
  // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
#if !BUILDFLAG(USE_BACKUP_REF_PTR)
  PA_DCHECK(!internal::PartitionAddressSpace::IsInBRPPool(address));
#endif
  return internal::PartitionAddressSpace::IsInNonBRPPool(address)
#if BUILDFLAG(USE_BACKUP_REF_PTR)
         || internal::PartitionAddressSpace::IsInBRPPool(address)
#endif
         || internal::PartitionAddressSpace::IsInConfigurablePool(address);
}

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAllocNonBRPPool(const void* address) {
  return internal::PartitionAddressSpace::IsInNonBRPPool(address);
}

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAllocBRPPool(const void* address) {
  return internal::PartitionAddressSpace::IsInBRPPool(address);
}

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAllocConfigurablePool(
    const void* address) {
  return internal::PartitionAddressSpace::IsInConfigurablePool(address);
}

ALWAYS_INLINE bool IsConfigurablePoolAvailable() {
  return internal::PartitionAddressSpace::IsConfigurablePoolInitialized();
}
#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
