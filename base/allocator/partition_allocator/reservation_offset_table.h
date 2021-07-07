// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_RESERVATION_OFFSET_TABLE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_RESERVATION_OFFSET_TABLE_H_

#include <cstddef>
#include <cstdint>
#include <limits>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

namespace base {
namespace internal {

static constexpr uint16_t kOffsetTagNotAllocated =
    std::numeric_limits<uint16_t>::max();
static constexpr uint16_t kOffsetTagNormalBuckets =
    std::numeric_limits<uint16_t>::max() - 1;

// The main purpose of the reservation offset table is to easily locate the
// direct map reservation start address for any given address. There is one
// entry if the table for each super page.
//
// When PartitionAlloc reserves an address region it is always aligned to
// super page boundary. However, in 32-bit mode, the size may not be aligned
// super-page-aligned, so it may look like this:
//   |<--------- actual reservation size --------->|
//   +----------+----------+-----+-----------+-----+ - - - +
//   |SuperPage0|SuperPage1| ... |SuperPage K|SuperPage K+1|
//   +----------+----------+-----+-----------+-----+ - - -.+
//                                           |<-X->|<-Y*)->|
//
// The table entries for reserved super pages say how many pages away from the
// reservation the super page is:
//   +----------+----------+-----+-----------+-------------+
//   |Entry for |Entry for | ... |Entry for  |Entry for    |
//   |SuperPage0|SuperPage1|     |SuperPage K|SuperPage K+1|
//   +----------+----------+-----+-----------+-------------+
//   |     0    |    1     | ... |     K     |   K + 1     |
//   +----------+----------+-----+-----------+-------------+
//
// For an address Z, the reservation start can be found using this formula:
//   ((Z >> kSuperPageShift) - (the entry for Z)) << kSuperPageShift
//
// kOffsetTagNotAllocated is a special tag denoting that the super page isn't
// allocated by PartitionAlloc and kOffsetTagNormalBuckets denotes that it is
// used for a normal-bucket allocation, not for a direct-map allocation.
//
// *) In 32-bit mode, Y is not used by PartitionAlloc, and cannot be used
//    until X is unreserved, because PartitionAlloc always uses kSuperPageSize
//    alignment when reserving address spaces. One can use "GigaCage" to
//    further determine which part of the supe page is used by PartitionAlloc.
//    This isn't a problem in 64-bit mode, where allocation granularity is
//    kSuperPageSize.
class BASE_EXPORT ReservationOffsetTable {
 public:
#if defined(PA_HAS_64_BITS_POINTERS)
  // The size of the reservation offset table should cover the entire GigaCage
  // (kBRPPoolSize + kNonBRPPoolSize), one element per super page.
  static constexpr size_t kReservationOffsetTableCoverage =
      PartitionAddressSpace::kTotalSize;
  static constexpr size_t kReservationOffsetTableLength =
      kReservationOffsetTableCoverage >> kSuperPageShift;
#else
  // The size of the reservation offset table should cover the entire 32-bit
  // address space, one element per super page.
  static constexpr uint64_t kGiB = 1024 * 1024 * 1024ull;
  static constexpr size_t kReservationOffsetTableLength =
      4 * kGiB / kSuperPageSize;
#endif
  static_assert(kReservationOffsetTableLength < kOffsetTagNormalBuckets,
                "Offsets should be smaller than kOffsetTagNormalBuckets.");

  static struct _ReservationOffsetTable {
    // Thenumber of table elements is less than MAX_UINT16, so the element type
    // can be uint16_t.
    uint16_t offsets[kReservationOffsetTableLength] = {};

