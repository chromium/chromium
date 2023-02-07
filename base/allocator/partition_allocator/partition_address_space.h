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
#include "base/allocator/partition_allocator/pkey.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "build/build_config.h"

// The feature is not applicable to 32-bit address space.
#if BUILDFLAG(HAS_64_BIT_POINTERS)

namespace partition_alloc {

namespace internal {

// Manages PartitionAlloc address space, which is split into pools.
// See `glossary.md`.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) PartitionAddressSpace {
 public:
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
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
    pool_handle pool = kNullPoolHandle;
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
#if BUILDFLAG(ENABLE_PKEYS)
    } else if (IsInPkeyPool(address)) {
      pool = kPkeyPoolHandle;
      base = setup_.pkey_pool_base_address_;
#endif
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
#if BUILDFLAG(ENABLE_PKEYS)
  static void InitPkeyPool(int pkey);
  static void UninitPkeyPoolForTesting();
#endif
  static void UninitForTesting();
  static void UninitConfigurablePoolForTesting();

  static PA_ALWAYS_INLINE bool IsInitialized() {
    // Either neither or both regular and BRP pool are initialized. The
    // configurable and pkey pool are initialized separately.
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

#if BUILDFLAG(ENABLE_PKEYS)
  static PA_ALWAYS_INLINE bool IsPkeyPoolInitialized() {
    return setup_.pkey_pool_base_address_ != kUninitializedPoolBaseAddress;
  }
#endif

  // Returns false for nullptr.
  static PA_ALWAYS_INLINE bool IsInRegularPool(uintptr_t address) {
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
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
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
    const uintptr_t brp_pool_base_mask = setup_.brp_pool_base_mask_;
#else
    constexpr uintptr_t brp_pool_base_mask = kBRPPoolBaseMask;
#endif
    return (address & brp_pool_base_mask) == setup_.brp_pool_base_address_;
  }

#if PA_CONFIG(GLUE_CORE_POOLS)
  // Checks whether the address belongs to either regular or BRP pool.
  // Returns false for nullptr.
  static PA_ALWAYS_INLINE bool IsInCorePools(uintptr_t address) {
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
    const uintptr_t core_pools_base_mask = setup_.core_pools_base_mask_;
#else
    // When PA_GLUE_CORE_POOLS is on, the BRP pool is placed at the end of the
    // regular pool, effectively forming one virtual pool of a twice bigger
    // size. Adjust the mask appropriately.
    constexpr uintptr_t core_pools_base_mask = kRegularPoolBaseMask << 1;
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
    bool ret =
        (address & core_pools_base_mask) == setup_.regular_pool_base_address_;
    PA_DCHECK(ret == (IsInRegularPool(address) || IsInBRPPool(address)));
    return ret;
  }
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  static PA_ALWAYS_INLINE size_t CorePoolsSize() {
    return RegularPoolSize() * 2;
  }
#else
  static PA_ALWAYS_INLINE constexpr size_t CorePoolsSize() {
    return RegularPoolSize() * 2;
  }
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
#endif  // PA_CONFIG(GLUE_CORE_POOLS)

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

#if BUILDFLAG(ENABLE_PKEYS)
  // Returns false for nullptr.
  static PA_ALWAYS_INLINE bool IsInPkeyPool(uintptr_t address) {
    return (address & kPkeyPoolBaseMask) == setup_.pkey_pool_base_address_;
  }
#endif

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
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
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
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
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

#if BUILDFLAG(ENABLE_PKEYS)
  constexpr static PA_ALWAYS_INLINE size_t PkeyPoolSize() {
    return kPkeyPoolSize;
  }
#endif

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
  static_assert(base::bits::IsPowerOfTwo(kRegularPoolSize));
  static_assert(base::bits::IsPowerOfTwo(kBRPPoolSize));
#if BUILDFLAG(ENABLE_PKEYS)
  static constexpr size_t kPkeyPoolSize = kGiB / 4;
  static_assert(base::bits::IsPowerOfTwo(kPkeyPoolSize));
#endif
  static constexpr size_t kConfigurablePoolMaxSize = kPoolMaxSize;
  static constexpr size_t kConfigurablePoolMinSize = 1 * kGiB;
  static_assert(kConfigurablePoolMinSize <= kConfigurablePoolMaxSize);
  static_assert(base::bits::IsPowerOfTwo(kConfigurablePoolMaxSize));
  static_assert(base::bits::IsPowerOfTwo(kConfigurablePoolMinSize));

#if BUILDFLAG(IS_IOS)

#if !PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
#error iOS is only supported with a dynamically sized GigaCase.
#endif

  // We can't afford pool sizes as large as kPoolMaxSize in iOS EarlGrey tests,
  // since the test process cannot use an extended virtual address space (see
  // crbug.com/1250788).
  static constexpr size_t kRegularPoolSizeForIOSTestProcess = kGiB / 4;
  static constexpr size_t kBRPPoolSizeForIOSTestProcess = kGiB / 4;
  static_assert(kRegularPoolSizeForIOSTestProcess < kRegularPoolSize);
  static_assert(kBRPPoolSizeForIOSTestProcess < kBRPPoolSize);
  static_assert(base::bits::IsPowerOfTwo(kRegularPoolSizeForIOSTestProcess));
  static_assert(base::bits::IsPowerOfTwo(kBRPPoolSizeForIOSTestProcess));
#endif  // BUILDFLAG(IOS_IOS)

#if !PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  // Masks used to easy determine belonging to a pool.
  static constexpr uintptr_t kRegularPoolOffsetMask =
      static_cast<uintptr_t>(kRegularPoolSize) - 1;
  static constexpr uintptr_t kRegularPoolBaseMask = ~kRegularPoolOffsetMask;
  static constexpr uintptr_t kBRPPoolOffsetMask =
      static_cast<uintptr_t>(kBRPPoolSize) - 1;
  static constexpr uintptr_t kBRPPoolBaseMask = ~kBRPPoolOffsetMask;
#endif  // !PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

#if BUILDFLAG(ENABLE_PKEYS)
  static constexpr uintptr_t kPkeyPoolOffsetMask =
      static_cast<uintptr_t>(kPkeyPoolSize) - 1;
  static constexpr uintptr_t kPkeyPoolBaseMask = ~kPkeyPoolOffsetMask;
#endif

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
#if BUILDFLAG(ENABLE_PKEYS)
          pkey_pool_base_address_(kUninitializedPoolBaseAddress),
#endif
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
          regular_pool_base_mask_(0),
          brp_pool_base_mask_(0),
#if PA_CONFIG(GLUE_CORE_POOLS)
          core_pools_base_mask_(0),
#endif
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
          configurable_pool_base_mask_(0)
#if BUILDFLAG(ENABLE_PKEYS)
          ,
          pkey_(kInvalidPkey)
#endif
    {
    }

    // Using a union to enforce padding.
    union {
      struct {
        uintptr_t regular_pool_base_address_;
        uintptr_t brp_pool_base_address_;
        uintptr_t configurable_pool_base_address_;
#if BUILDFLAG(ENABLE_PKEYS)
        uintptr_t pkey_pool_base_address_;
#endif
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
        uintptr_t regular_pool_base_mask_;
        uintptr_t brp_pool_base_mask_;
#if PA_CONFIG(GLUE_CORE_POOLS)
        uintptr_t core_pools_base_mask_;
#endif
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
        uintptr_t configurable_pool_base_mask_;
#if BUILDFLAG(ENABLE_PKEYS)
        int pkey_;
#endif
      };

#if BUILDFLAG(ENABLE_PKEYS)
      // With pkey support, we want to be able to pkey-tag all global metadata
      // which requires page granularity.
      char one_page_[SystemPageSize()];
#else
      char one_cacheline_[kPartitionCachelineSize];
#endif
    };
  };
#if BUILDFLAG(ENABLE_PKEYS)
  static_assert(sizeof(PoolSetup) % SystemPageSize() == 0,
                "PoolSetup has to fill a page(s)");
#else
  static_assert(sizeof(PoolSetup) % kPartitionCachelineSize == 0,
                "PoolSetup has to fill a cacheline(s)");
#endif

  // See the comment describing the address layout above.
  //
  // These are write-once fields, frequently accessed thereafter. Make sure they
  // don't share a cacheline with other, potentially writeable data, through
  // alignment and padding.
#if BUILDFLAG(ENABLE_PKEYS)
  static_assert(PA_PKEY_ALIGN_SZ >= kPartitionCachelineSize);
  alignas(PA_PKEY_ALIGN_SZ)
#else
  alignas(kPartitionCachelineSize)
#endif
      static PoolSetup setup_ PA_CONSTINIT;

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  static std::ptrdiff_t regular_pool_shadow_offset_;
  static std::ptrdiff_t brp_pool_shadow_offset_;
#endif

#if BUILDFLAG(ENABLE_PKEYS)
  // If we use a pkey pool, we need to tag its metadata with the pkey. Allow the
  // function to get access to the PoolSetup.
  friend void TagGlobalsWithPkey(int pkey);
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

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
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
#if BUILDFLAG(ENABLE_PKEYS)
         || internal::PartitionAddressSpace::IsInPkeyPool(address)
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

#if PA_CONFIG(GLUE_CORE_POOLS)
// Checks whether the address belongs to either regular or BRP pool.
// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocCorePools(uintptr_t address) {
  return internal::PartitionAddressSpace::IsInCorePools(address);
}
#endif  // PA_CONFIG(GLUE_CORE_POOLS)

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocConfigurablePool(
    uintptr_t address) {
  return internal::PartitionAddressSpace::IsInConfigurablePool(address);
}

#if BUILDFLAG(ENABLE_PKEYS)
// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocPkeyPool(uintptr_t address) {
  return internal::PartitionAddressSpace::IsInPkeyPool(address);
}
#endif

PA_ALWAYS_INLINE bool IsConfigurablePoolAvailable() {
  return internal::PartitionAddressSpace::IsConfigurablePoolInitialized();
}

}  // namespace partition_alloc

#endif  // BUILDFLAG(HAS_64_BIT_POINTERS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
