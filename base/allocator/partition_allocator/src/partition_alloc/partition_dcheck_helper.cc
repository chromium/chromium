// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/partition_dcheck_helper.h"

#include <cstdint>

#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"

namespace partition_alloc::internal {

#if PA_BUILDFLAG(DCHECKS_ARE_ON)

void DCheckIsValidShiftFromSlotStart(const SlotSpanMetadata* slot_span,
                                     uintptr_t shift_from_slot_start) {
  PartitionRoot* root = PartitionRoot::FromSlotSpanMetadata(slot_span);
  // Use <= to allow an address immediately past the object.
  PA_DCHECK(shift_from_slot_start <= root->GetSlotUsableSize(slot_span));
}

void DCheckIsValidObjectAddress(const SlotSpanMetadata* slot_span,
                                uintptr_t object_addr) {
  PartitionRoot* root = PartitionRoot::FromSlotSpanMetadata(slot_span);
  uintptr_t slot_span_start =
      SlotSpanMetadata::ToSlotSpanStart(slot_span, root->MetadataOffset())
          .value();
  PA_DCHECK((object_addr - slot_span_start) % slot_span->bucket->slot_size ==
            0);
}

void DCheckNumberOfPartitionPagesInSuperPagePayload(
    const PartitionSuperPageExtentEntry* entry,
    const PartitionRoot* root,
    size_t number_of_nonempty_slot_spans) {
  uintptr_t super_page = base::bits::AlignDown(
      reinterpret_cast<uintptr_t>(entry), kSuperPageAlignment);
  size_t number_of_partition_pages_in_superpage_payload =
      SuperPagePayloadSize(super_page) / PartitionPageSize();
  PA_DCHECK(number_of_partition_pages_in_superpage_payload >
            number_of_nonempty_slot_spans);
}

void DCheckRootLockIsAcquired(PartitionRoot* root) {
  PartitionRootLock(root).AssertAcquired();
}

#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

bool DeducedRootIsValid(const SlotSpanMetadata* slot_span) {
  PartitionRoot* root = PartitionRoot::FromSlotSpanMetadata(slot_span);
  return root->inverted_self == ~reinterpret_cast<uintptr_t>(root);
}

}  // namespace partition_alloc::internal
