// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_

#include <algorithm>
#include <array>
#include <limits>

#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "build/build_config.h"

// The feature is not applicable to 32-bit address space.
#if defined(PA_HAS_64_BITS_POINTERS)

namespace partition_alloc {

namespace internal {

// Reserves address space for PartitionAllocator.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) PartitionAddressSpace {
 public:
  // BRP stands for BackupRefPtr. GigaCage is split into pools, one which
  // supports BackupRefPtr and one that doesn't.
  static PA_ALWAYS_INLINE internal::pool_handle GetRegularPool() {
    return setup_.regular_pool_;
  }

#if defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
  static PA_ALWAYS_INLINE uintptr_t RegularPoolBaseMask() {
    return setup_.regular_pool_base_mask_;
  }
#else
  static PA_ALWAYS_INLINE constexpr uintptr_t RegularPoolBaseMask() {
    return kRegularPoolBaseMask;
  }
#endif

  static PA_ALWAYS_INLINE internal::pool_handle GetBRPPool() {
    return setup_.brp_pool_;
  }

  // The Configurable Pool can be created inside an existing mapping and so will
  // be located outside PartitionAlloc's GigaCage.
  static PA_ALWAYS_INLINE internal::pool_handle GetConfigurablePool() {
    return setup_.configurable_pool_;
  }

