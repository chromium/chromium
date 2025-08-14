// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/partition_address_space.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/allocator_config.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/compressed_pointer.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/debug/alias.h"
#include "partition_alloc/partition_alloc_base/files/platform_file.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/thread_isolation/thread_isolation.h"

#if PA_BUILDFLAG(IS_IOS)
#include <mach-o/dyld.h>
#endif

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // PA_BUILDFLAG(IS_WIN)

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
#include <sys/mman.h>
#endif

namespace partition_alloc::internal {

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)

namespace {

#if PA_BUILDFLAG(IS_WIN)

PA_NOINLINE void HandlePoolAllocFailureOutOfVASpace() {
  PA_NO_CODE_FOLDING();
  PA_CHECK(false);
}

PA_NOINLINE void HandlePoolAllocFailureOutOfCommitCharge() {
  PA_NO_CODE_FOLDING();
  PA_CHECK(false);
}
#endif  // PA_BUILDFLAG(IS_WIN)

PA_NOINLINE void HandlePoolAllocFailure() {
  PA_NO_CODE_FOLDING();
  uint32_t alloc_page_error_code = GetAllocPageErrorCode();
  PA_DEBUG_DATA_ON_STACK("error", static_cast<size_t>(alloc_page_error_code));
  // It's important to easily differentiate these two failures on Windows, so
  // crash with different stacks.
#if PA_BUILDFLAG(IS_WIN)
  if (alloc_page_error_code == ERROR_NOT_ENOUGH_MEMORY) {
    // The error code says NOT_ENOUGH_MEMORY, but since we only do MEM_RESERVE,
    // it must be VA space exhaustion.
    HandlePoolAllocFailureOutOfVASpace();
  } else if (alloc_page_error_code == ERROR_COMMITMENT_LIMIT ||
             alloc_page_error_code == ERROR_COMMITMENT_MINIMUM) {
    // Should not happen, since as of Windows 8.1+, reserving address space
    // should not be charged against the commit limit, aside from a very small
    // amount per 64kiB block. Keep this path anyway, to check in crash reports.
    HandlePoolAllocFailureOutOfCommitCharge();
  } else
#endif  // PA_BUILDFLAG(IS_WIN)
  {
    PA_CHECK(false);
  }
}

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
MetadataInnerOffset(pool_handle pool) {
  PA_DCHECK(kRegularPoolHandle <= pool && pool < kMaxPoolHandle);
  // Each metadata needs 2 SystemPage, i.e. 1 SystemPage for Guardian
  // and another SystemPage for actual metadata.
  // To make the first SystemPage a guardian, need `+ 1`.
  return SystemPageSize() * (2 * (pool - kRegularPoolHandle) + 1);
}
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)

}  // namespace

PA_CONSTINIT PartitionAddressSpace::PoolSetup PartitionAddressSpace::setup_;

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
PA_CONSTINIT std::array<std::ptrdiff_t, kMaxPoolHandle>
    PartitionAddressSpace::offsets_to_metadata_ = {
        0, 0, 0, 0,
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
        0,
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
};

uintptr_t PartitionAddressSpace::metadata_region_start_ =
    PartitionAddressSpace::kUninitializedPoolBaseAddress;

#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
size_t PartitionAddressSpace::metadata_region_size_ = 0;
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)

#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
#if !PA_BUILDFLAG(IS_IOS)
#error Dynamic pool size is only supported on iOS.
#endif

bool PartitionAddressSpace::IsIOSTestProcess() {
  // On iOS, only applications with the extended virtual addressing entitlement
  // can use a large address space. Since Earl Grey test runner apps cannot get
  // entitlements, they must use a much smaller pool size. Similarly,
  // integration tests for ChromeWebView end up with two PartitionRoots since
  // both the integration tests and ChromeWebView have a copy of base/. Even
  // with the entitlement, there is insufficient address space for two
  // PartitionRoots, so a smaller pool size is needed.

  // Use a fixed buffer size to avoid allocation inside the allocator.
  constexpr size_t path_buffer_size = 8192;
  char executable_path[path_buffer_size];

  uint32_t executable_length = path_buffer_size;
  int rv = _NSGetExecutablePath(executable_path, &executable_length);
  PA_CHECK(!rv);
  size_t executable_path_length =
      std::char_traits<char>::length(executable_path);

  auto has_suffix = [&](const char* suffix) -> bool {
    size_t suffix_length = std::char_traits<char>::length(suffix);
    if (executable_path_length < suffix_length) {
      return false;
    }
    return std::char_traits<char>::compare(
               executable_path + (executable_path_length - suffix_length),
               suffix, suffix_length) == 0;
  };

  return has_suffix("Runner") || has_suffix("ios_web_view_inttests");
}
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

