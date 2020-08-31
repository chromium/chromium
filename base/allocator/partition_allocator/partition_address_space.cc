// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_address_space.h"

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/bits.h"

namespace base {

namespace internal {

#if defined(PA_HAS_64_BITS_POINTERS)

uintptr_t PartitionAddressSpace::reserved_base_address_ = 0;
// Before PartitionAddressSpace::Init(), no allocation are allocated from a
// reserved address space. Therefore, set *_pool_base_address_ initially to
// k*PoolOffsetMask, so that PartitionAddressSpace::IsIn*Pool() always returns
// false.
uintptr_t PartitionAddressSpace::direct_map_pool_base_address_ =
    kDirectMapPoolOffsetMask;
uintptr_t PartitionAddressSpace::normal_bucket_pool_base_address_ =
    kNormalBucketPoolOffsetMask;

pool_handle PartitionAddressSpace::direct_map_pool_ = 0;
pool_handle PartitionAddressSpace::normal_bucket_pool_ = 0;

void PartitionAddressSpace::Init() {
  if (IsInitialized())
    return;

  reserved_base_address_ = reinterpret_cast<uintptr_t>(AllocPages(
      nullptr, kDesiredAddressSpaceSize, kReservedAddressSpaceAlignment,
      base::PageInaccessible, PageTag::kPartitionAlloc, false));
  PA_CHECK(reserved_base_address_);

  uintptr_t current = reserved_base_address_;

  direct_map_pool_base_address_ = current;
  direct_map_pool_ = internal::AddressPoolManager::GetInstance()->Add(
      current, kDirectMapPoolSize);
  PA_DCHECK(direct_map_pool_);
  PA_DCHECK(!IsInDirectMapPool(reinterpret_cast<void*>(current - 1)));
  PA_DCHECK(IsInDirectMapPool(reinterpret_cast<void*>(current)));
  current += kDirectMapPoolSize;
  PA_DCHECK(IsInDirectMapPool(reinterpret_cast<void*>(current - 1)));
  PA_DCHECK(!IsInDirectMapPool(reinterpret_cast<void*>(current)));

  normal_bucket_pool_base_address_ = current;
  normal_bucket_pool_ = internal::AddressPoolManager::GetInstance()->Add(
      current, kNormalBucketPoolSize);
  PA_DCHECK(normal_bucket_pool_);
  PA_DCHECK(!IsInNormalBucketPool(reinterpret_cast<void*>(current - 1)));
  PA_DCHECK(IsInNormalBucketPool(reinterpret_cast<void*>(current)));
  current += kNormalBucketPoolSize;
  PA_DCHECK(IsInNormalBucketPool(reinterpret_cast<void*>(current - 1)));
  PA_DCHECK(!IsInNormalBucketPool(reinterpret_cast<void*>(current)));

  PA_DCHECK(reserved_base_address_ + kDesiredAddressSpaceSize == current);
}

void PartitionAddressSpace::UninitForTesting() {
  FreePages(reinterpret_cast<void*>(reserved_base_address_),
            kReservedAddressSpaceAlignment);
  reserved_base_address_ = 0;
  direct_map_pool_base_address_ = kDirectMapPoolOffsetMask;
  normal_bucket_pool_base_address_ = kNormalBucketPoolOffsetMask;
  direct_map_pool_ = 0;
  normal_bucket_pool_ = 0;
  internal::AddressPoolManager::GetInstance()->ResetForTesting();
}

#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal

}  // namespace base