  static PA_ALWAYS_INLINE std::pair<pool_handle, uintptr_t> GetPoolAndOffset(
      uintptr_t address) {
    address = ::partition_alloc::internal::UnmaskPtr(address);
    // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
#if !BUILDFLAG(USE_BACKUP_REF_PTR)
    PA_DCHECK(!IsInBRPPool(address));
#endif
    pool_handle pool = 0;
    uintptr_t base = 0;
    if (IsInRegularPool(address)) {
      pool = GetRegularPool();
      base = setup_.regular_pool_base_address_;
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
    return std::make_pair(pool, address - base);
  }
  static PA_ALWAYS_INLINE constexpr size_t ConfigurablePoolMaxSize() {
    return kConfigurablePoolMaxSize;
  }
  static PA_ALWAYS_INLINE constexpr size_t ConfigurablePoolMinSize() {
    return kConfigurablePoolMinSize;
  }

  // Initialize the GigaCage and the Pools inside of it.
  // This function must only be called from the main thread.
  static void Init();
  // Initialize the ConfigurablePool at the given address |pool_base|. It must
  // be aligned to the size of the pool. The size must be a power of two and
  // must be within [ConfigurablePoolMinSize(), ConfigurablePoolMaxSize()]. This
  // function must only be called from the main thread.
  static void InitConfigurablePool(uintptr_t pool_base, size_t size);
  static void UninitForTesting();
  static void UninitConfigurablePoolForTesting();

  static PA_ALWAYS_INLINE bool IsInitialized() {
    // Either neither or both regular and BRP pool are initialized. The
    // configurable pool is initialized separately.
    if (setup_.regular_pool_) {
      PA_DCHECK(setup_.brp_pool_ != 0);
      return true;
    }

    PA_DCHECK(setup_.brp_pool_ == 0);
    return false;
  }

  static PA_ALWAYS_INLINE bool IsConfigurablePoolInitialized() {
    return setup_.configurable_pool_base_address_ !=
           kUninitializedPoolBaseAddress;
  }

  // Returns false for nullptr.
  static PA_ALWAYS_INLINE bool IsInRegularPool(uintptr_t address) {
#if defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
    const uintptr_t regular_pool_base_mask = setup_.regular_pool_base_mask_;
#else
    constexpr uintptr_t regular_pool_base_mask = kRegularPoolBaseMask;
#endif
    return (address & regular_pool_base_mask) ==
           setup_.regular_pool_base_address_;
  }

  static PA_ALWAYS_INLINE uintptr_t RegularPoolBase() {
    return setup_.regular_pool_base_address_;
  }

  // Returns false for nullptr.
  static PA_ALWAYS_INLINE bool IsInBRPPool(uintptr_t address) {
#if defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
    const uintptr_t brp_pool_base_mask = setup_.brp_pool_base_mask_;
#else
    constexpr uintptr_t brp_pool_base_mask = kBRPPoolBaseMask;
#endif
    return (address & brp_pool_base_mask) == setup_.brp_pool_base_address_;
  }
  // Returns false for nullptr.
  static PA_ALWAYS_INLINE bool IsInConfigurablePool(uintptr_t address) {
    return (address & setup_.configurable_pool_base_mask_) ==
           setup_.configurable_pool_base_address_;
  }

  static PA_ALWAYS_INLINE uintptr_t ConfigurablePoolBase() {
    return setup_.configurable_pool_base_address_;
  }

  static PA_ALWAYS_INLINE uintptr_t OffsetInBRPPool(uintptr_t address) {
    PA_DCHECK(IsInBRPPool(address));
    return ::partition_alloc::internal::UnmaskPtr(address) -
           setup_.brp_pool_base_address_;
  }

  // PartitionAddressSpace is static_only class.
  PartitionAddressSpace() = delete;
  PartitionAddressSpace(const PartitionAddressSpace&) = delete;
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;

 private:
#if defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
  static PA_ALWAYS_INLINE size_t RegularPoolSize();
  static PA_ALWAYS_INLINE size_t BRPPoolSize();
#else
  // The pool sizes should be as large as maximum whenever possible.
  constexpr static PA_ALWAYS_INLINE size_t RegularPoolSize() {
    return kRegularPoolSize;
  }
  constexpr static PA_ALWAYS_INLINE size_t BRPPoolSize() {
    return kBRPPoolSize;
  }
#endif  // defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)

  // On 64-bit systems, GigaCage is split into disjoint pools. The BRP pool, is
  // where all allocations have a BRP ref-count, thus pointers pointing there
  // can use a BRP protection against UaF. Allocations in the other pools don't
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
  static constexpr size_t kRegularPoolSize = kPoolMaxSize;
  static constexpr size_t kBRPPoolSize = kPoolMaxSize;
  static_assert(base::bits::IsPowerOfTwo(kRegularPoolSize) &&
                base::bits::IsPowerOfTwo(kBRPPoolSize));
  static constexpr size_t kConfigurablePoolMaxSize = kPoolMaxSize;
  static constexpr size_t kConfigurablePoolMinSize = 1 * kGiB;
  static_assert(kConfigurablePoolMinSize <= kConfigurablePoolMaxSize);
  static_assert(base::bits::IsPowerOfTwo(kConfigurablePoolMaxSize) &&
                base::bits::IsPowerOfTwo(kConfigurablePoolMinSize));

#if BUILDFLAG(IS_IOS)

#if !defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
#error iOS is only supported with a dynamically sized GigaCase.
#endif

  // We can't afford pool sizes as large as kPoolMaxSize in iOS EarlGrey tests,
  // since the test process cannot use an extended virtual address space (see
  // crbug.com/1250788).
  static constexpr size_t kRegularPoolSizeForIOSTestProcess = kGiB / 4;
  static constexpr size_t kBRPPoolSizeForIOSTestProcess = kGiB / 4;
  static_assert(kRegularPoolSizeForIOSTestProcess < kRegularPoolSize);
  static_assert(kBRPPoolSizeForIOSTestProcess < kBRPPoolSize);
  static_assert(base::bits::IsPowerOfTwo(kRegularPoolSizeForIOSTestProcess) &&
                base::bits::IsPowerOfTwo(kBRPPoolSizeForIOSTestProcess));
#endif  // BUILDFLAG(IOS_IOS)

#if !defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
  // Masks used to easy determine belonging to a pool.
  // On Arm, the top byte of each pointer is ignored (meaning there are
  // effectively 256 versions of each valid pointer). 4 bits are used to store
  // tags for Arm's Memory Tagging Extension (MTE). To ensure that tagged
  // pointers are recognized as being in the pool, mask off the top byte with
  // kMemTagUnmask.
  static constexpr uintptr_t kRegularPoolOffsetMask =
      static_cast<uintptr_t>(kRegularPoolSize) - 1;
  static constexpr uintptr_t kRegularPoolBaseMask =
      ~kRegularPoolOffsetMask & kMemTagUnmask;
  static constexpr uintptr_t kBRPPoolOffsetMask =
      static_cast<uintptr_t>(kBRPPoolSize) - 1;
  static constexpr uintptr_t kBRPPoolBaseMask =
      ~kBRPPoolOffsetMask & kMemTagUnmask;
#endif  // !defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)

  // This must be set to such a value that IsIn*Pool() always returns false when
  // the pool isn't initialized.
  static constexpr uintptr_t kUninitializedPoolBaseAddress =
      static_cast<uintptr_t>(-1);

  struct GigaCageSetup {
    // Before PartitionAddressSpace::Init(), no allocation are allocated from a
    // reserved address space. Therefore, set *_pool_base_address_ initially to
    // -1, so that PartitionAddressSpace::IsIn*Pool() always returns false.
    constexpr GigaCageSetup()
        : regular_pool_base_address_(kUninitializedPoolBaseAddress),
          brp_pool_base_address_(kUninitializedPoolBaseAddress),
          configurable_pool_base_address_(kUninitializedPoolBaseAddress),
#if defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
          regular_pool_base_mask_(0),
          brp_pool_base_mask_(0),
#endif
          configurable_pool_base_mask_(0),
          regular_pool_(0),
          brp_pool_(0),
          configurable_pool_(0) {
    }

    // Using a union to enforce padding.
    union {
      struct {
        uintptr_t regular_pool_base_address_;
        uintptr_t brp_pool_base_address_;
        uintptr_t configurable_pool_base_address_;
#if defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
        uintptr_t regular_pool_base_mask_;
        uintptr_t brp_pool_base_mask_;
#endif
        uintptr_t configurable_pool_base_mask_;

        pool_handle regular_pool_;
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

PA_ALWAYS_INLINE std::pair<pool_handle, uintptr_t> GetPoolAndOffset(
    uintptr_t address) {
  return PartitionAddressSpace::GetPoolAndOffset(address);
}

PA_ALWAYS_INLINE pool_handle GetPool(uintptr_t address) {
  return std::get<0>(GetPoolAndOffset(address));
}

PA_ALWAYS_INLINE uintptr_t OffsetInBRPPool(uintptr_t address) {
  return PartitionAddressSpace::OffsetInBRPPool(address);
}

}  // namespace internal

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAlloc(uintptr_t address) {
  // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
#if !BUILDFLAG(USE_BACKUP_REF_PTR)
  PA_DCHECK(!internal::PartitionAddressSpace::IsInBRPPool(address));
#endif
  return internal::PartitionAddressSpace::IsInRegularPool(address)
#if BUILDFLAG(USE_BACKUP_REF_PTR)
         || internal::PartitionAddressSpace::IsInBRPPool(address)
#endif
         || internal::PartitionAddressSpace::IsInConfigurablePool(address);
}

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocRegularPool(uintptr_t address) {
  return internal::PartitionAddressSpace::IsInRegularPool(address);
}

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocBRPPool(uintptr_t address) {
  return internal::PartitionAddressSpace::IsInBRPPool(address);
}

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocConfigurablePool(
    uintptr_t address) {
  return internal::PartitionAddressSpace::IsInConfigurablePool(address);
}

PA_ALWAYS_INLINE bool IsConfigurablePoolAvailable() {
  return internal::PartitionAddressSpace::IsConfigurablePoolInitialized();
}

}  // namespace partition_alloc

#endif  // defined(PA_HAS_64_BITS_POINTERS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
