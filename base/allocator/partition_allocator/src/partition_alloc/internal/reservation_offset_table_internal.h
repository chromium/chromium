// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_INTERNAL_RESERVATION_OFFSET_TABLE_INTERNAL_H_
#define PARTITION_ALLOC_INTERNAL_RESERVATION_OFFSET_TABLE_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <limits>

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/reservation_offset_table.h"

namespace partition_alloc::internal {

PA_ALWAYS_INLINE ReservationOffsetTable
ReservationOffsetTable::Get(pool_handle handle) {
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  PoolOffsetLookup offset_lookup =
      PartitionAddressSpace::GetOffsetLookup(handle);
  switch (handle) {
    case kRegularPoolHandle:
      return ReservationOffsetTable(regular_pool_table_, offset_lookup);
    case kBRPPoolHandle:
      return ReservationOffsetTable(brp_pool_table_, offset_lookup);
    case kConfigurablePoolHandle:
      return ReservationOffsetTable(configurable_pool_table_, offset_lookup);
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
    case kThreadIsolatedPoolHandle:
      return ReservationOffsetTable(thread_isolated_pool_table_, offset_lookup);
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
    default:
      PA_NOTREACHED();
  }
#else
  return ReservationOffsetTable(reservation_offset_table_);
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
}

PA_ALWAYS_INLINE ReservationOffsetTable
ReservationOffsetTable::Get(uintptr_t address) {
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  return ReservationOffsetTable::Get(
      PartitionAddressSpace::GetPoolInfo(address).handle);
#else
  return ReservationOffsetTable(reservation_offset_table_);
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
}

PA_ALWAYS_INLINE void* ReservationOffsetTable::GetData() const {
  return table_begin_;
}

PA_ALWAYS_INLINE void ReservationOffsetTable::SetNotAllocatedTag(
    uintptr_t reservation_start,
    size_t reservation_size) const {
  PA_DCHECK((reservation_start & kSuperPageOffsetMask) == 0);
  PA_DCHECK(reservation_size > 0);
  uint16_t* offset_ptr = GetOffsetPointer(reservation_start);

  PA_DCHECK((reservation_size - 1) >> kSuperPageShift <=
            std::numeric_limits<uint16_t>::max());
  const uint16_t offset_end =
      static_cast<uint16_t>((reservation_size - 1) >> kSuperPageShift);
  for (uint16_t offset = 0; offset <= offset_end; ++offset) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
    PA_DCHECK(offset_ptr < table_end_);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
    *PA_UNSAFE_TODO(offset_ptr++) = kOffsetTagNotAllocated;
  }
}

PA_ALWAYS_INLINE void ReservationOffsetTable::SetNormalBucketsTag(
    uintptr_t reservation_start) const {
  PA_DCHECK((reservation_start & kSuperPageOffsetMask) == 0);
  *GetOffsetPointer(reservation_start) = kOffsetTagNormalBuckets;
}

PA_ALWAYS_INLINE void ReservationOffsetTable::SetDirectMapReservationStart(
    uintptr_t reservation_start,
    size_t reservation_size) const {
  PA_DCHECK((reservation_start & kSuperPageOffsetMask) == 0);
  PA_DCHECK(reservation_size > 0);
  uint16_t* offset_ptr = GetOffsetPointer(reservation_start);

  PA_DCHECK((reservation_size - 1) >> kSuperPageShift <=
            std::numeric_limits<uint16_t>::max());
  const uint16_t offset_end =
      static_cast<uint16_t>((reservation_size - 1) >> kSuperPageShift);
  for (uint16_t offset = 0; offset <= offset_end; ++offset) {
    PA_DCHECK(offset < kOffsetTagNormalBuckets);
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
    PA_DCHECK(offset_ptr < table_end_);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
    *PA_UNSAFE_TODO(offset_ptr++) = offset;
  }
}

// If the given address doesn't point to direct-map allocated memory,
// returns 0.
PA_ALWAYS_INLINE uintptr_t
ReservationOffsetTable::GetDirectMapReservationStart(uintptr_t address) {
  uint16_t* offset_ptr = GetOffsetPointer(address);
  PA_DCHECK(*offset_ptr != kOffsetTagNotAllocated);
  if (*offset_ptr == kOffsetTagNormalBuckets) {
    return 0;
  }
  uintptr_t reservation_start =
      (address & kSuperPageBaseMask) -
      (static_cast<size_t>(*offset_ptr) << kSuperPageShift);

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  PA_DCHECK(offset_lookup_.Includes(reservation_start));
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  PA_DCHECK(*GetOffsetPointer(reservation_start) == 0);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

  return reservation_start;
}

// Returns true if |address| is the beginning of the first super page of a
// reservation, i.e. either a normal bucket super page, or the first super
// page of direct map. |address| must belong to an allocated super page.
PA_ALWAYS_INLINE bool ReservationOffsetTable::IsReservationStart(
    uintptr_t address) const {
  uint16_t* offset_ptr = GetOffsetPointer(address);
  PA_DCHECK(*offset_ptr != kOffsetTagNotAllocated);
  return ((*offset_ptr == kOffsetTagNormalBuckets) || (*offset_ptr == 0)) &&
         (address % kSuperPageSize == 0);
}

// Returns true if |address| belongs to a normal bucket super page.
PA_ALWAYS_INLINE bool ReservationOffsetTable::IsManagedByNormalBuckets(
    uintptr_t address) const {
  uint16_t* offset_ptr = GetOffsetPointer(address);
  return *offset_ptr == kOffsetTagNormalBuckets;
}

// Returns true if |address| belongs to a direct map region.
PA_ALWAYS_INLINE bool ReservationOffsetTable::IsManagedByDirectMap(
    uintptr_t address) const {
  uint16_t* offset_ptr = GetOffsetPointer(address);
  return *offset_ptr != kOffsetTagNormalBuckets &&
         *offset_ptr != kOffsetTagNotAllocated;
}

// Returns true if |address| belongs to a normal bucket super page or a direct
// map region, i.e. belongs to an allocated super page.
PA_ALWAYS_INLINE bool
ReservationOffsetTable::IsManagedByNormalBucketsOrDirectMap(
    uintptr_t address) const {
  uint16_t* offset_ptr = GetOffsetPointer(address);
  return *offset_ptr != kOffsetTagNotAllocated;
}

PA_ALWAYS_INLINE uint16_t* ReservationOffsetTable::GetOffsetPointerForTesting(
    uintptr_t address) const {
  return GetOffsetPointer(address);
}

PA_ALWAYS_INLINE uint16_t* ReservationOffsetTable::GetOffsetPointer(
    uintptr_t address) const {
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  size_t table_index = offset_lookup_.GetOffset(address) >> kSuperPageShift;
#else
  size_t table_index = address >> kSuperPageShift;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  uint16_t* offset_ptr = &PA_UNSAFE_TODO(table_begin_[table_index]);
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_DCHECK(offset_ptr < table_end_);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
  return offset_ptr;
}

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_INTERNAL_RESERVATION_OFFSET_TABLE_INTERNAL_H_
