// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_

#include <algorithm>
#include <array>
#include <cstddef>
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

// Manages PartitionAlloc address space, which is split into pools.
// See `glossary.md`.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) PartitionAddressSpace {
 public:
#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
  static PA_ALWAYS_INLINE uintptr_t RegularPoolBaseMask() {
    return setup_.regular_pool_base_mask_;
  }
#else
  static PA_ALWAYS_INLINE constexpr uintptr_t RegularPoolBaseMask() {
    return kRegularPoolBaseMask;
  }
#endif

  static PA_ALWAYS_INLINE std::pair<pool_handle, uintptr_t> GetPoolAndOffset(
      uintptr_t address) {
    // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
#if !BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    PA_DCHECK(!IsInBRPPool(address));
#endif
    pool_handle pool = 0;
    uintptr_t base = 0;
    if (IsInRegularPool(address)) {
      pool = kRegularPoolHandle;
      base = setup_.regular_pool_base_address_;
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    } else if (IsInBRPPool(address)) {
      pool = kBRPPoolHandle;
      base = setup_.brp_pool_base_address_;
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    } else if (IsInConfigurablePool(address)) {
      PA_DCHECK(IsConfigurablePoolInitialized());
      pool = kConfigurablePoolHandle;
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

  // Initialize pools (except for the configurable one).
  //
  // This function must only be called from the main thread.
  static void Init();
  // Initialize the ConfigurablePool at the given address |pool_base|. It must
  // be aligned to the size of the pool. The size must be a power of two and
  // must be within [ConfigurablePoolMinSize(), ConfigurablePoolMaxSize()].
  //
  // This function must only be called from the main thread.
  static void InitConfigurablePool(uintptr_t pool_base, size_t size);
  static void UninitForTesting();
  static void UninitConfigurablePoolForTesting();

  static PA_ALWAYS_INLINE bool IsInitialized() {
    // Either neither or both regular and BRP pool are initialized. The
    // configurable pool is initialized separately.
    if (setup_.regular_pool_base_address_ != kUninitializedPoolBaseAddress) {
      PA_DCHECK(setup_.brp_pool_base_address_ != kUninitializedPoolBaseAddress);
      return true;
    }

    PA_DCHECK(setup_.brp_pool_base_address_ == kUninitializedPoolBaseAddress);
    return false;
  }

  static PA_ALWAYS_INLINE bool IsConfigurablePoolInitialized() {
    return setup_.configurable_pool_base_address_ !=
           kUninitializedPoolBaseAddress;
  }

  // Returns false for nullptr.
  static PA_ALWAYS_INLINE bool IsInRegularPool(uintptr_t address) {
#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
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
#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
    const uintptr_t brp_pool_base_mask = setup_.brp_pool_base_mask_;
#else
    constexpr uintptr_t brp_pool_base_mask = kBRPPoolBaseMask;
#endif
    return (address & brp_pool_base_mask) == setup_.brp_pool_base_address_;
  }

  static PA_ALWAYS_INLINE uintptr_t OffsetInBRPPool(uintptr_t address) {
    PA_DCHECK(IsInBRPPool(address));
    return address - setup_.brp_pool_base_address_;
  }

  // Returns false for nullptr.
  static PA_ALWAYS_INLINE bool IsInConfigurablePool(uintptr_t address) {
    return (address & setup_.configurable_pool_base_mask_) ==
           setup_.configurable_pool_base_address_;
  }

  static PA_ALWAYS_INLINE uintptr_t ConfigurablePoolBase() {
    return setup_.configurable_pool_base_address_;
  }

#if defined(PA_ENABLE_SHADOW_METADATA)
  static PA_ALWAYS_INLINE std::ptrdiff_t ShadowPoolOffset(pool_handle pool) {
    if (pool == kRegularPoolHandle) {
      return regular_pool_shadow_offset_;
    } else if (pool == kBRPPoolHandle) {
      return brp_pool_shadow_offset_;
    } else {
      // TODO(crbug.com/1362969): Add shadow for configurable pool as well.
      // Shadow is not created for ConfigurablePool for now, so this part should
      // be unreachable.
      PA_NOTREACHED();
      return 0;
    }
  }
#endif

  // PartitionAddressSpace is static_only class.
  PartitionAddressSpace() = delete;
  PartitionAddressSpace(const PartitionAddressSpace&) = delete;
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;

 private:
#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
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
#endif  // defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)

  // On 64-bit systems, PA allocates from several contiguous, mutually disjoint
  // pools. The BRP pool is where all allocations have a BRP ref-count, thus
  // pointers pointing there can use a BRP protection against UaF. Allocations
  // in the other pools don't have that.
  //
  // Pool sizes have to be the power of two. Each pool will be aligned at its
  // own size boundary.
  //
  // NOTE! The BRP pool must be preceded by an inaccessible region. This is to
  // prevent a pointer to the end of a non-BRP-pool allocation from falling into
  // the BRP pool, thus triggering BRP mechanism and likely crashing. This
  // "forbidden zone" can be as small as 1B, but it's simpler to just reserve an
  // allocation granularity unit.
  //
  // The ConfigurablePool is an optional Pool that can be created inside an
  // existing mapping provided by the embedder. This Pool can be used when
  // certain PA allocations must be located inside a given virtual address
  // region. One use case for this Pool is V8 Sandbox, which requires that
  // ArrayBuffers be located inside of it.
  static constexpr size_t kRegularPoolSize = kPoolMaxSize;
  static constexpr size_t kBRPPoolSize = kPoolMaxSize;
  static_assert(base::bits::IsPowerOfTwo(kRegularPoolSize) &&
                base::bits::IsPowerOfTwo(kBRPPoolSize));
#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
  // We can't afford pool sizes as large as kPoolMaxSize on Windows <8.1 (see
  // crbug.com/1101421 and crbug.com/1217759).
  static constexpr size_t kRegularPoolSizeForLegacyWindows = 4 * kGiB;
  static constexpr size_t kBRPPoolSizeForLegacyWindows = 4 * kGiB;
  static_assert(kRegularPoolSizeForLegacyWindows < kRegularPoolSize);
  static_assert(kBRPPoolSizeForLegacyWindows < kBRPPoolSize);
  static_assert(base::bits::IsPowerOfTwo(kRegularPoolSizeForLegacyWindows) &&
                base::bits::IsPowerOfTwo(kBRPPoolSizeForLegacyWindows));
#endif  // defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
  static constexpr size_t kConfigurablePoolMaxSize = kPoolMaxSize;
  static constexpr size_t kConfigurablePoolMinSize = 1 * kGiB;
  static_assert(kConfigurablePoolMinSize <= kConfigurablePoolMaxSize);
  static_assert(base::bits::IsPowerOfTwo(kConfigurablePoolMaxSize) &&
                base::bits::IsPowerOfTwo(kConfigurablePoolMinSize));

#if BUILDFLAG(IS_IOS)

#if !defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
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

#if !defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
  // Masks used to easy determine belonging to a pool.
  static constexpr uintptr_t kRegularPoolOffsetMask =
      static_cast<uintptr_t>(kRegularPoolSize) - 1;
  static constexpr uintptr_t kRegularPoolBaseMask = ~kRegularPoolOffsetMask;
  static constexpr uintptr_t kBRPPoolOffsetMask =
      static_cast<uintptr_t>(kBRPPoolSize) - 1;
  static constexpr uintptr_t kBRPPoolBaseMask = ~kBRPPoolOffsetMask;
#endif  // !defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)

  // This must be set to such a value that IsIn*Pool() always returns false when
  // the pool isn't initialized.
  static constexpr uintptr_t kUninitializedPoolBaseAddress =
      static_cast<uintptr_t>(-1);

  struct PoolSetup {
    // Before PartitionAddressSpace::Init(), no allocation are allocated from a
    // reserved address space. Therefore, set *_pool_base_address_ initially to
    // -1, so that PartitionAddressSpace::IsIn*Pool() always returns false.
    constexpr PoolSetup()
        : regular_pool_base_address_(kUninitializedPoolBaseAddress),
          brp_pool_base_address_(kUninitializedPoolBaseAddress),
          configurable_pool_base_address_(kUninitializedPoolBaseAddress),
#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
          regular_pool_base_mask_(0),
          brp_pool_base_mask_(0),
#endif
          configurable_pool_base_mask_(0) {
    }

    // Using a union to enforce padding.
    union {
      struct {
        uintptr_t regular_pool_base_address_;
        uintptr_t brp_pool_base_address_;
        uintptr_t configurable_pool_base_address_;
#if defined(PA_DYNAMICALLY_SELECT_POOL_SIZE)
        uintptr_t regular_pool_base_mask_;
        uintptr_t brp_pool_base_mask_;
#endif
        uintptr_t configurable_pool_base_mask_;
      };

      char one_cacheline_[kPartitionCachelineSize];
    };
  };
  static_assert(sizeof(PoolSetup) % kPartitionCachelineSize == 0,
                "PoolSetup has to fill a cacheline(s)");

  // See the comment describing the address layout above.
  //
  // These are write-once fields, frequently accessed thereafter. Make sure they
  // don't share a cacheline with other, potentially writeable data, through
  // alignment and padding.
  alignas(kPartitionCachelineSize) static PoolSetup setup_;

#if defined(PA_ENABLE_SHADOW_METADATA)
  static std::ptrdiff_t regular_pool_shadow_offset_;
  static std::ptrdiff_t brp_pool_shadow_offset_;
#endif
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

#if defined(PA_ENABLE_SHADOW_METADATA)
PA_ALWAYS_INLINE std::ptrdiff_t ShadowPoolOffset(pool_handle pool) {
  return PartitionAddressSpace::ShadowPoolOffset(pool);
}
#endif

}  // namespace internal

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAlloc(uintptr_t address) {
  // When ENABLE_BACKUP_REF_PTR_SUPPORT is off, BRP pool isn't used.
#if !BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  PA_DCHECK(!internal::PartitionAddressSpace::IsInBRPPool(address));
#endif
  return internal::PartitionAddressSpace::IsInRegularPool(address)
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
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
