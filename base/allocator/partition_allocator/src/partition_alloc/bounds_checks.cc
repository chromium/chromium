// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/bounds_checks.h"

#include <cstdint>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_direct_map_extent.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/slot_address_and_size.h"
#include "partition_alloc/slot_start.h"
#include "partition_alloc/tagging.h"

namespace partition_alloc {

namespace {

PtrPosWithinAlloc IsPtrWithinSameAlloc(uintptr_t orig_address,
                                       uintptr_t test_address,
                                       size_t type_size,
                                       internal::pool_handle pool) {
  auto [slot_start, _] = SlotAddressAndSize::From(orig_address, pool);
  // Don't use |orig_address| beyond this point at all. It was needed to
  // pick the right slot, but now we're dealing with very concrete addresses.
  // Zero it just in case, to catch errors.
  orig_address = 0;

  std::ptrdiff_t offset = internal::GetMetadataOffset(pool);
  auto* slot_span =
      internal::SlotSpanMetadata::FromSlotStart(slot_start, offset);
  auto* root = PartitionRoot::FromSlotSpanMetadata(slot_span);

  uintptr_t object_addr = slot_start.value();
  uintptr_t object_end = object_addr + root->GetSlotUsableSize(slot_span);
  if (test_address < object_addr || object_end < test_address) {
    return PtrPosWithinAlloc::kFarOOB;
#if PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
  } else if (object_end - type_size < test_address) {
    // Not even a single element of the type referenced by the pointer can fit
    // between the pointer and the end of the object.
    return PtrPosWithinAlloc::kAllocEnd;
#endif
  }
  return PtrPosWithinAlloc::kInBounds;
}

}  // namespace

PtrPosWithinAlloc IsPtrWithinSameAllocInBRPPool(uintptr_t orig_address,
                                                uintptr_t test_address,
                                                size_t type_size) {
  internal::DCheckIfManagedByPartitionAllocBRPPool(orig_address);
  PA_DCHECK(internal::ReservationOffsetTable::Get(
                internal::pool_handle::kBRPPoolHandle)
                .IsManagedByNormalBucketsOrDirectMap(orig_address));

  return IsPtrWithinSameAlloc(orig_address, test_address, type_size,
                              internal::pool_handle::kBRPPoolHandle);
}

// This implementation collapses into a constexpr false when Checked
// Span is not built.
#if PA_BUILDFLAG(CHECKED_SPAN)
bool IsExtentOutOfBounds(const void* ptr,
                         size_t extent_bytes,
                         size_t type_size) {
  const uintptr_t address = partition_alloc::UntagPtr(ptr);

  if (!partition_alloc::IsManagedByPartitionAlloc(address)) {
    return false;
  }
  const auto pool = partition_alloc::internal::GetPool(address);
  if (!partition_alloc::internal::ReservationOffsetTable::Get(pool)
           .IsManagedByNormalBucketsOrDirectMap(address)) {
    return false;
  }

  return IsPtrWithinSameAlloc(address, address + extent_bytes, type_size,
                              pool) == PtrPosWithinAlloc::kFarOOB;
}
#endif  // PA_BUILDFLAG(CHECKED_SPAN)

}  // namespace partition_alloc
