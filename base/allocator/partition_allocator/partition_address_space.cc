// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_address_space.h"

#include <array>
#include <cstdint>
#include <ostream>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace partition_alloc::internal {

#if defined(PA_HAS_64_BITS_POINTERS)

namespace {

#if BUILDFLAG(IS_WIN)
NOINLINE void HandleGigaCageAllocFailureOutOfVASpace() {
  NO_CODE_FOLDING();
  PA_CHECK(false);
}

NOINLINE void HandleGigaCageAllocFailureOutOfCommitCharge() {
  NO_CODE_FOLDING();
  PA_CHECK(false);
}
#endif  // BUILDFLAG(IS_WIN)

NOINLINE void HandleGigaCageAllocFailure() {
  NO_CODE_FOLDING();
  uint32_t alloc_page_error_code = base::GetAllocPageErrorCode();
  PA_DEBUG_DATA_ON_STACK("error", static_cast<size_t>(alloc_page_error_code));
  // It's important to easily differentiate these two failures on Windows, so
  // crash with different stacks.
#if BUILDFLAG(IS_WIN)
  if (alloc_page_error_code == ERROR_NOT_ENOUGH_MEMORY) {
    // The error code says NOT_ENOUGH_MEMORY, but since we only do MEM_RESERVE,
    // it must be VA space exhaustion.
    HandleGigaCageAllocFailureOutOfVASpace();
  } else if (alloc_page_error_code == ERROR_COMMITMENT_LIMIT) {
    // On Windows <8.1, MEM_RESERVE increases commit charge to account for
    // not-yet-committed PTEs needed to cover that VA space, if it was to be
    // committed (see crbug.com/1101421#c16).
    HandleGigaCageAllocFailureOutOfCommitCharge();
  } else
#endif  // BUILDFLAG(IS_WIN)
  {
    PA_CHECK(false);
  }
}

}  // namespace

alignas(kPartitionCachelineSize)
    PartitionAddressSpace::GigaCageSetup PartitionAddressSpace::setup_;

#if defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
ALWAYS_INLINE size_t PartitionAddressSpace::RegularPoolSize() {
  return kRegularPoolSize;
}
ALWAYS_INLINE size_t PartitionAddressSpace::BRPPoolSize() {
  return kBRPPoolSize;
}
#endif  // defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)

