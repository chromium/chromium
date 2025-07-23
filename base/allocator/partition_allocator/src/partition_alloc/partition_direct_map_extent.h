// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_DIRECT_MAP_EXTENT_H_
#define PARTITION_ALLOC_PARTITION_DIRECT_MAP_EXTENT_H_

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_page.h"

namespace partition_alloc::internal {

struct PartitionDirectMapExtent {
  PartitionDirectMapExtent* next_extent;
  PartitionDirectMapExtent* prev_extent;
  PartitionBucket* bucket;
  // Size of the entire reservation, including guard pages, meta-data,
  // padding for alignment before allocation, and padding for granularity at the
  // end of the allocation.
  size_t reservation_size;
  // Padding between the first partition page (guard pages + meta-data) and
  // the allocation.
  size_t padding_for_alignment;

  PA_ALWAYS_INLINE static PartitionDirectMapExtent* FromSlotSpanMetadata(
      SlotSpanMetadata* slot_span);
};

// Metadata page for direct-mapped allocations.
struct PartitionDirectMapMetadata {
  // |page_metadata| and |second_page_metadata| are needed to match the
  // layout of normal buckets (specifically, of single-slot slot spans), with
  // the caveat that only the first subsequent page is needed (for
  // SubsequentPageMetadata) and others aren't used for direct map.
  PartitionPageMetadata page_metadata;
  PartitionPageMetadata second_page_metadata;
  // The following fields are metadata specific to direct map allocations. All
  // these fields will easily fit into the precalculated metadata region,
  // because a direct map allocation starts no further than half way through the
  // super page.
  PartitionBucket bucket;
  PartitionDirectMapExtent direct_map_extent;

  PA_ALWAYS_INLINE static PartitionDirectMapMetadata* FromSlotSpanMetadata(
      SlotSpanMetadata* slot_span);
};

PA_ALWAYS_INLINE PartitionDirectMapMetadata*
PartitionDirectMapMetadata::FromSlotSpanMetadata(SlotSpanMetadata* slot_span) {
  PA_DCHECK(slot_span->bucket->is_direct_mapped());
  // |*slot_span| is the first field of |PartitionDirectMapMetadata|, just cast.
  auto* metadata = reinterpret_cast<PartitionDirectMapMetadata*>(slot_span);
  PA_DCHECK(&metadata->page_metadata.slot_span_metadata == slot_span);
  return metadata;
}

PA_ALWAYS_INLINE PartitionDirectMapExtent*
PartitionDirectMapExtent::FromSlotSpanMetadata(SlotSpanMetadata* slot_span) {
  PA_DCHECK(slot_span->bucket->is_direct_mapped());
  return &PartitionDirectMapMetadata::FromSlotSpanMetadata(slot_span)
              ->direct_map_extent;
}

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PARTITION_DIRECT_MAP_EXTENT_H_
