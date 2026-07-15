// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/slot_address_and_size.h"

#include <cstddef>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/internal/partition_root_internal.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_direct_map_extent.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/slot_start.h"

namespace partition_alloc {

namespace {

PA_ALWAYS_INLINE SlotAddressAndSize
FromDirectMap(const uintptr_t address,
              const internal::pool_handle pool,
              uintptr_t reservation_start,
              std::ptrdiff_t metadata_offset) {
  PA_DCHECK(reservation_start);
  // The direct map allocation may not start exactly from the first page, as
  // there may be padding for alignment. The first page metadata holds an
  // offset to where direct map metadata, and thus direct map start, are
  // located.
  auto* first_page_metadata = internal::PartitionPageMetadata::FromAddr(
      reservation_start + PartitionPageSize(), metadata_offset);
  auto* page_metadata = PA_UNSAFE_TODO(
      first_page_metadata + first_page_metadata->slot_span_metadata_offset);
  PA_DCHECK(page_metadata->is_valid);
  PA_DCHECK(!page_metadata->slot_span_metadata_offset);
  auto* slot_span = &page_metadata->slot_span_metadata;
  internal::SlotSpanStart slot_span_start =
      internal::SlotSpanMetadata::ToSlotSpanStart(slot_span, metadata_offset);
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  auto* direct_map_metadata =
      internal::PartitionDirectMapMetadata::FromSlotSpanMetadata(slot_span);
  size_t padding_for_alignment =
      direct_map_metadata->direct_map_extent.padding_for_alignment;
  PA_DCHECK(padding_for_alignment ==
            static_cast<size_t>(page_metadata - first_page_metadata) *
                PartitionPageSize());
  PA_DCHECK(slot_span_start ==
            reservation_start + PartitionPageSize() + padding_for_alignment);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
  return SlotAddressAndSize{.slot_start = slot_span_start.AsSlotStart(),
                            .size = slot_span->bucket->slot_size};
}

}  // namespace

SlotAddressAndSize SlotAddressAndSize::From(
    uintptr_t address,
    internal::pool_handle pool,
    internal::ReservationOffsetTableAddressInfo reservation_info,
    std::ptrdiff_t metadata_offset) {
  if (reservation_info.GetType() ==
      internal::ReservationOffsetTableAddressInfo::Type::kDirectMap)
      [[unlikely]] {
    auto directmap_slot_info = FromDirectMap(
        address, pool, reservation_info.GetDirectMapReservationStart(),
        metadata_offset);
    PA_CHECK(directmap_slot_info.slot_start);
    return directmap_slot_info;
  }

  auto* slot_span =
      internal::SlotSpanMetadata::FromAddr(address, metadata_offset);

  // Get the offset from the beginning of the slot span.
  internal::SlotSpanStart slot_span_start =
      internal::SlotSpanMetadata::ToSlotSpanStart(slot_span, metadata_offset);
  size_t offset_in_slot_span = address - slot_span_start.value();

  auto* bucket = slot_span->bucket;

  // This branch handles a mystery whose root cause is still unknown.
  // If we end up staring at crashes from here, it may be worth
  // upgrading some of the DCHECK()s in `SlotSpanMetadata::FromAddr()`.
  //
  // See also: https://crrev.com/c/7953898
  if (!bucket) [[unlikely]] {
    PA_DEBUG_DATA_ON_STACK("address", address);
    PA_DEBUG_DATA_ON_STACK("pool", pool);
    PA_IMMEDIATE_CRASH();
  }
  return SlotAddressAndSize{
      .slot_start = internal::UntaggedSlotStart::Unchecked(
          slot_span_start.value() +
          bucket->slot_size * bucket->GetSlotNumber(offset_in_slot_span)),
      .size = bucket->slot_size};
}

SlotAddressAndSize SlotAddressAndSize::From(uintptr_t address,
                                            internal::pool_handle pool) {
  auto reservation_info =
      internal::ReservationOffsetTable::Get(pool).GetAddressInfo(address);
  return SlotAddressAndSize::From(address, pool, reservation_info,
                                  internal::GetMetadataOffset(pool));
}

}  // namespace partition_alloc