void PartitionAddressSpace::Init() {
  if (IsInitialized()) {
    return;
  }

  const size_t core_pool_size = CorePoolSize();

  size_t glued_pool_sizes = core_pool_size * 2;
  // Note, BRP pool requires to be preceded by a "forbidden zone", which is
  // conveniently taken care of by the last guard page of the regular pool.
  setup_.regular_pool_base_address_ =
      AllocPages(glued_pool_sizes, glued_pool_sizes,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc);
#if PA_BUILDFLAG(IS_ANDROID)
  // On Android, Adreno-GSL library fails to mmap if we snatch address
  // 0x400000000. Find a different address instead.
  if (setup_.regular_pool_base_address_ == 0x400000000) {
    uintptr_t new_base_address =
        AllocPages(glued_pool_sizes, glued_pool_sizes,
                   PageAccessibilityConfiguration(
                       PageAccessibilityConfiguration::kInaccessible),
                   PageTag::kPartitionAlloc);
    FreePages(setup_.regular_pool_base_address_, glued_pool_sizes);
    setup_.regular_pool_base_address_ = new_base_address;
  }
#endif  // PA_BUILDFLAG(IS_ANDROID)
  if (!setup_.regular_pool_base_address_) {
    HandlePoolAllocFailure();
  }
  setup_.brp_pool_base_address_ =
      setup_.regular_pool_base_address_ + core_pool_size;

#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  setup_.core_pool_base_mask_ = ~(core_pool_size - 1);
  // The BRP pool is placed at the end of the regular pool, effectively forming
  // one virtual pool of a twice bigger size. Adjust the mask appropriately.
  setup_.glued_pools_base_mask_ = setup_.core_pool_base_mask_ << 1;
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

  AddressPoolManager::GetInstance().Add(
      kRegularPoolHandle, setup_.regular_pool_base_address_, core_pool_size);
  AddressPoolManager::GetInstance().Add(
      kBRPPoolHandle, setup_.brp_pool_base_address_, core_pool_size);

  // Sanity check pool alignment.
  PA_DCHECK(!(setup_.regular_pool_base_address_ & (core_pool_size - 1)));
  PA_DCHECK(!(setup_.brp_pool_base_address_ & (core_pool_size - 1)));
  PA_DCHECK(!(setup_.regular_pool_base_address_ & (glued_pool_sizes - 1)));

  // Sanity check pool belonging.
  PA_DCHECK(!IsInRegularPool(setup_.regular_pool_base_address_ - 1));
  PA_DCHECK(IsInRegularPool(setup_.regular_pool_base_address_));
  PA_DCHECK(
      IsInRegularPool(setup_.regular_pool_base_address_ + core_pool_size - 1));
  PA_DCHECK(
      !IsInRegularPool(setup_.regular_pool_base_address_ + core_pool_size));
  PA_DCHECK(!IsInBRPPool(setup_.brp_pool_base_address_ - 1));
  PA_DCHECK(IsInBRPPool(setup_.brp_pool_base_address_));
  PA_DCHECK(IsInBRPPool(setup_.brp_pool_base_address_ + core_pool_size - 1));
  PA_DCHECK(!IsInBRPPool(setup_.brp_pool_base_address_ + core_pool_size));
  PA_DCHECK(!IsInCorePools(setup_.regular_pool_base_address_ - 1));
  PA_DCHECK(IsInCorePools(setup_.regular_pool_base_address_));
  PA_DCHECK(
      IsInCorePools(setup_.regular_pool_base_address_ + core_pool_size - 1));
  PA_DCHECK(IsInCorePools(setup_.regular_pool_base_address_ + core_pool_size));
  PA_DCHECK(IsInCorePools(setup_.brp_pool_base_address_ - 1));
  PA_DCHECK(IsInCorePools(setup_.brp_pool_base_address_));
  PA_DCHECK(IsInCorePools(setup_.brp_pool_base_address_ + core_pool_size - 1));
  PA_DCHECK(!IsInCorePools(setup_.brp_pool_base_address_ + core_pool_size));

#if PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
  CompressedPointerBaseGlobal::SetBase(setup_.regular_pool_base_address_);
#endif  // PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  InitMetadataRegionAndOffsets();
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
}

