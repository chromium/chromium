// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_INTERNAL_PARTITION_ADDRESS_SPACE_INTERNAL_H_
#define PARTITION_ALLOC_INTERNAL_PARTITION_ADDRESS_SPACE_INTERNAL_H_

#include <cstddef>
#include <utility>

#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc_base/notreached.h"

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
#include "partition_alloc/thread_isolation/thread_isolation.h"
#endif

// The feature is not applicable to 32-bit address space.
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)

namespace partition_alloc {

namespace internal {

PA_ALWAYS_INLINE PartitionAddressSpace::PoolInfo
PartitionAddressSpace::GetPoolInfo(uintptr_t address) {
  // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
#if !PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  PA_DCHECK(!IsInBRPPool(address));
#endif
  pool_handle pool = kNullPoolHandle;
  uintptr_t base = 0;
  uintptr_t base_mask = 0;
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (IsInBRPPool(address)) {
    pool = kBRPPoolHandle;
    base = setup_.brp_pool_base_address_;
    base_mask = CorePoolBaseMask();
  } else
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    if (IsInRegularPool(address)) {
      pool = kRegularPoolHandle;
      base = setup_.regular_pool_base_address_;
      base_mask = CorePoolBaseMask();
    } else if (IsInConfigurablePool(address)) {
      PA_DCHECK(IsConfigurablePoolInitialized());
      pool = kConfigurablePoolHandle;
      base = setup_.configurable_pool_base_address_;
      base_mask = setup_.configurable_pool_base_mask_;
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
    } else if (IsInThreadIsolatedPool(address)) {
      pool = kThreadIsolatedPoolHandle;
      base = setup_.thread_isolated_pool_base_address_;
      base_mask = kThreadIsolatedPoolBaseMask;
#endif
    } else {
      PA_NOTREACHED();
    }
  return PartitionAddressSpace::PoolInfo{.handle = pool,
                                         .base = base,
                                         .base_mask = base_mask,
                                         .offset = address - base};
}

PA_ALWAYS_INLINE PoolOffsetLookup
PartitionAddressSpace::GetOffsetLookup(pool_handle pool) {
  switch (pool) {
    case kRegularPoolHandle:
      return PoolOffsetLookup(setup_.regular_pool_base_address_,
                              CorePoolBaseMask());
    case kBRPPoolHandle:
      return PoolOffsetLookup(setup_.brp_pool_base_address_,
                              CorePoolBaseMask());
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
    case kThreadIsolatedPoolHandle:
      return PoolOffsetLookup(setup_.thread_isolated_pool_base_address_,
                              kThreadIsolatedPoolBaseMask);
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
    case kConfigurablePoolHandle:
      return PoolOffsetLookup(setup_.configurable_pool_base_address_,
                              setup_.configurable_pool_base_mask_);
    default:
      PA_NOTREACHED();
  }
}

PA_ALWAYS_INLINE bool PartitionAddressSpace::IsInitialized() {
  // Either neither or both regular and BRP pool are initialized. The
  // configurable and thread isolated pool are initialized separately.
  if (setup_.regular_pool_base_address_ != kUninitializedPoolBaseAddress) {
    PA_DCHECK(setup_.brp_pool_base_address_ != kUninitializedPoolBaseAddress);
    return true;
  }

  PA_DCHECK(setup_.brp_pool_base_address_ == kUninitializedPoolBaseAddress);
  return false;
}

PA_ALWAYS_INLINE bool PartitionAddressSpace::IsConfigurablePoolInitialized() {
  return setup_.configurable_pool_base_address_ !=
         kUninitializedPoolBaseAddress;
}

PA_ALWAYS_INLINE uintptr_t PartitionAddressSpace::RegularPoolBase() {
  return setup_.regular_pool_base_address_;
}

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
PA_ALWAYS_INLINE uintptr_t PartitionAddressSpace::BRPPoolBase() {
  return RegularPoolBase() + CorePoolSize();
}
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)

PA_ALWAYS_INLINE uintptr_t
PartitionAddressSpace::OffsetInBRPPool(uintptr_t address) {
  PA_DCHECK(IsInBRPPool(address));
  return address - setup_.brp_pool_base_address_;
}

PA_ALWAYS_INLINE uintptr_t PartitionAddressSpace::ConfigurablePoolBase() {
  return setup_.configurable_pool_base_address_;
}

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
// The MetadataRegionSize() returns the size of address space of metadata.
// The address space contains all metadata for all pools (i.e. regular, brp,
// and configurable pools).
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
PA_ALWAYS_INLINE size_t PartitionAddressSpace::MetadataRegionSize() {
  return metadata_region_size_;
}
#else
PA_ALWAYS_INLINE constexpr size_t PartitionAddressSpace::MetadataRegionSize() {
  return std::max(kConfigurablePoolMaxSize, CorePoolSize());
}
#endif

// Returns a metadata offset. SuperPage address plus the offset contains
// the metadata for the SuperPage.
PA_ALWAYS_INLINE std::ptrdiff_t PartitionAddressSpace::MetadataOffset(
    pool_handle pool) {
  return setup_.offsets_to_metadata_[pool];
}

PA_ALWAYS_INLINE std::ptrdiff_t PartitionAddressSpace::MetadataOffsetFromAddr(
    uintptr_t address) {
  return MetadataOffset(GetPoolHandle(address));
}

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
PA_ALWAYS_INLINE bool PartitionAddressSpace::IsInMetadataRegion(
    uintptr_t address) {
  return setup_.metadata_region_start_ <= address &&
         address < setup_.metadata_region_start_ + MetadataRegionSize();
}
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

PA_ALWAYS_INLINE pool_handle
PartitionAddressSpace::GetPoolHandle(uintptr_t address) {
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (IsInBRPPool(address)) [[likely]] {
    return kBRPPoolHandle;
  }
#endif
  if (IsInRegularPool(address)) [[likely]] {
    return kRegularPoolHandle;
  }
  if (IsInConfigurablePool(address)) {
    return kConfigurablePoolHandle;
  }
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  if (IsInThreadIsolatedPool(address)) {
    return kThreadIsolatedPoolHandle;
  }
#endif
  return kNullPoolHandle;
}
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)

