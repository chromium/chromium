// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_address_space.h"

#include <array>
#include <ostream>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/bits.h"

namespace base {

namespace internal {

#if defined(PA_HAS_64_BITS_POINTERS)

alignas(kPartitionCachelineSize)
    PartitionAddressSpace::GigaCageSetup PartitionAddressSpace::setup_;

void PartitionAddressSpace::Init() {
  if (IsInitialized())
    return;

  setup_.non_brp_pool_base_address_ = reinterpret_cast<uintptr_t>(
      AllocPages(nullptr, kNonBRPPoolSize, kNonBRPPoolSize,
                 base::PageInaccessible, PageTag::kPartitionAlloc));
  PA_CHECK(setup_.non_brp_pool_base_address_);

  PA_DCHECK(!(setup_.non_brp_pool_base_address_ & (kNonBRPPoolSize - 1)));
  setup_.non_brp_pool_ = internal::AddressPoolManager::GetInstance()->Add(
      setup_.non_brp_pool_base_address_, kNonBRPPoolSize);
  PA_CHECK(setup_.non_brp_pool_ == kNonBRPPoolHandle);
  PA_DCHECK(!IsInNonBRPPool(
      reinterpret_cast<void*>(setup_.non_brp_pool_base_address_ - 1)));
  PA_DCHECK(IsInNonBRPPool(
      reinterpret_cast<void*>(setup_.non_brp_pool_base_address_)));
  PA_DCHECK(IsInNonBRPPool(reinterpret_cast<void*>(
      setup_.non_brp_pool_base_address_ + kNonBRPPoolSize - 1)));
  PA_DCHECK(!IsInNonBRPPool(reinterpret_cast<void*>(
      setup_.non_brp_pool_base_address_ + kNonBRPPoolSize)));

  // Reserve an extra allocation granularity unit before the BRP pool, but keep
  // the pool aligned at kBRPPoolSize. A pointer immediately past an allocation
  // is a valid pointer, and having a "forbidden zone" before the BRP pool
  // prevents such a pointer from "sneaking into" the pool.
  const size_t kForbiddenZoneSize = PageAllocationGranularity();
  setup_.brp_pool_base_address_ =
      reinterpret_cast<uintptr_t>(AllocPagesWithAlignOffset(
          nullptr, kBRPPoolSize + kForbiddenZoneSize, kBRPPoolSize,
          kBRPPoolSize - kForbiddenZoneSize, base::PageInaccessible,
          PageTag::kPartitionAlloc)) +
      kForbiddenZoneSize;
  PA_CHECK(setup_.brp_pool_base_address_);
  PA_DCHECK(!(setup_.brp_pool_base_address_ & (kBRPPoolSize - 1)));
  setup_.brp_pool_ = internal::AddressPoolManager::GetInstance()->Add(
      setup_.brp_pool_base_address_, kBRPPoolSize);
  PA_CHECK(setup_.brp_pool_ == kBRPPoolHandle);
  PA_DCHECK(
      !IsInBRPPool(reinterpret_cast<void*>(setup_.brp_pool_base_address_ - 1)));
  PA_DCHECK(
      IsInBRPPool(reinterpret_cast<void*>(setup_.brp_pool_base_address_)));
  PA_DCHECK(IsInBRPPool(reinterpret_cast<void*>(setup_.brp_pool_base_address_ +
                                                kBRPPoolSize - 1)));
  PA_DCHECK(!IsInBRPPool(
      reinterpret_cast<void*>(setup_.brp_pool_base_address_ + kBRPPoolSize)));

#if PA_STARSCAN_USE_CARD_TABLE
  // Reserve memory for PCScan quarantine card table.
  void* requested_address =
      reinterpret_cast<void*>(setup_.non_brp_pool_base_address_);
  char* actual_address = internal::AddressPoolManager::GetInstance()->Reserve(
      non_brp_pool_, requested_address, kSuperPageSize);
  PA_CHECK(requested_address == actual_address)
      << "QuarantineCardTable is required to be allocated in the beginning of "
         "the non-BRP pool";
#endif  // PA_STARSCAN_USE_CARD_TABLE
}

void PartitionAddressSpace::InitConfigurablePool(void* address, size_t size) {
  // The ConfigurablePool must only be initialized once.
  PA_CHECK(!IsConfigurablePoolInitialized());

  // The other Pools must be initialized first.
  Init();

  PA_CHECK(address);
  PA_CHECK(size == kConfigurablePoolSize);
  PA_CHECK(bits::IsPowerOfTwo(size));
  PA_CHECK(reinterpret_cast<uintptr_t>(address) % size == 0);

  setup_.configurable_pool_base_address_ = reinterpret_cast<uintptr_t>(address);

  setup_.configurable_pool_ = internal::AddressPoolManager::GetInstance()->Add(
      setup_.configurable_pool_base_address_, size);
  PA_CHECK(setup_.configurable_pool_ == kConfigurablePoolHandle);
}

void PartitionAddressSpace::UninitForTesting() {
  FreePages(reinterpret_cast<void*>(setup_.non_brp_pool_base_address_),
            kNonBRPPoolSize);
  // For BRP pool, the allocation region includes a "forbidden zone" before the
  // pool.
  const size_t kForbiddenZoneSize = PageAllocationGranularity();
  FreePages(reinterpret_cast<void*>(setup_.brp_pool_base_address_ -
                                    kForbiddenZoneSize),
            kBRPPoolSize + kForbiddenZoneSize);
  // Do not free pages for the configurable pool, because its memory is owned
  // by someone else, but deinitialize it nonetheless.
  setup_.non_brp_pool_base_address_ = kNonBRPPoolOffsetMask;
  setup_.brp_pool_base_address_ = kBRPPoolOffsetMask;
  setup_.configurable_pool_base_address_ = kConfigurablePoolOffsetMask;
  setup_.non_brp_pool_ = 0;
  setup_.brp_pool_ = 0;
  setup_.configurable_pool_ = 0;
  internal::AddressPoolManager::GetInstance()->ResetForTesting();
}

#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal

}  // namespace base