void PartitionAddressSpace::InitConfigurablePool(uintptr_t pool_base,
                                                 size_t size) {
  // The ConfigurablePool must only be initialized once.
  PA_CHECK(!IsConfigurablePoolInitialized());

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // It's possible that the thread isolated pool has been initialized first, in
  // which case the setup_ memory has been made read-only. Remove the protection
  // temporarily.
  if (IsThreadIsolatedPoolInitialized()) {
    UnprotectThreadIsolatedGlobals();
  }
#endif

  PA_CHECK(pool_base);
  PA_CHECK(size <= kConfigurablePoolMaxSize);
  PA_CHECK(size >= kConfigurablePoolMinSize);
  PA_CHECK(base::bits::HasSingleBit(size));
  PA_CHECK(pool_base % size == 0);

  setup_.configurable_pool_base_address_ = pool_base;
  setup_.configurable_pool_base_mask_ = ~(size - 1);

  AddressPoolManager::GetInstance().Add(
      kConfigurablePoolHandle, setup_.configurable_pool_base_address_, size);

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // Put the metadata protection back in place.
  if (IsThreadIsolatedPoolInitialized()) {
    WriteProtectThreadIsolatedGlobals(setup_.thread_isolation_);
  }
#endif

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  // Initialize metadata for configurable pool without PartitionAlloc enabled.
  // This will happen when there exists a test which uses configurable pool
  // and a sanitizer is enabled.
  if (metadata_region_start_ != kUninitializedPoolBaseAddress) {
    // Set offset from ConfigurablePool to MetadataRegion.
    offsets_to_metadata_[kConfigurablePoolHandle] =
        metadata_region_start_ - ConfigurablePoolBase() +
        MetadataInnerOffset(kConfigurablePoolHandle);
  } else {
    // If no metadata region is available, use `SystemPageSize()`.
    offsets_to_metadata_[kConfigurablePoolHandle] = SystemPageSize();
  }
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
}

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
void PartitionAddressSpace::InitThreadIsolatedPool(
    ThreadIsolationOption thread_isolation) {
  // The ThreadIsolated pool can't be initialized with conflicting settings.
  if (IsThreadIsolatedPoolInitialized()) {
    PA_CHECK(setup_.thread_isolation_ == thread_isolation);
    return;
  }

  size_t pool_size = ThreadIsolatedPoolSize();
  setup_.thread_isolated_pool_base_address_ =
      AllocPages(pool_size, pool_size,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc);
  if (!setup_.thread_isolated_pool_base_address_) {
    HandlePoolAllocFailure();
  }

  PA_DCHECK(!(setup_.thread_isolated_pool_base_address_ & (pool_size - 1)));
  setup_.thread_isolation_ = thread_isolation;
  AddressPoolManager::GetInstance().Add(
      kThreadIsolatedPoolHandle, setup_.thread_isolated_pool_base_address_,
      pool_size);

  PA_DCHECK(
      !IsInThreadIsolatedPool(setup_.thread_isolated_pool_base_address_ - 1));
  PA_DCHECK(IsInThreadIsolatedPool(setup_.thread_isolated_pool_base_address_));
  PA_DCHECK(IsInThreadIsolatedPool(setup_.thread_isolated_pool_base_address_ +
                                   pool_size - 1));
  PA_DCHECK(!IsInThreadIsolatedPool(setup_.thread_isolated_pool_base_address_ +
                                    pool_size));

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  if (metadata_region_start_ != kUninitializedPoolBaseAddress) {
    offsets_to_metadata_[kThreadIsolatedPoolHandle] =
        metadata_region_start_ - setup_.thread_isolated_pool_base_address_ +
        MetadataInnerOffset(kThreadIsolatedPoolHandle);
  } else {
    // If no metadata region is available, use `SystemPageSize()`.
    offsets_to_metadata_[kThreadIsolatedPoolHandle] = SystemPageSize();
  }
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
}
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

