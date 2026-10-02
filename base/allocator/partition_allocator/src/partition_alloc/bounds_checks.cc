// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/bounds_checks.h"

#include <cstdint>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/in_slot_metadata.h"
#include "partition_alloc/internal/partition_root_internal.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/numerics/checked_math.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_public_constants.h"
#include "partition_alloc/partition_direct_map_extent.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/slot_address_and_size.h"
#include "partition_alloc/slot_start.h"
#include "partition_alloc/tagging.h"

namespace partition_alloc {

namespace {

// Build support permitting, `CHECK()`s that the `is_allocated` bit of
// the `InSlotMetadata` is set.
enum class SlotLiveness {
  kCheck,
  kDontCheck,
};

template <SlotLiveness check>
size_t UsableOrSmuggledSizeFrom(const std::ptrdiff_t offset,
                                const SlotAddressAndSize slot_and_size) {
  auto* slot_span = internal::SlotSpanMetadata::FromSlotStart(
      slot_and_size.slot_start, offset);
  const auto* root = PartitionRoot::FromSlotSpanMetadata(slot_span);
  const size_t usable_size = root->GetSlotUsableSize(slot_span);

#if PA_BUILDFLAG(CHECKED_SPAN_HAS_METADATA_SUPPORT)
  if (root->brp_enabled()) [[likely]] {
    internal::InSlotMetadata* metadata =
        internal::InSlotMetadata::From(slot_and_size);
    if constexpr (check == SlotLiveness::kCheck) {
      metadata->EnsureAlive(slot_and_size.slot_start, slot_span);
    }

    if (metadata->IsSmuggledSizeAvailable()) {
      auto smuggled_size =
          internal::GetSmuggledSize(slot_and_size.slot_start, usable_size);
      // The usable size is the absolute upper bound; if the smuggled
      // size exceeds this, this slot has probably been corrupted.
      PA_CHECK(smuggled_size <= usable_size);
      return smuggled_size;
    }
  }
#endif  // PA_BUILDFLAG(CHECKED_SPAN_HAS_METADATA_SUPPORT)

  return usable_size;
}

template <SlotLiveness check = SlotLiveness::kDontCheck>
PtrPosWithinAlloc IsPtrWithinSameAlloc(
    uintptr_t orig_address,
    uintptr_t test_address,
    size_t type_size,
    internal::pool_handle pool,
    internal::ReservationOffsetTableAddressInfo offset_info) {
  const std::ptrdiff_t offset = internal::GetMetadataOffset(pool);
  const auto slot_and_size =
      SlotAddressAndSize::From(orig_address, pool, offset_info, offset);
  // Don't use |orig_address| beyond this point at all. It was needed to
  // pick the right slot, but now we're dealing with very concrete addresses.
  // Zero it just in case, to catch errors.
  orig_address = 0;

  size_t extent = UsableOrSmuggledSizeFrom<check>(offset, slot_and_size);

  uintptr_t object_addr = slot_and_size.slot_start.value();
  uintptr_t object_end = object_addr + extent;
  if (test_address < object_addr || object_end < test_address) [[unlikely]] {
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
                              internal::pool_handle::kBRPPoolHandle,
                              internal::ReservationOffsetTable::Get(
                                  internal::pool_handle::kBRPPoolHandle)
                                  .GetAddressInfo(orig_address));
}

bool IsExtentOutOfBounds(const void* ptr,
                         size_t extent_bytes,
                         size_t type_size) {
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  if (!extent_bytes) {
    return false;
  }

  const uintptr_t address = partition_alloc::UntagPtr(ptr);
  if (!partition_alloc::IsManagedByPartitionAlloc(address)) {
    return false;
  }

  const auto pool = partition_alloc::internal::GetPool(address);
  auto reservation_offset_table =
      partition_alloc::internal::ReservationOffsetTable::Get(pool);
  auto offset_info = reservation_offset_table.GetAddressInfo(address);
  if (offset_info.GetType() ==
      internal::ReservationOffsetTableAddressInfo::kNotAllocated) [[unlikely]] {
    return false;
  }

  return IsPtrWithinSameAlloc<SlotLiveness::kCheck>(
             address,
             internal::base::CheckAdd(address, extent_bytes).ValueOrDie(),
             type_size, pool, offset_info) == PtrPosWithinAlloc::kFarOOB;
#else
  return false;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
}

}  // namespace partition_alloc
