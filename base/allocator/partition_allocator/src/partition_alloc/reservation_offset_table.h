// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef PARTITION_ALLOC_RESERVATION_OFFSET_TABLE_H_
#define PARTITION_ALLOC_RESERVATION_OFFSET_TABLE_H_

#include <cstddef>
#include <cstdint>
#include <limits>

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/tagging.h"
#include "partition_alloc/thread_isolation/alignment.h"

namespace partition_alloc::internal {
// The main purpose of the reservation offset table is to easily locate the
// direct map reservation start address for any given address. There is one
// entry in the table for each super page.
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
//    alignment when reserving address spaces. One can use check "is in pool?"
//    to further determine which part of the super page is used by
//    PartitionAlloc. This isn't a problem in 64-bit mode, where allocation
//    granularity is kSuperPageSize.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) ReservationOffsetTable {
  static constexpr uint16_t kOffsetTagNotAllocated =
      std::numeric_limits<uint16_t>::max();
  static constexpr uint16_t kOffsetTagNormalBuckets =
      std::numeric_limits<uint16_t>::max() - 1;

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  // There is one reservation offset table per Pool in 64-bit mode.
  static constexpr size_t kRegularOffsetTableLength =
      PartitionAddressSpace::CorePoolMaxSize() >> kSuperPageShift;
  static_assert(kRegularOffsetTableLength < kOffsetTagNormalBuckets,
                "Offsets should be smaller than kOffsetTagNormalBuckets.");
  static constexpr size_t kBRPOffsetTableLength =
      PartitionAddressSpace::CorePoolMaxSize() >> kSuperPageShift;
  static_assert(kBRPOffsetTableLength < kOffsetTagNormalBuckets,
                "Offsets should be smaller than kOffsetTagNormalBuckets.");
  static constexpr size_t kConfigurableOffsetTableLength =
      PartitionAddressSpace::ConfigurablePoolMaxSize() >> kSuperPageShift;
  static_assert(kConfigurableOffsetTableLength < kOffsetTagNormalBuckets,
                "Offsets should be smaller than kOffsetTagNormalBuckets.");
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
 public:
  static constexpr size_t kThreadIsolatedOffsetTableLength =
      PartitionAddressSpace::ThreadIsolatedPoolSize() >> kSuperPageShift;
  static_assert(kThreadIsolatedOffsetTableLength < kOffsetTagNormalBuckets,
                "Offsets should be smaller than kOffsetTagNormalBuckets.");

 private:
  static constexpr size_t kThreadIsolatedOffsetTablePaddingSize =
      base::bits::AlignUp(kThreadIsolatedOffsetTableLength * sizeof(uint16_t),
                          SystemPageSize()) -
      kThreadIsolatedOffsetTableLength * sizeof(uint16_t);
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
#else
  // The size of the reservation offset table should cover the entire 32-bit
  // address space, one element per super page.
  static constexpr uint64_t kGiB = 1024 * 1024 * 1024ull;
  static constexpr size_t kReservationOffsetTableLength =
      4 * kGiB / kSuperPageSize;
  static_assert(kReservationOffsetTableLength < kOffsetTagNormalBuckets,
                "Offsets should be smaller than kOffsetTagNormalBuckets.");
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

  template <size_t length, size_t padding_size = 0>
  struct _ReservationOffsetTable {
    // The number of table elements is less than MAX_UINT16, so the element type
    // can be uint16_t.
    static_assert(
        length <= std::numeric_limits<uint16_t>::max(),
        "Length of the reservation offset table must be less than MAX_UINT16");
    uint16_t offsets[length] = {};
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-length-array"
#endif
    char pad_[padding_size] = {};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    constexpr _ReservationOffsetTable() {
      for (uint16_t& offset : offsets) {
        offset = kOffsetTagNotAllocated;
      }
    }
  };

  template <size_t length, size_t padding_size>
  PA_ALWAYS_INLINE explicit ReservationOffsetTable(
      _ReservationOffsetTable<length, padding_size>& table
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
      ,
      PoolOffsetLookup offset_lookup
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
      )
      : table_begin_(table.offsets)
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
        ,
        table_end_(table.offsets + length)
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
        ,
        offset_lookup_(offset_lookup)
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  {
  }

 public:
  PA_ALWAYS_INLINE ReservationOffsetTable() = default;

