// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/pointers/realloc_protected_iterator.h"

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#include "partition_alloc/address_pool_manager_bitmap.h"
#include "partition_alloc/in_slot_metadata.h"
#include "partition_alloc/internal/partition_root_internal.h"  // nogncheck
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/slot_address_and_size.h"
#include "partition_alloc/slot_start.h"
#include "partition_alloc/tagging.h"
#endif

namespace base::internal {

WrappedBackingSlot WrapBackingSlot([[maybe_unused]] const void* p) {
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (!p) {
    return {};
  }
  const uintptr_t addr = partition_alloc::UntagPtr(p);
  if (!partition_alloc::IsManagedByPartitionAllocBRPPool(addr)) {
    return {};
  }
  auto [slot_start, slot_size] =
      partition_alloc::SlotAddressAndSize::FromBRPPool(addr);
  partition_alloc::PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
      partition_alloc::internal::UntaggedSlotStart(slot_start), slot_size)
      ->AcquireFromUnprotectedPtr();
  return {slot_start.value(), slot_size};
#else
  return {};
#endif
}

void UnwrapBackingSlot([[maybe_unused]] WrappedBackingSlot slot) {
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (!slot) {
    return;
  }
  auto untagged =
      partition_alloc::internal::UntaggedSlotStart::Unchecked(slot.slot_start);
  auto* metadata =
      partition_alloc::PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
          untagged, slot.slot_size);

  // Security check: if the backing was freed while the wrapper held its ref,
  // the slot's "allocated" bit in InSlotMetadata is clear. That means the
  // container reallocated (or otherwise freed its backing) while iteration
  // was in progress -- a classic iterator-UAF setup. PA's BRP quarantine
  // kept the slot from being reused, but rather than let the caller continue
  // reading zapped memory, crash now.
  PA_BASE_CHECK(metadata->IsAlive());

  if (metadata->ReleaseFromUnprotectedPtr()) {
    partition_alloc::PartitionRoot::FreeAfterBRPQuarantine(untagged,
                                                           slot.slot_size);
  }
#endif
}

}  // namespace base::internal