void PartitionAddressSpace::Init() {
  if (IsInitialized())
    return;

  setup_.regular_pool_base_address_ = AllocPages(
      RegularPoolSize(), RegularPoolSize(),
      PageAccessibilityConfiguration::kInaccessible, PageTag::kPartitionAlloc);
  if (!setup_.regular_pool_base_address_)
    HandleGigaCageAllocFailure();
#if defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
  setup_.regular_pool_base_mask_ = ~(RegularPoolSize() - 1) & kMemTagUnmask;
#endif
  PA_DCHECK(!(setup_.regular_pool_base_address_ & (RegularPoolSize() - 1)));
  setup_.regular_pool_ = AddressPoolManager::GetInstance()->Add(
      setup_.regular_pool_base_address_, RegularPoolSize());
  PA_CHECK(setup_.regular_pool_ == kRegularPoolHandle);
  PA_DCHECK(!IsInRegularPool(setup_.regular_pool_base_address_ - 1));
  PA_DCHECK(IsInRegularPool(setup_.regular_pool_base_address_));
  PA_DCHECK(IsInRegularPool(setup_.regular_pool_base_address_ +
                            RegularPoolSize() - 1));
  PA_DCHECK(
      !IsInRegularPool(setup_.regular_pool_base_address_ + RegularPoolSize()));

  // Reserve an extra allocation granularity unit before the BRP pool, but keep
  // the pool aligned at BRPPoolSize(). A pointer immediately past an allocation
  // is a valid pointer, and having a "forbidden zone" before the BRP pool
  // prevents such a pointer from "sneaking into" the pool.
  const size_t kForbiddenZoneSize = PageAllocationGranularity();
  uintptr_t base_address = AllocPagesWithAlignOffset(
      0, BRPPoolSize() + kForbiddenZoneSize, BRPPoolSize(),
      BRPPoolSize() - kForbiddenZoneSize,
      PageAccessibilityConfiguration::kInaccessible, PageTag::kPartitionAlloc);
  if (!base_address)
    HandleGigaCageAllocFailure();
  setup_.brp_pool_base_address_ = base_address + kForbiddenZoneSize;
#if defined(PA_USE_DYNAMICALLY_SIZED_GIGA_CAGE)
  setup_.brp_pool_base_mask_ = ~(BRPPoolSize() - 1) & kMemTagUnmask;
#endif
  PA_DCHECK(!(setup_.brp_pool_base_address_ & (BRPPoolSize() - 1)));
  setup_.brp_pool_ = AddressPoolManager::GetInstance()->Add(
      setup_.brp_pool_base_address_, BRPPoolSize());
  PA_CHECK(setup_.brp_pool_ == kBRPPoolHandle);
  PA_DCHECK(!IsInBRPPool(setup_.brp_pool_base_address_ - 1));
  PA_DCHECK(IsInBRPPool(setup_.brp_pool_base_address_));
  PA_DCHECK(IsInBRPPool(setup_.brp_pool_base_address_ + BRPPoolSize() - 1));
  PA_DCHECK(!IsInBRPPool(setup_.brp_pool_base_address_ + BRPPoolSize()));

#if PA_STARSCAN_USE_CARD_TABLE
  // Reserve memory for PCScan quarantine card table.
  uintptr_t requested_address = setup_.regular_pool_base_address_;
  uintptr_t actual_address = AddressPoolManager::GetInstance()->Reserve(
      setup_.regular_pool_, requested_address, kSuperPageSize);
  PA_CHECK(requested_address == actual_address)
      << "QuarantineCardTable is required to be allocated at the beginning of "
         "the regular pool";
#endif  // PA_STARSCAN_USE_CARD_TABLE
}

void PartitionAddressSpace::InitConfigurablePool(uintptr_t pool_base,
                                                 size_t size) {
  // The ConfigurablePool must only be initialized once.
  PA_CHECK(!IsConfigurablePoolInitialized());

  // The other Pools must be initialized first.
  Init();

  PA_CHECK(pool_base);
  PA_CHECK(size <= kConfigurablePoolMaxSize);
  PA_CHECK(size >= kConfigurablePoolMinSize);
  PA_CHECK(base::bits::IsPowerOfTwo(size));
  PA_CHECK(pool_base % size == 0);

  setup_.configurable_pool_base_address_ = pool_base;
  setup_.configurable_pool_base_mask_ = ~(size - 1);

  setup_.configurable_pool_ = AddressPoolManager::GetInstance()->Add(
      setup_.configurable_pool_base_address_, size);
  PA_CHECK(setup_.configurable_pool_ == kConfigurablePoolHandle);
}

void PartitionAddressSpace::UninitForTesting() {
  FreePages(setup_.regular_pool_base_address_, RegularPoolSize());
  // For BRP pool, the allocation region includes a "forbidden zone" before the
  // pool.
  const size_t kForbiddenZoneSize = PageAllocationGranularity();
  FreePages(setup_.brp_pool_base_address_ - kForbiddenZoneSize,
            BRPPoolSize() + kForbiddenZoneSize);
  // Do not free pages for the configurable pool, because its memory is owned
  // by someone else, but deinitialize it nonetheless.
  setup_.regular_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.brp_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_mask_ = 0;
  setup_.regular_pool_ = 0;
  setup_.brp_pool_ = 0;
  setup_.configurable_pool_ = 0;
  AddressPoolManager::GetInstance()->ResetForTesting();
}

void PartitionAddressSpace::UninitConfigurablePoolForTesting() {
  AddressPoolManager::GetInstance()->Remove(setup_.configurable_pool_);
  setup_.configurable_pool_base_address_ = kUninitializedPoolBaseAddress;
  setup_.configurable_pool_base_mask_ = 0;
  setup_.configurable_pool_ = 0;
}

#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace partition_alloc::internal