  PA_ALWAYS_INLINE static ReservationOffsetTable Get(pool_handle handle) {
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
        return ReservationOffsetTable(thread_isolated_pool_table_,
                                      offset_lookup);
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
      default:
        PA_NOTREACHED();
    }
#else
    return ReservationOffsetTable(reservation_offset_table_);
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  }

  PA_ALWAYS_INLINE static ReservationOffsetTable Get(uintptr_t address) {
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    return ReservationOffsetTable::Get(
        PartitionAddressSpace::GetPoolInfo(address).handle);
#else
    return ReservationOffsetTable(reservation_offset_table_);
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  }

  PA_ALWAYS_INLINE void* GetData() const { return table_begin_; }

  PA_ALWAYS_INLINE void SetNotAllocatedTag(uintptr_t reservation_start,
                                           size_t reservation_size = 1) const {
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
      *offset_ptr++ = kOffsetTagNotAllocated;
    }
  }

  PA_ALWAYS_INLINE void SetNormalBucketsTag(uintptr_t reservation_start) const {
    PA_DCHECK((reservation_start & kSuperPageOffsetMask) == 0);
    *GetOffsetPointer(reservation_start) = kOffsetTagNormalBuckets;
  }

  PA_ALWAYS_INLINE void SetDirectMapReservationStart(
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
      *offset_ptr++ = offset;
    }
  }

  // If the given address doesn't point to direct-map allocated memory,
  // returns 0.
  PA_ALWAYS_INLINE uintptr_t GetDirectMapReservationStart(uintptr_t address) {
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
  PA_ALWAYS_INLINE bool IsReservationStart(uintptr_t address) const {
    uint16_t* offset_ptr = GetOffsetPointer(address);
    PA_DCHECK(*offset_ptr != kOffsetTagNotAllocated);
    return ((*offset_ptr == kOffsetTagNormalBuckets) || (*offset_ptr == 0)) &&
           (address % kSuperPageSize == 0);
  }

  // Returns true if |address| belongs to a normal bucket super page.
  PA_ALWAYS_INLINE bool IsManagedByNormalBuckets(uintptr_t address) const {
    uint16_t* offset_ptr = GetOffsetPointer(address);
    return *offset_ptr == kOffsetTagNormalBuckets;
  }

  // Returns true if |address| belongs to a direct map region.
  PA_ALWAYS_INLINE bool IsManagedByDirectMap(uintptr_t address) const {
    uint16_t* offset_ptr = GetOffsetPointer(address);
    return *offset_ptr != kOffsetTagNormalBuckets &&
           *offset_ptr != kOffsetTagNotAllocated;
  }

  // Returns true if |address| belongs to a normal bucket super page or a direct
  // map region, i.e. belongs to an allocated super page.
  PA_ALWAYS_INLINE bool IsManagedByNormalBucketsOrDirectMap(
      uintptr_t address) const {
    uint16_t* offset_ptr = GetOffsetPointer(address);
    return *offset_ptr != kOffsetTagNotAllocated;
  }

  PA_ALWAYS_INLINE uint16_t* GetOffsetPointerForTesting(
      uintptr_t address) const {
    return GetOffsetPointer(address);
  }

 private:
  PA_ALWAYS_INLINE uint16_t* GetOffsetPointer(uintptr_t address) const {
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    size_t table_index = offset_lookup_.GetOffset(address) >> kSuperPageShift;
#else
    size_t table_index = address >> kSuperPageShift;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    uint16_t* offset_ptr = &table_begin_[table_index];
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
    PA_DCHECK(offset_ptr < table_end_);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
    return offset_ptr;
  }

  uint16_t* table_begin_ = nullptr;
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  uint16_t* table_end_ = nullptr;
#endif
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  PoolOffsetLookup offset_lookup_;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  PA_CONSTINIT static _ReservationOffsetTable<kRegularOffsetTableLength>
      regular_pool_table_;
  PA_CONSTINIT static _ReservationOffsetTable<kBRPOffsetTableLength>
      brp_pool_table_;
  PA_CONSTINIT static _ReservationOffsetTable<kConfigurableOffsetTableLength>
      configurable_pool_table_;
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // If thread isolation support is enabled, we need to write-protect the tables
  // of the thread isolated pool. For this, the thread isolated ones start on a
  // page boundary.
  PA_THREAD_ISOLATED_ALIGN
  PA_CONSTINIT static _ReservationOffsetTable<
      kThreadIsolatedOffsetTableLength,
      kThreadIsolatedOffsetTablePaddingSize>
      thread_isolated_pool_table_;
#endif
#else
  // A single table for the entire 32-bit address space.
  PA_CONSTINIT static _ReservationOffsetTable<kReservationOffsetTableLength>
      reservation_offset_table_;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
};

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_RESERVATION_OFFSET_TABLE_H_
