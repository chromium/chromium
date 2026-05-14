// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_INTERNAL_PARTITION_SUPERPAGE_EXTENT_ENTRY_INTERNAL_H_
#define PARTITION_ALLOC_INTERNAL_PARTITION_SUPERPAGE_EXTENT_ENTRY_INTERNAL_H_

#include <cstdint>

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/address_pool_manager_types.h"
#include "partition_alloc/partition_alloc-inl.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_dcheck_helper.h"
#include "partition_alloc/partition_superpage_extent_entry.h"
#include "partition_alloc/reservation_offset_table.h"

// Should not include partition_root.h, partition_bucket.h, partition_page.h.

namespace partition_alloc::internal {

PA_ALWAYS_INLINE void
PartitionSuperPageExtentEntry::IncrementNumberOfNonemptySlotSpans() {
  DCheckNumberOfPartitionPagesInSuperPagePayload(this, root,
                                                 number_of_nonempty_slot_spans);
  ++number_of_nonempty_slot_spans;
}

PA_ALWAYS_INLINE void
PartitionSuperPageExtentEntry::DecrementNumberOfNonemptySlotSpans() {
  PA_DCHECK(number_of_nonempty_slot_spans);
  --number_of_nonempty_slot_spans;
}
// Returns the base of the first super page in the range of consecutive super
// pages.
//
// CAUTION! |extent| must point to the extent of the first super page in the
// range of consecutive super pages.
PA_ALWAYS_INLINE uintptr_t
SuperPagesBeginFromExtent(const PartitionSuperPageExtentEntry* extent) {
  PA_DCHECK(0 < extent->number_of_consecutive_super_pages);
  uintptr_t extent_as_uintptr = PartitionMetadataPageToSuperPage(
      reinterpret_cast<uintptr_t>(extent), GetMetadataOffset(extent->root));
  PA_DCHECK(ReservationOffsetTable::Get(extent_as_uintptr)
                .IsManagedByNormalBuckets(extent_as_uintptr));
  return base::bits::AlignDown(extent_as_uintptr, kSuperPageAlignment);
}

// Returns the base of the first super page in the range of consecutive super
// pages.
//
// CAUTION! |extent| must point to the extent of the first super page in the
// range of consecutive super pages.
uintptr_t SuperPagesBeginFromExtent(
    const PartitionSuperPageExtentEntry* extent);

// Returns the end of the last super page in the range of consecutive
// super pages.
//
// CAUTION! |extent| must point to the extent of the first super page in
// the range of consecutive super pages.
PA_ALWAYS_INLINE uintptr_t
SuperPagesEndFromExtent(const PartitionSuperPageExtentEntry* extent) {
  return SuperPagesBeginFromExtent(extent) +
         (extent->number_of_consecutive_super_pages * kSuperPageSize);
}

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_INTERNAL_PARTITION_SUPERPAGE_EXTENT_ENTRY_INTERNAL_H_