PA_ALWAYS_INLINE PartitionAddressSpace::PoolInfo GetPoolInfo(
    uintptr_t address) {
  return PartitionAddressSpace::GetPoolInfo(address);
}

PA_ALWAYS_INLINE pool_handle GetPool(uintptr_t address) {
  return GetPoolInfo(address).handle;
}

PA_ALWAYS_INLINE uintptr_t OffsetInBRPPool(uintptr_t address) {
  return PartitionAddressSpace::OffsetInBRPPool(address);
}

}  // namespace internal

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocRegularPool(uintptr_t address) {
  return internal::PartitionAddressSpace::IsInRegularPool(address);
}

// Checks whether the address belongs to either regular or BRP pool.
// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocCorePools(uintptr_t address) {
  return internal::PartitionAddressSpace::IsInCorePools(address);
}

// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocConfigurablePool(
    uintptr_t address) {
  return internal::PartitionAddressSpace::IsInConfigurablePool(address);
}

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
// Returns false for nullptr.
PA_ALWAYS_INLINE bool IsManagedByPartitionAllocThreadIsolatedPool(
    uintptr_t address) {
  return internal::PartitionAddressSpace::IsInThreadIsolatedPool(address);
}
#endif

PA_ALWAYS_INLINE bool IsConfigurablePoolAvailable() {
  return internal::PartitionAddressSpace::IsConfigurablePoolInitialized();
}

}  // namespace partition_alloc

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

// To reduce boilerplate code include some simple free functions directly for
// both 32-bit and 64-bit platforms. These should always be trivial either
// calling PartitionAddressSpace or having simple values to compute.
namespace partition_alloc::internal {
// On 32-bit platforms, METADATA_OUT_OF_GIGACAGE is not supported and thus
// offset is always 0.
PA_ALWAYS_INLINE std::ptrdiff_t GetMetadataOffset(pool_handle pool) {
#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  return PartitionAddressSpace::MetadataOffset(pool);
#else
  return 0;
#endif
}

// On 32-bit platforms, METADATA_OUT_OF_GIGACAGE is not supported and thus
// offset is always 0.
PA_ALWAYS_INLINE std::ptrdiff_t GetMetadataOffsetFromAddr(uintptr_t address) {
#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  return PartitionAddressSpace::MetadataOffsetFromAddr(address);
#else
  return 0;
#endif
}
}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_INTERNAL_PARTITION_ADDRESS_SPACE_INTERNAL_H_
