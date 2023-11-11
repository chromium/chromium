// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_SUPERPAGE_EXTENT_ENTRY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_SUPERPAGE_EXTENT_ENTRY_H_

#include <cstdint>

#include "base/allocator/partition_allocator/src/partition_alloc/address_pool_manager.h"
#include "base/allocator/partition_allocator/src/partition_alloc/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_dcheck_helper.h"
#include "base/allocator/partition_allocator/src/partition_alloc/reservation_offset_table.h"

// Should not include partition_root.h, partition_bucket.h, partition_page.h.
// For IsQuarantineAllowed(), use partition_dcheck_helper.h instead of
// partition_root.h.

namespace partition_alloc::internal {

// An "extent" is a span of consecutive superpages. We link the partition's next
// extent (if there is one) to the very start of a superpage's metadata area.
struct PartitionSuperPageExtentEntry {
  PartitionRoot* root;
  PartitionSuperPageExtentEntry* next;
  uint16_t number_of_consecutive_super_pages;
  uint16_t number_of_nonempty_slot_spans;

  PA_ALWAYS_INLINE void IncrementNumberOfNonemptySlotSpans() {
    DCheckNumberOfPartitionPagesInSuperPagePayload(
        this, root, number_of_nonempty_slot_spans);
    ++number_of_nonempty_slot_spans;
  }

  PA_ALWAYS_INLINE void DecrementNumberOfNonemptySlotSpans() {
    PA_DCHECK(number_of_nonempty_slot_spans);
    --number_of_nonempty_slot_spans;
  }
};

static_assert(
    sizeof(PartitionSuperPageExtentEntry) <= kPageMetadataSize,
    "PartitionSuperPageExtentEntry must be able to fit in a metadata slot");
static_assert(kMaxSuperPagesInPool / kSuperPageSize <=
                  std::numeric_limits<
                      decltype(PartitionSuperPageExtentEntry ::
                                   number_of_consecutive_super_pages)>::max(),
              "number_of_consecutive_super_pages must be big enough");

// Returns the base of the first super page in the range of consecutive super
// pages.
//
// CAUTION! |extent| must point to the extent of the first super page in the
// range of consecutive super pages.
PA_ALWAYS_INLINE uintptr_t
SuperPagesBeginFromExtent(const PartitionSuperPageExtentEntry* extent) {
  PA_DCHECK(0 < extent->number_of_consecutive_super_pages);
  uintptr_t extent_as_uintptr = reinterpret_cast<uintptr_t>(extent);
  PA_DCHECK(IsManagedByNormalBuckets(extent_as_uintptr));
  return base::bits::AlignDown(extent_as_uintptr, kSuperPageAlignment);
}

// Returns the end of the last super page in the range of consecutive super
// pages.
//
// CAUTION! |extent| must point to the extent of the first super page in the
// range of consecutive super pages.
PA_ALWAYS_INLINE uintptr_t
SuperPagesEndFromExtent(const PartitionSuperPageExtentEntry* extent) {
  return SuperPagesBeginFromExtent(extent) +
         (extent->number_of_consecutive_super_pages * kSuperPageSize);
}

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_SUPERPAGE_EXTENT_ENTRY_H_
