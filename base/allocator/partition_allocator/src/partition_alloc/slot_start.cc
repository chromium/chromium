// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/slot_start.h"

#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"

// TODO(crbug.com/459322791): Enforce this check in non-debug builds.
#if PA_BUILDFLAG(DCHECKS_ARE_ON)

namespace partition_alloc::internal {

namespace {
PA_ALWAYS_INLINE
void CheckIsSlotStart([[maybe_unused]] uintptr_t untagged_slot_start,
                      [[maybe_unused]] const PartitionRoot* root) {
  auto* slot_span_metadata =
      SlotSpanMetadata::FromAddr(untagged_slot_start, root);
  uintptr_t slot_span =
      SlotSpanMetadata::ToSlotSpanStart(slot_span_metadata, root).value();
  PA_CHECK(!((untagged_slot_start - slot_span) %
             slot_span_metadata->bucket->slot_size));
}
}  // namespace

void SlotStart::Check(const PartitionRoot* root) const {
  CheckIsSlotStart(UntagAddr(address_), root);
}

void UntaggedSlotStart::Check(const PartitionRoot* root) const {
  CheckIsSlotStart(address_, root);
}

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
