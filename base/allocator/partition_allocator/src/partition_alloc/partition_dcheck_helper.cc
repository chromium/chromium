// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_dcheck_helper.h"

#include <cstdint>

#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/partition_superpage_extent_entry.h"

namespace partition_alloc::internal {

#if PA_BUILDFLAG(DCHECKS_ARE_ON)

void DCheckIsValidShiftFromSlotStart(
    const SlotSpanMetadata<MetadataKind::kReadOnly>* slot_span,
    uintptr_t shift_from_slot_start) {
  PartitionRoot* root = PartitionRoot::FromSlotSpanMetadata(slot_span);
  // Use <= to allow an address immediately past the object.
  PA_DCHECK(shift_from_slot_start <= root->GetSlotUsableSize(slot_span));
}

void DCheckIsValidObjectAddress(
    const SlotSpanMetadata<MetadataKind::kReadOnly>* slot_span,
    uintptr_t object_addr) {
  uintptr_t slot_span_start =
      SlotSpanMetadata<MetadataKind::kReadOnly>::ToSlotSpanStart(slot_span);
  PA_DCHECK((object_addr - slot_span_start) % slot_span->bucket->slot_size ==
            0);
}

void DCheckNumberOfPartitionPagesInSuperPagePayload(
    PartitionSuperPageExtentEntry<MetadataKind::kWritable>* entry,
    const PartitionRoot* root,
    size_t number_of_nonempty_slot_spans) {
  PartitionSuperPageExtentEntry<MetadataKind::kReadOnly>* readonly_entry =
      entry->ToReadOnly(root);
  uintptr_t entry_address = reinterpret_cast<uintptr_t>(readonly_entry);
  uintptr_t super_page =
      base::bits::AlignDown(entry_address, kSuperPageAlignment);
  size_t number_of_partition_pages_in_superpage_payload =
      SuperPagePayloadSize(super_page) / PartitionPageSize();
  PA_DCHECK(number_of_partition_pages_in_superpage_payload >
            number_of_nonempty_slot_spans);
}

void DCheckRootLockIsAcquired(PartitionRoot* root) {
  PartitionRootLock(root).AssertAcquired();
}

#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

bool DeducedRootIsValid(SlotSpanMetadata<MetadataKind::kReadOnly>* slot_span) {
  PartitionRoot* root = PartitionRoot::FromSlotSpanMetadata(slot_span);
  return root->inverted_self == ~reinterpret_cast<uintptr_t>(root);
}

}  // namespace partition_alloc::internal
