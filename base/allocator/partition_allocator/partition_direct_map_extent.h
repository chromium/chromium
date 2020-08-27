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

template <bool thread_safe>
ALWAYS_INLINE PartitionDirectMapExtent<thread_safe>*
PartitionDirectMapExtent<thread_safe>::FromPage(
    PartitionPage<thread_safe>* page) {
  PA_DCHECK(page->bucket->is_direct_mapped());
  return reinterpret_cast<PartitionDirectMapExtent<thread_safe>*>(
      reinterpret_cast<char*>(page) + 3 * kPageMetadataSize);
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_H_
