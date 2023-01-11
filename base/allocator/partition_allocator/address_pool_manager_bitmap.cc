// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"

#if !PA_CONFIG(HAS_64_BITS_POINTERS)

namespace partition_alloc::internal {

namespace {

Lock g_lock;

}  // namespace

Lock& AddressPoolManagerBitmap::GetLock() {
  return g_lock;
}

std::bitset<AddressPoolManagerBitmap::kRegularPoolBits>
    AddressPoolManagerBitmap::regular_pool_bits_;  // GUARDED_BY(GetLock())
std::bitset<AddressPoolManagerBitmap::kBRPPoolBits>
    AddressPoolManagerBitmap::brp_pool_bits_;  // GUARDED_BY(GetLock())
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
std::array<std::atomic_bool,
           AddressPoolManagerBitmap::kAddressSpaceSize / kSuperPageSize>
    AddressPoolManagerBitmap::brp_forbidden_super_page_map_;
std::atomic_size_t AddressPoolManagerBitmap::blocklist_hit_count_;
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

}  // namespace partition_alloc::internal

#endif  // !PA_CONFIG(HAS_64_BITS_POINTERS)
