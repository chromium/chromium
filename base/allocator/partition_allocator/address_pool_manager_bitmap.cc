// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"

#if !defined(PA_HAS_64_BITS_POINTERS)

namespace base {
namespace internal {

namespace {

PartitionLock g_lock;

}  // namespace

PartitionLock& AddressPoolManagerBitmap::GetLock() {
  return g_lock;
}

std::bitset<AddressPoolManagerBitmap::kRegularPoolBits>
    AddressPoolManagerBitmap::regular_pool_bits_;  // GUARDED_BY(GetLock())
std::bitset<AddressPoolManagerBitmap::kBRPPoolBits>
    AddressPoolManagerBitmap::brp_pool_bits_;  // GUARDED_BY(GetLock())
#if BUILDFLAG(USE_BACKUP_REF_PTR)
#if BUILDFLAG(NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST)
std::array<std::atomic_bool,
           AddressPoolManagerBitmap::kAddressSpaceSize / kSuperPageSize>
    AddressPoolManagerBitmap::brp_forbidden_super_page_map_;
#else
std::array<std::atomic_uint32_t,
           AddressPoolManagerBitmap::kAddressSpaceSize / kSuperPageSize>
    AddressPoolManagerBitmap::super_page_refcount_map_;
#endif
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
}  // namespace internal
}  // namespace base

#endif  // !defined(PA_HAS_64_BITS_POINTERS)
