// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_address_space.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>

#include "build/build_config.h"
#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/compressed_pointer.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/debug/alias.h"
#include "partition_alloc/partition_alloc_buildflags.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/thread_isolation/thread_isolation.h"

#if BUILDFLAG(IS_IOS)
#include <mach-o/dyld.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // BUILDFLAG(IS_WIN)

#if PA_CONFIG(ENABLE_SHADOW_METADATA) || BUILDFLAG(ENABLE_THREAD_ISOLATION)
#include <sys/mman.h>
#endif

namespace partition_alloc::internal {

#if BUILDFLAG(HAS_64_BIT_POINTERS)

namespace {

#if BUILDFLAG(IS_WIN)

PA_NOINLINE void HandlePoolAllocFailureOutOfVASpace() {
  PA_NO_CODE_FOLDING();
  PA_CHECK(false);
}

PA_NOINLINE void HandlePoolAllocFailureOutOfCommitCharge() {
  PA_NO_CODE_FOLDING();
  PA_CHECK(false);
}
#endif  // BUILDFLAG(IS_WIN)

PA_NOINLINE void HandlePoolAllocFailure() {
  PA_NO_CODE_FOLDING();
  uint32_t alloc_page_error_code = GetAllocPageErrorCode();
  PA_DEBUG_DATA_ON_STACK("error", static_cast<size_t>(alloc_page_error_code));
  // It's important to easily differentiate these two failures on Windows, so
  // crash with different stacks.
#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)
  {
    PA_CHECK(false);
  }
}

}  // namespace

PartitionAddressSpace::PoolSetup PartitionAddressSpace::setup_;

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
std::ptrdiff_t PartitionAddressSpace::regular_pool_shadow_offset_ = 0;
std::ptrdiff_t PartitionAddressSpace::brp_pool_shadow_offset_ = 0;
#endif

#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
#if !BUILDFLAG(IS_IOS)
#error Dynamic pool size is only supported on iOS.
#endif

namespace {
bool IsIOSTestProcess() {
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
}  // namespace

PA_ALWAYS_INLINE size_t PartitionAddressSpace::RegularPoolSize() {
  return IsIOSTestProcess() ? kRegularPoolSizeForIOSTestProcess
                            : kRegularPoolSize;
}
PA_ALWAYS_INLINE size_t PartitionAddressSpace::BRPPoolSize() {
  return IsIOSTestProcess() ? kBRPPoolSizeForIOSTestProcess : kBRPPoolSize;
}
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

void PartitionAddressSpace::Init() {
  if (IsInitialized()) {
    return;
  }

  size_t regular_pool_size = RegularPoolSize();
  size_t brp_pool_size = BRPPoolSize();

#if BUILDFLAG(GLUE_CORE_POOLS)
  // Gluing core pools (regular & BRP) makes sense only when both pools are of
  // the same size. This the only way we can check belonging to either of the
  // two with a single bitmask operation.
  PA_CHECK(regular_pool_size == brp_pool_size);

  // TODO(crbug.com/1362969): Support PA_ENABLE_SHADOW_METADATA.
  int pools_fd = -1;

  size_t glued_pool_sizes = regular_pool_size * 2;
  // Note, BRP pool requires to be preceded by a "forbidden zone", which is
  // conveniently taken care of by the last guard page of the regular pool.
  setup_.regular_pool_base_address_ =
      AllocPages(glued_pool_sizes, glued_pool_sizes,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc, pools_fd);
  if (!setup_.regular_pool_base_address_) {
    HandlePoolAllocFailure();
  }
  setup_.brp_pool_base_address_ =
      setup_.regular_pool_base_address_ + regular_pool_size;
#else  // BUILDFLAG(GLUE_CORE_POOLS)
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  int regular_pool_fd = memfd_create("/regular_pool", MFD_CLOEXEC);
#else
  int regular_pool_fd = -1;
#endif
  setup_.regular_pool_base_address_ =
      AllocPages(regular_pool_size, regular_pool_size,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc, regular_pool_fd);
  if (!setup_.regular_pool_base_address_) {
    HandlePoolAllocFailure();
  }

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  int brp_pool_fd = memfd_create("/brp_pool", MFD_CLOEXEC);
#else
  int brp_pool_fd = -1;
#endif
  // Reserve an extra allocation granularity unit before the BRP pool, but keep
  // the pool aligned at BRPPoolSize(). A pointer immediately past an allocation
  // is a valid pointer, and having a "forbidden zone" before the BRP pool
  // prevents such a pointer from "sneaking into" the pool.
  const size_t kForbiddenZoneSize = PageAllocationGranularity();
  uintptr_t base_address = AllocPagesWithAlignOffset(
      0, brp_pool_size + kForbiddenZoneSize, brp_pool_size,
      brp_pool_size - kForbiddenZoneSize,
      PageAccessibilityConfiguration(
          PageAccessibilityConfiguration::kInaccessible),
      PageTag::kPartitionAlloc, brp_pool_fd);
  if (!base_address) {
    HandlePoolAllocFailure();
  }
  setup_.brp_pool_base_address_ = base_address + kForbiddenZoneSize;
#endif  // BUILDFLAG(GLUE_CORE_POOLS)

#if PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)
  setup_.regular_pool_base_mask_ = ~(regular_pool_size - 1);
  setup_.brp_pool_base_mask_ = ~(brp_pool_size - 1);
#if BUILDFLAG(GLUE_CORE_POOLS)
  // When PA_GLUE_CORE_POOLS is on, the BRP pool is placed at the end of the
  // regular pool, effectively forming one virtual pool of a twice bigger
  // size. Adjust the mask appropriately.
  setup_.core_pools_base_mask_ = setup_.regular_pool_base_mask_ << 1;
  PA_DCHECK(setup_.core_pools_base_mask_ == (setup_.brp_pool_base_mask_ << 1));
#endif
#endif  // PA_CONFIG(DYNAMICALLY_SELECT_POOL_SIZE)

  AddressPoolManager::GetInstance().Add(
      kRegularPoolHandle, setup_.regular_pool_base_address_, regular_pool_size);
  AddressPoolManager::GetInstance().Add(
      kBRPPoolHandle, setup_.brp_pool_base_address_, brp_pool_size);

  // Sanity check pool alignment.
  PA_DCHECK(!(setup_.regular_pool_base_address_ & (regular_pool_size - 1)));
  PA_DCHECK(!(setup_.brp_pool_base_address_ & (brp_pool_size - 1)));
#if BUILDFLAG(GLUE_CORE_POOLS)
  PA_DCHECK(!(setup_.regular_pool_base_address_ & (glued_pool_sizes - 1)));
#endif

  // Sanity check pool belonging.
  PA_DCHECK(!IsInRegularPool(setup_.regular_pool_base_address_ - 1));
  PA_DCHECK(IsInRegularPool(setup_.regular_pool_base_address_));
  PA_DCHECK(IsInRegularPool(setup_.regular_pool_base_address_ +
                            regular_pool_size - 1));
  PA_DCHECK(
      !IsInRegularPool(setup_.regular_pool_base_address_ + regular_pool_size));
  PA_DCHECK(!IsInBRPPool(setup_.brp_pool_base_address_ - 1));
  PA_DCHECK(IsInBRPPool(setup_.brp_pool_base_address_));
  PA_DCHECK(IsInBRPPool(setup_.brp_pool_base_address_ + brp_pool_size - 1));
  PA_DCHECK(!IsInBRPPool(setup_.brp_pool_base_address_ + brp_pool_size));
#if BUILDFLAG(GLUE_CORE_POOLS)
  PA_DCHECK(!IsInCorePools(setup_.regular_pool_base_address_ - 1));
  PA_DCHECK(IsInCorePools(setup_.regular_pool_base_address_));
  PA_DCHECK(
      IsInCorePools(setup_.regular_pool_base_address_ + regular_pool_size - 1));
  PA_DCHECK(
      IsInCorePools(setup_.regular_pool_base_address_ + regular_pool_size));
  PA_DCHECK(IsInCorePools(setup_.brp_pool_base_address_ - 1));
  PA_DCHECK(IsInCorePools(setup_.brp_pool_base_address_));
  PA_DCHECK(IsInCorePools(setup_.brp_pool_base_address_ + brp_pool_size - 1));
  PA_DCHECK(!IsInCorePools(setup_.brp_pool_base_address_ + brp_pool_size));
#endif  // BUILDFLAG(GLUE_CORE_POOLS)

#if PA_CONFIG(STARSCAN_USE_CARD_TABLE)
  // Reserve memory for PCScan quarantine card table.
  uintptr_t requested_address = setup_.regular_pool_base_address_;
  uintptr_t actual_address = AddressPoolManager::GetInstance().Reserve(
      kRegularPoolHandle, requested_address, kSuperPageSize);
  PA_CHECK(requested_address == actual_address)
      << "QuarantineCardTable is required to be allocated at the beginning of "
         "the regular pool";
#endif  // PA_CONFIG(STARSCAN_USE_CARD_TABLE)

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  // Reserve memory for the shadow pools.
  uintptr_t regular_pool_shadow_address =
      AllocPages(regular_pool_size, regular_pool_size,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc, regular_pool_fd);
  regular_pool_shadow_offset_ =
      regular_pool_shadow_address - setup_.regular_pool_base_address_;

  uintptr_t brp_pool_shadow_address = AllocPagesWithAlignOffset(
      0, brp_pool_size + kForbiddenZoneSize, brp_pool_size,
      brp_pool_size - kForbiddenZoneSize,
      PageAccessibilityConfiguration(
          PageAccessibilityConfiguration::kInaccessible),
      PageTag::kPartitionAlloc, brp_pool_fd);
  brp_pool_shadow_offset_ =
      brp_pool_shadow_address - setup_.brp_pool_base_address_;
#endif

#if BUILDFLAG(ENABLE_POINTER_COMPRESSION)
  CompressedPointerBaseGlobal::SetBase(setup_.regular_pool_base_address_);
#endif  // BUILDFLAG(ENABLE_POINTER_COMPRESSION)
}

void PartitionAddressSpace::InitConfigurablePool(uintptr_t pool_base,
                                                 size_t size) {
  // The ConfigurablePool must only be initialized once.
  PA_CHECK(!IsConfigurablePoolInitialized());

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
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
  PA_CHECK(std::has_single_bit(size));
  PA_CHECK(pool_base % size == 0);

  setup_.configurable_pool_base_address_ = pool_base;
  setup_.configurable_pool_base_mask_ = ~(size - 1);

  AddressPoolManager::GetInstance().Add(
      kConfigurablePoolHandle, setup_.configurable_pool_base_address_, size);

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // Put the metadata protection back in place.
  if (IsThreadIsolatedPoolInitialized()) {
    WriteProtectThreadIsolatedGlobals(setup_.thread_isolation_);
  }
#endif
}

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
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

