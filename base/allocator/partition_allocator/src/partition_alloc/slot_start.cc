// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/slot_start.h"

#include "partition_alloc/buildflags.h"
#include "partition_alloc/internal/partition_root_internal.h"
#include "partition_alloc/partition_page.h"

// TODO(crbug.com/459322791): Enforce this check in non-debug builds.
#if PA_BUILDFLAG(DCHECKS_ARE_ON)

namespace partition_alloc {

namespace {
PA_ALWAYS_INLINE
void CheckIsSlotStart([[maybe_unused]] uintptr_t untagged_slot_start,
                      [[maybe_unused]] const PartitionRoot* root) {
  auto* slot_span_metadata =
      internal::SlotSpanMetadata::FromAddr(untagged_slot_start, root);
  uintptr_t slot_span =
      internal::SlotSpanMetadata::ToSlotSpanStart(slot_span_metadata, root)
          .value();
  size_t offset_in_slot_span = untagged_slot_start - slot_span;
  auto* bucket = slot_span_metadata->bucket;
  if (offset_in_slot_span <= BucketIndexLookup::kMaxBucketSize &&
      bucket->slot_size <= BucketIndexLookup::kMaxBucketSize) [[likely]] {
    PA_CHECK(bucket->GetSlotNumber(offset_in_slot_span) * bucket->slot_size ==
             offset_in_slot_span);
  } else {
    PA_CHECK(!(offset_in_slot_span % bucket->slot_size));
  }
}
}  // namespace

void SlotStart::Check(const PartitionRoot* root) const {
  CheckIsSlotStart(internal::UntagAddr(address_), root);
}

void UntaggedSlotStart::Check(const PartitionRoot* root) const {
  CheckIsSlotStart(address_, root);
}

}  // namespace partition_alloc

#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