    constexpr _ReservationOffsetTable() {
      for (uint16_t& offset : offsets)
        offset = kOffsetTagNotAllocated;
    }
  } reservation_offset_table_;
};

ALWAYS_INLINE uint16_t* GetReservationOffsetTable() {
  return ReservationOffsetTable::reservation_offset_table_.offsets;
}

ALWAYS_INLINE const uint16_t* GetReservationOffsetTableEnd() {
  return ReservationOffsetTable::reservation_offset_table_.offsets +
         ReservationOffsetTable::kReservationOffsetTableLength;
}

ALWAYS_INLINE uint16_t* ReservationOffsetPointer(uintptr_t address) {
#if defined(PA_HAS_64_BITS_POINTERS)
  // In 64-bit mode, use the offset from the beginning of GigaCage, since
  // that's what the reservation offset table covers.
  size_t table_index =
      PartitionAddressSpace::GigaCageOffset(address) >> kSuperPageShift;
#else
  size_t table_index = address >> kSuperPageShift;
#endif
  PA_DCHECK(table_index <
            ReservationOffsetTable::kReservationOffsetTableLength);
  return GetReservationOffsetTable() + table_index;
}

// If the given address doesn't point to direct-map allocated memory,
// returns 0.
ALWAYS_INLINE uintptr_t GetDirectMapReservationStart(void* address) {
#if DCHECK_IS_ON()
  bool is_in_brp_pool = IsManagedByPartitionAllocBRPPool(address);
  bool is_in_non_brp_pool = IsManagedByPartitionAllocNonBRPPool(address);
#endif
  uintptr_t ptr_as_uintptr = reinterpret_cast<uintptr_t>(address);
  uint16_t* offset_ptr = ReservationOffsetPointer(ptr_as_uintptr);
  PA_DCHECK(*offset_ptr != kOffsetTagNotAllocated);
  if (*offset_ptr == kOffsetTagNormalBuckets)
    return 0;
  uintptr_t reservation_start =
      (ptr_as_uintptr & kSuperPageBaseMask) -
      (static_cast<size_t>(*offset_ptr) << kSuperPageShift);
#if DCHECK_IS_ON()
  // Make sure the reservation start is in the same pool as |address|.
  // In the 32-bit mode, the beginning of a reservation may be excluded from the
  // BRP pool, so shift the pointer. Non-BRP pool doesn't have logic.
  PA_DCHECK(is_in_brp_pool ==
            IsManagedByPartitionAllocBRPPool(reinterpret_cast<void*>(
                reservation_start
#if !defined(PA_HAS_64_BITS_POINTERS)
                + AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap *
                      AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap
#endif  // !defined(PA_HAS_64_BITS_POINTERS)
                )));
  PA_DCHECK(is_in_non_brp_pool ==
            IsManagedByPartitionAllocNonBRPPool(
                reinterpret_cast<void*>(reservation_start)));
  PA_DCHECK(*ReservationOffsetPointer(reservation_start) == 0);
#endif  // DCHECK_IS_ON()

  return reservation_start;
}

// Returns true if |address| is the beginning of the first super page of a
// reservation, i.e. either a normal bucket super page, or the first super page
// of direct map.
// |address| must belong to an allocated super page.
ALWAYS_INLINE bool IsReservationStart(const void* address) {
  uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
  uint16_t* offset_ptr = ReservationOffsetPointer(address_as_uintptr);
  PA_DCHECK(*offset_ptr != kOffsetTagNotAllocated);
  return ((*offset_ptr == kOffsetTagNormalBuckets) || (*offset_ptr == 0)) &&
         (address_as_uintptr % kSuperPageSize == 0);
}

// Returns true if |address| belongs to a normal bucket super page.
ALWAYS_INLINE bool IsManagedByNormalBuckets(const void* address) {
  uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
  uint16_t* offset_ptr = ReservationOffsetPointer(address_as_uintptr);
  return *offset_ptr == kOffsetTagNormalBuckets;
}

// Returns true if |address| belongs to a direct map region.
ALWAYS_INLINE bool IsManagedByDirectMap(const void* address) {
  uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
  uint16_t* offset_ptr = ReservationOffsetPointer(address_as_uintptr);
  return *offset_ptr != kOffsetTagNormalBuckets &&
         *offset_ptr != kOffsetTagNotAllocated;
}

// Returns true if |address| belongs to a normal bucket super page or a direct
// map region, i.e. belongs to an allocated super page.
ALWAYS_INLINE bool IsManagedByNormalBucketsOrDirectMap(const void* address) {
  uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
  uint16_t* offset_ptr = ReservationOffsetPointer(address_as_uintptr);
  return *offset_ptr != kOffsetTagNotAllocated;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_RESERVATION_OFFSET_TABLE_H_