  // TODO(1362969): support PA_ENABLE_SHADOW_METADATA
}
#endif  // BUILDFLAG(ENABLE_THREAD_ISOLATION)

void PartitionAddressSpace::UninitForTesting() {
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  UninitThreadIsolatedPoolForTesting();  // IN-TEST
#endif
#if BUILDFLAG(GLUE_CORE_POOLS)
  // The core pools (regular & BRP) were allocated using a single allocation of
  // double size.
  FreePages(setup_.regular_pool_base_address_, 2 * RegularPoolSize());
#else   // BUILDFLAG(GLUE_CORE_POOLS)
  FreePages(setup_.regular_pool_base_address_, RegularPoolSize());
  // For BRP pool, the allocation region includes a "forbidden zone" before the
  // pool.
  const size_t kForbiddenZoneSize = PageAllocationGranularity();
  FreePages(setup_.brp_pool_base_address_ - kForbiddenZoneSize,
            BRPPoolSize() + kForbiddenZoneSize);
#endif  // BUILDFLAG(GLUE_CORE_POOLS)
  // Do not free pages for the configurable pool, because its memory is owned
  // by someone else, but deinitialize it nonetheless.
  setup_.regular_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.brp_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_mask_ = 0;
  AddressPoolManager::GetInstance().ResetForTesting();
#if BUILDFLAG(ENABLE_POINTER_COMPRESSION)
  CompressedPointerBaseGlobal::ResetBaseForTesting();
#endif  // BUILDFLAG(ENABLE_POINTER_COMPRESSION)
}

void PartitionAddressSpace::UninitConfigurablePoolForTesting() {
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
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
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // Put the metadata protection back in place.
  if (IsThreadIsolatedPoolInitialized()) {
    WriteProtectThreadIsolatedGlobals(setup_.thread_isolation_);
  }
#endif
}

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
void PartitionAddressSpace::UninitThreadIsolatedPoolForTesting() {
  if (IsThreadIsolatedPoolInitialized()) {
    UnprotectThreadIsolatedGlobals();
#if BUILDFLAG(PA_DCHECK_IS_ON)
    ThreadIsolationSettings::settings.enabled = false;
#endif

    FreePages(setup_.thread_isolated_pool_base_address_,
              ThreadIsolatedPoolSize());
    AddressPoolManager::GetInstance().Remove(kThreadIsolatedPoolHandle);
    setup_.thread_isolated_pool_base_address_ = kUninitializedPoolBaseAddress;
    setup_.thread_isolation_.enabled = false;
  }
}
#endif

#if defined(PARTITION_ALLOCATOR_CONSTANTS_POSIX_NONCONST_PAGE_SIZE)

PageCharacteristics page_characteristics;

#endif

#endif  // BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace partition_alloc::internal
