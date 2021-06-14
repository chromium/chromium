// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_H_

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_page.h"

namespace base {
namespace internal {

template <bool thread_safe>
struct PartitionDirectMapExtent {
  PartitionDirectMapExtent<thread_safe>* next_extent;
  PartitionDirectMapExtent<thread_safe>* prev_extent;
  PartitionBucket<thread_safe>* bucket;
  // Size of the entire reservation, including guard pages, meta-data,
  // padding for alignment before allocation, and padding for granularity at the
  // end of the allocation.
  size_t reservation_size;
  // Padding between the first partition page (guard pages + meta-data) and
  // the allocation.
  size_t padding_for_alignment;

  ALWAYS_INLINE static PartitionDirectMapExtent<thread_safe>* FromSlotSpan(
      SlotSpanMetadata<thread_safe>* slot_span);
};

// Metadata page for direct-mapped allocations.
template <bool thread_safe>
struct PartitionDirectMapMetadata {
  PartitionPage<thread_safe> page;
  PartitionPage<thread_safe> subsequent_page;
  PartitionBucket<thread_safe> bucket;
  PartitionDirectMapExtent<thread_safe> direct_map_extent;
};

template <bool thread_safe>
ALWAYS_INLINE PartitionDirectMapExtent<thread_safe>*
PartitionDirectMapExtent<thread_safe>::FromSlotSpan(
    SlotSpanMetadata<thread_safe>* slot_span) {
  PA_DCHECK(slot_span->bucket->is_direct_mapped());
  // |*slot_span| is the first field of |PartitionDirectMapMetadata|, just cast.
  auto* metadata =
      reinterpret_cast<PartitionDirectMapMetadata<thread_safe>*>(slot_span);
  PA_DCHECK(&metadata->page.slot_span_metadata == slot_span);
  return &metadata->direct_map_extent;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_DIRECT_MAP_EXTENT_H_
