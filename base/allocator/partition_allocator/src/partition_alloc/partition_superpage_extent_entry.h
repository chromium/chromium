// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_SUPERPAGE_EXTENT_ENTRY_H_
#define PARTITION_ALLOC_PARTITION_SUPERPAGE_EXTENT_ENTRY_H_

#include <cstdint>

#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_forward.h"
// Should not include partition_root.h, partition_bucket.h, partition_page.h.

namespace partition_alloc::internal {

// An "extent" is a span of consecutive superpages. We link the partition's next
// extent (if there is one) to the very start of a superpage's metadata area.
struct PartitionSuperPageExtentEntry {
  PartitionRoot* root;
  PartitionSuperPageExtentEntry* next;
  uint16_t number_of_consecutive_super_pages;
  uint16_t number_of_nonempty_slot_spans;

  PA_ALWAYS_INLINE void IncrementNumberOfNonemptySlotSpans();

  PA_ALWAYS_INLINE void DecrementNumberOfNonemptySlotSpans();
};

static_assert(
    sizeof(PartitionSuperPageExtentEntry) <= kPageMetadataSize,
    "PartitionSuperPageExtentEntry must be able to fit in a metadata slot");
static_assert(kMaxSuperPagesInPool / kSuperPageSize <=
                  std::numeric_limits<
                      decltype(PartitionSuperPageExtentEntry ::
                                   number_of_consecutive_super_pages)>::max(),
              "number_of_consecutive_super_pages must be big enough");
}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PARTITION_SUPERPAGE_EXTENT_ENTRY_H_
