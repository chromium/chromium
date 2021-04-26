// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_root.h"

#include <cstring>

#include "base/no_destructor.h"

namespace base {
namespace internal {

namespace {
constexpr PartitionOptions kConfig{PartitionOptions::AlignedAlloc::kDisallowed,
                                   PartitionOptions::ThreadCache::kDisabled,
                                   PartitionOptions::Quarantine::kDisallowed,
                                   PartitionOptions::Cookies::kAllowed,
                                   PartitionOptions::RefCount::kDisallowed};
}

ThreadSafePartitionRoot& PCScanMetadataAllocator() {
  static base::NoDestructor<ThreadSafePartitionRoot> allocator(kConfig);
  return *allocator;
}

void ReinitPCScanMetadataAllocatorForTesting() {
  // First, purge memory owned by PCScanMetadataAllocator.
  PCScanMetadataAllocator().PurgeMemory(PartitionPurgeDecommitEmptySlotSpans |
                                        PartitionPurgeDiscardUnusedSystemPages);
  // Then, reinit the allocator.
  memset(&PCScanMetadataAllocator(), 0, sizeof(PCScanMetadataAllocator()));
  PCScanMetadataAllocator().Init(kConfig);
}

}  // namespace internal
}  // namespace base
