// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_root.h"

#include <cstring>

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/no_destructor.h"

namespace partition_alloc::internal {

namespace {
constexpr PartitionOptions kConfig{};
}  // namespace

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
PartitionRoot& PCScanMetadataAllocator() {
  static internal::base::NoDestructor<PartitionRoot> allocator(kConfig);
  return *allocator;
}

// TODO(tasak): investigate whether PartitionAlloc tests really need this
// function or not. If we found no tests need, remove it.
void ReinitPCScanMetadataAllocatorForTesting() {
  // First, purge memory owned by PCScanMetadataAllocator.
  PCScanMetadataAllocator().PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans |
                                        PurgeFlags::kDiscardUnusedSystemPages);
  // Then, reinit the allocator.
  PCScanMetadataAllocator().ResetForTesting(true);  // IN-TEST
  PCScanMetadataAllocator().Init(kConfig);
}

}  // namespace partition_alloc::internal
