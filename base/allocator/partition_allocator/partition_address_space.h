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
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"
#include "base/base_export.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/memory/tagging.h"
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
    return setup_.non_brp_pool_;
  }

  static ALWAYS_INLINE constexpr uintptr_t NonBRPPoolBaseMask() {
    return kNonBRPPoolBaseMask;
  }

  static ALWAYS_INLINE internal::pool_handle GetBRPPool() {
    return setup_.brp_pool_;
  }

  // The Configurable Pool can be created inside an existing mapping and so will
  // be located outside PartitionAlloc's GigaCage.
  static ALWAYS_INLINE internal::pool_handle GetConfigurablePool() {
    return setup_.configurable_pool_;
  }

  static ALWAYS_INLINE std::pair<pool_handle, uintptr_t> GetPoolAndOffset(
      const void* address) {
    address = memory::UnmaskPtr(address);
    // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
#if !BUILDFLAG(USE_BACKUP_REF_PTR)
    PA_DCHECK(!IsInBRPPool(address));
#endif
    pool_handle pool = 0;
    uintptr_t base = 0;
    if (IsInNonBRPPool(address)) {
      pool = GetNonBRPPool();
      base = setup_.non_brp_pool_base_address_;
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    } else if (IsInBRPPool(address)) {
      pool = GetBRPPool();
      base = setup_.brp_pool_base_address_;
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
    } else if (IsInConfigurablePool(address)) {
      pool = GetConfigurablePool();
      base = setup_.configurable_pool_base_address_;
    } else {
      PA_NOTREACHED();
    }
    uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
    return std::make_pair(pool, address_as_uintptr - base);
  }
  static ALWAYS_INLINE constexpr size_t ConfigurablePoolMaxSize() {
    return kConfigurablePoolMaxSize;
  }
  static ALWAYS_INLINE constexpr size_t ConfigurablePoolMinSize() {
    return kConfigurablePoolMinSize;
  }

  // Initialize the GigaCage and the Pools inside of it.
  // This function must only be called from the main thread.
  static void Init();
  // Initialize the ConfigurablePool at the given address.  The address must be
  // aligned to the size of the pool. The size must be a power of two and must
  // be within [ConfigurablePoolMinSize(), ConfigurablePoolMaxSize()]. This
  // function must only be called from the main thread.
  static void InitConfigurablePool(void* address, size_t size);
  static void UninitForTesting();
  static void UninitConfigurablePoolForTesting();

  static ALWAYS_INLINE bool IsInitialized() {
    // Either neither or both non-BRP and BRP pool are initialized. The
    // configurable pool is initialized separately.
    if (setup_.non_brp_pool_) {
      PA_DCHECK(setup_.brp_pool_ != 0);
      return true;
    }

    PA_DCHECK(setup_.brp_pool_ == 0);
    return false;
  }

  static ALWAYS_INLINE bool IsConfigurablePoolInitialized() {
    return setup_.configurable_pool_base_address_ !=
           kConfigurablePoolInitialBaseAddress;
  }

  // Returns false for nullptr.
  static ALWAYS_INLINE bool IsInNonBRPPool(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) & kNonBRPPoolBaseMask) ==
           setup_.non_brp_pool_base_address_;
  }

  static ALWAYS_INLINE uintptr_t NonBRPPoolBase() {
    return setup_.non_brp_pool_base_address_;
  }

  // Returns false for nullptr.
  static ALWAYS_INLINE bool IsInBRPPool(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) & kBRPPoolBaseMask) ==
           setup_.brp_pool_base_address_;
  }
  // Returns false for nullptr.
  static ALWAYS_INLINE bool IsInConfigurablePool(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) &
            setup_.configurable_pool_base_mask_) ==
           setup_.configurable_pool_base_address_;
  }

  static ALWAYS_INLINE uintptr_t ConfigurablePoolBase() {
    return setup_.configurable_pool_base_address_;
  }

  static ALWAYS_INLINE uintptr_t OffsetInBRPPool(const void* address) {
    PA_DCHECK(IsInBRPPool(address));
    return reinterpret_cast<uintptr_t>(memory::UnmaskPtr(address)) -
           setup_.brp_pool_base_address_;
  }

  // PartitionAddressSpace is static_only class.
  PartitionAddressSpace() = delete;
  PartitionAddressSpace(const PartitionAddressSpace&) = delete;
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;

 private:
  // On 64-bit systems, GigaCage is split into disjoint pools. The BRP pool, is
  // where all allocations have a BRP ref-count, thus pointers pointing there
  // can use a BRP protection against UaF. Allocations in the non-BRP pool don't
  // have that.
  //
  // Pool sizes have to be the power of two. Each pool will be aligned at its
  // own size boundary.
  //
  // NOTE! The BRP pool must be preceded by a reserved region, where allocations
  // are forbidden. This is to prevent a pointer immediately past a non-GigaCage
  // allocation from falling into the BRP pool, thus triggering BRP mechanism
  // and likely crashing. This "forbidden zone" can be as small as 1B, but it's
  // simpler to just reserve an allocation granularity unit.
  //
  // The ConfigurablePool is an optional Pool that can be created inside an
  // existing mapping by the embedder, and so will be outside of the GigaCage.
  // This Pool can be used when certain PA allocations must be located inside a
  // given virtual address region. One use case for this Pool is V8's virtual
  // memory cage, which requires that ArrayBuffers be located inside of it.
  static constexpr size_t kNonBRPPoolSize = kPoolMaxSize;
  static constexpr size_t kBRPPoolSize = kPoolMaxSize;
  static constexpr size_t kConfigurablePoolMaxSize = kPoolMaxSize;
  static constexpr size_t kConfigurablePoolMinSize = 1 * kGiB;
  static_assert(bits::IsPowerOfTwo(kNonBRPPoolSize) &&
                    bits::IsPowerOfTwo(kBRPPoolSize) &&
                    bits::IsPowerOfTwo(kConfigurablePoolMaxSize) &&
                    bits::IsPowerOfTwo(kConfigurablePoolMinSize),
                "Each pool size should be a power of two.");

  // Masks used to easy determine belonging to a pool.
  // On Arm, the top byte of each pointer is ignored (meaning there are
  // effectively 256 versions of each valid pointer). 4 bits are used to store
  // tags for Arm's Memory Tagging Extension (MTE). To ensure that tagged
  // pointers are recognized as being in the pool, mask off the top byte with
  // kMemTagUnmask.
  static constexpr uintptr_t kNonBRPPoolOffsetMask =
      static_cast<uintptr_t>(kNonBRPPoolSize) - 1;
  static constexpr uintptr_t kNonBRPPoolBaseMask =
      ~kNonBRPPoolOffsetMask & kMemTagUnmask;
  static constexpr uintptr_t kBRPPoolOffsetMask =
      static_cast<uintptr_t>(kBRPPoolSize) - 1;
  static constexpr uintptr_t kBRPPoolBaseMask =
      ~kBRPPoolOffsetMask & kMemTagUnmask;

  // This must be != 0 so that IsInConfigurablePool always returns false
  // when the pool isn't initialized.
  static constexpr uintptr_t kConfigurablePoolInitialBaseAddress =
      static_cast<uintptr_t>(-1);

  struct GigaCageSetup {
    // Before PartitionAddressSpace::Init(), no allocation are allocated from a
    // reserved address space. Therefore, set *_pool_base_address_ initially to
    // k*PoolOffsetMask, or kConfigurablePoolInitialBaseAddress for the
    // ConfigurablePool, so that PartitionAddressSpace::IsIn*Pool() always
    // returns false.
    constexpr GigaCageSetup()
        : non_brp_pool_base_address_(kNonBRPPoolOffsetMask),
          brp_pool_base_address_(kBRPPoolOffsetMask),
          configurable_pool_base_address_(kConfigurablePoolInitialBaseAddress),
          configurable_pool_base_mask_(0),
          non_brp_pool_(0),
          brp_pool_(0),
          configurable_pool_(0) {}

    // Using a union to enforce padding.
    union {
      struct {
        uintptr_t non_brp_pool_base_address_;
        uintptr_t brp_pool_base_address_;
        uintptr_t configurable_pool_base_address_;
        uintptr_t configurable_pool_base_mask_;

        pool_handle non_brp_pool_;
        pool_handle brp_pool_;
        pool_handle configurable_pool_;
      };

      char one_cacheline_[kPartitionCachelineSize];
    };
  };
  static_assert(sizeof(GigaCageSetup) % kPartitionCachelineSize == 0,
                "GigaCageSetup has to fill a cacheline(s)");

  // See the comment describing the address layout above.
  //
  // These are write-once fields, frequently accessed thereafter. Make sure they
  // don't share a cacheline with other, potentially writeable data, through
  // alignment and padding.
  alignas(kPartitionCachelineSize) static GigaCageSetup setup_;
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
