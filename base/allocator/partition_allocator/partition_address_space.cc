// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_address_space.h"

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

uintptr_t PartitionAddressSpace::reserved_base_address_ = 0;
// Before PartitionAddressSpace::Init(), no allocation are allocated from a
// reserved address space. Therefore, set *_pool_base_address_ initially to
// k*PoolOffsetMask, so that PartitionAddressSpace::IsIn*Pool() always returns
// false.
uintptr_t PartitionAddressSpace::non_brp_pool_base_address_ =
    kNonBRPPoolOffsetMask;
uintptr_t PartitionAddressSpace::brp_pool_base_address_ = kBRPPoolOffsetMask;

pool_handle PartitionAddressSpace::non_brp_pool_ = 0;
pool_handle PartitionAddressSpace::brp_pool_ = 0;

void PartitionAddressSpace::Init() {
  if (IsInitialized())
    return;

  reserved_base_address_ = reinterpret_cast<uintptr_t>(AllocPages(
      nullptr, kDesiredAddressSpaceSize, kReservedAddressSpaceAlignment,
      base::PageInaccessible, PageTag::kPartitionAlloc));
  PA_CHECK(reserved_base_address_);

  uintptr_t current = reserved_base_address_;

  non_brp_pool_base_address_ = current;
  non_brp_pool_ = internal::AddressPoolManager::GetInstance()->Add(
      current, kNonBRPPoolSize);
  PA_DCHECK(non_brp_pool_);
  PA_DCHECK(!IsInNonBRPPool(reinterpret_cast<void*>(current - 1)));
  PA_DCHECK(IsInNonBRPPool(reinterpret_cast<void*>(current)));
  current += kNonBRPPoolSize;
  PA_DCHECK(IsInNonBRPPool(reinterpret_cast<void*>(current - 1)));
  PA_DCHECK(!IsInNonBRPPool(reinterpret_cast<void*>(current)));

  brp_pool_base_address_ = current;
  brp_pool_ =
      internal::AddressPoolManager::GetInstance()->Add(current, kBRPPoolSize);
  PA_DCHECK(brp_pool_);
  PA_DCHECK(!IsInBRPPool(reinterpret_cast<void*>(current - 1)));
  PA_DCHECK(IsInBRPPool(reinterpret_cast<void*>(current)));
  current += kBRPPoolSize;
  PA_DCHECK(IsInBRPPool(reinterpret_cast<void*>(current - 1)));
  PA_DCHECK(!IsInBRPPool(reinterpret_cast<void*>(current)));

#if PA_ALLOW_PCSCAN
  // Reserve memory for PCScan quarantine card table.
  void* requested_address = reinterpret_cast<void*>(brp_pool_base_address_);
  char* actual_address = internal::AddressPoolManager::GetInstance()->Reserve(
      brp_pool_, requested_address, kSuperPageSize);
  PA_CHECK(requested_address == actual_address)
      << "QuarantineCardTable is required to be allocated in the beginning of "
         "the BRPPool";
  SetSystemPagesAccess(actual_address, kSuperPageSize, PageInaccessible);
#endif

  PA_DCHECK(reserved_base_address_ + kDesiredAddressSpaceSize == current);
}

void PartitionAddressSpace::UninitForTesting() {
  FreePages(reinterpret_cast<void*>(reserved_base_address_),
            kReservedAddressSpaceAlignment);
  reserved_base_address_ = 0;
  non_brp_pool_base_address_ = kNonBRPPoolOffsetMask;
  brp_pool_base_address_ = kBRPPoolOffsetMask;
  non_brp_pool_ = 0;
  brp_pool_ = 0;
  internal::AddressPoolManager::GetInstance()->ResetForTesting();
}

#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal

}  // namespace base