void PartitionAddressSpace::UninitForTesting() {
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  UninitThreadIsolatedPoolForTesting();  // IN-TEST
#endif
  // The core pools (regular & BRP) were allocated using a single allocation of
  // double size.
  FreePages(setup_.regular_pool_base_address_, 2 * CorePoolSize());
  // Do not free pages for the configurable pool, because its memory is owned
  // by someone else, but deinitialize it nonetheless.
  setup_.regular_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.brp_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_mask_ = 0;
  AddressPoolManager::GetInstance().ResetForTesting();
#if PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
  CompressedPointerBaseGlobal::ResetBaseForTesting();
#endif  // PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  FreePages(metadata_region_start_, MetadataRegionSize());
  metadata_region_start_ = kUninitializedPoolBaseAddress;
#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  metadata_region_size_ = 0;
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  for (size_t i = 0; i < kMaxPoolHandle; ++i) {
    offsets_to_metadata_[i] = SystemPageSize();
  }
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
}

void PartitionAddressSpace::UninitConfigurablePoolForTesting() {
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // It's possible that the thread isolated pool has been initialized first, in
  // which case the setup_ memory has been made read-only. Remove the protection
  // temporarily.
  if (IsThreadIsolatedPoolInitialized()) {
    UnprotectThreadIsolatedGlobals();
  }
#endif
  AddressPoolManager::GetInstance().Remove(kConfigurablePoolHandle);
  setup_.configurable_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_mask_ = 0;
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // Put the metadata protection back in place.
  if (IsThreadIsolatedPoolInitialized()) {
    WriteProtectThreadIsolatedGlobals(setup_.thread_isolation_);
  }
#endif
#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  offsets_to_metadata_[kConfigurablePoolHandle] = SystemPageSize();
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
}

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
void PartitionAddressSpace::UninitThreadIsolatedPoolForTesting() {
  if (IsThreadIsolatedPoolInitialized()) {
    UnprotectThreadIsolatedGlobals();
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
    ThreadIsolationSettings::settings.enabled = false;
#endif

    FreePages(setup_.thread_isolated_pool_base_address_,
              ThreadIsolatedPoolSize());
    AddressPoolManager::GetInstance().Remove(kThreadIsolatedPoolHandle);
    setup_.thread_isolated_pool_base_address_ = kUninitializedPoolBaseAddress;
    setup_.thread_isolation_.enabled = false;
  }
#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  offsets_to_metadata_[kThreadIsolatedPoolHandle] = SystemPageSize();
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
}
#endif

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
// Allocate virtual address space of metadata and initialize metadata offsets
// of regular, brp and configurable pools.
void PartitionAddressSpace::InitMetadataRegionAndOffsets() {
  // Set up an address space only once.
  if (metadata_region_start_ != kUninitializedPoolBaseAddress) {
    return;
  }

#if PA_BUILDFLAG(ENABLE_MOVE_METADATA_OUT_OF_GIGACAGE_TRIAL)
  if (ExternalMetadataTrialGroup::kUndefined ==
      GetExternalMetadataTrialGroup()) {
    if (SelectExternalMetadataTrialGroup() !=
        ExternalMetadataTrialGroup::kEnabled) {
      for (size_t i = 0; i < kMaxPoolHandle; ++i) {
        offsets_to_metadata_[i] = SystemPageSize();
      }
      return;
    }
  }
#endif  // PA_BUILDFLAG(ENABLE_MOVE_METADATA_OUT_OF_GIGACAGE_TRIAL)

#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  metadata_region_size_ = std::max(kConfigurablePoolMaxSize, CorePoolSize());
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

  uintptr_t address =
      AllocPages(MetadataRegionSize(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc);
  if (!address) {
    HandlePoolAllocFailure();
  }

  metadata_region_start_ = address;

  PA_DCHECK(RegularPoolBase() != kUninitializedPoolBaseAddress);
  PA_DCHECK(BRPPoolBase() != kUninitializedPoolBaseAddress);

  offsets_to_metadata_[kRegularPoolHandle] =
      address - RegularPoolBase() + MetadataInnerOffset(kRegularPoolHandle);
  offsets_to_metadata_[kBRPPoolHandle] =
      address - BRPPoolBase() + MetadataInnerOffset(kBRPPoolHandle);

  // ConfigurablePool has not been initialized yet at this time.
  offsets_to_metadata_[kConfigurablePoolHandle] = SystemPageSize();
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  offsets_to_metadata_[kThreadIsolatedPoolHandle] = SystemPageSize();
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
}
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)

#if defined(PARTITION_ALLOCATOR_CONSTANTS_POSIX_NONCONST_PAGE_SIZE)

PageCharacteristics page_characteristics;

#endif

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace partition_alloc::internal
