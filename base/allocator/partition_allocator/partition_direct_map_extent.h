// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_H_

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/check.h"

namespace base {
namespace internal {

template <bool thread_safe>
struct PartitionDirectMapExtent {
  PartitionDirectMapExtent<thread_safe>* next_extent;
  PartitionDirectMapExtent<thread_safe>* prev_extent;
  PartitionBucket<thread_safe>* bucket;
  size_t map_size;  // Mapped size, not including guard pages and meta-data.

  ALWAYS_INLINE static PartitionDirectMapExtent<thread_safe>* FromPage(
      PartitionPage<thread_safe>* page);
};

// Metadata page for direct-mapped allocations.
template <bool thread_safe>
struct PartitionDirectMapMetadata {
  union {
    PartitionSuperPageExtentEntry<thread_safe> extent;
    // Never used, but must have the same size as a real PartitionPage.
    PartitionPage<thread_safe> first_invalid_page;
  };
  PartitionPage<thread_safe> page;
  PartitionBucket<thread_safe> bucket;
  PartitionDirectMapExtent<thread_safe> direct_map_extent;
};

template <bool thread_safe>
ALWAYS_INLINE PartitionDirectMapExtent<thread_safe>*
PartitionDirectMapExtent<thread_safe>::FromPage(
    PartitionPage<thread_safe>* page) {
  PA_DCHECK(page->bucket->is_direct_mapped());
  // The page passed here is always |page| in |PartitionDirectMapMetadata|
  // above. To get the metadata structure, need to get the invalid page address.
  auto* first_invalid_page = page - 1;
  auto* metadata = reinterpret_cast<PartitionDirectMapMetadata<thread_safe>*>(
      first_invalid_page);
  return &metadata->direct_map_extent;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_H_
