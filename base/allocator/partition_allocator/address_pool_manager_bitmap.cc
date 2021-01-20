// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/lazy_instance.h"

#if !defined(PA_HAS_64_BITS_POINTERS)
namespace base {
namespace internal {

namespace {

LazyInstance<Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;

}  // namespace

Lock& AddressPoolManagerBitmap::GetLock() {
  return g_lock.Get();
}

std::bitset<AddressPoolManagerBitmap::kDirectMapBits>
    AddressPoolManagerBitmap::directmap_bits_;  // GUARDED_BY(GetLock())
std::bitset<AddressPoolManagerBitmap::kNormalBucketBits>
    AddressPoolManagerBitmap::normal_bucket_bits_;  // GUARDED_BY(GetLock())

}  // namespace internal
}  // namespace base

#endif  // !defined(PA_HAS_64_BITS_POINTERS)
