// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_

#include <bitset>
#include <limits>

#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"

namespace base {

template <typename Type>
struct LazyInstanceTraitsBase;

namespace internal {

// (64bit version)
// AddressPoolManager takes a reserved virtual address space and manages address
// space allocation.
//
// AddressPoolManager (currently) supports up to 2 pools. Each pool manages a
// contiguous reserved address space. Alloc() takes a pool_handle and returns
// address regions from the specified pool. Free() also takes a pool_handle and
// returns the address region back to the manager.
//
// (32bit version)
// AddressPoolManager wraps AllocPages and FreePages and remembers allocated
// address regions using bitmaps. IsManagedByPartitionAllocBRPPool and
// IsManagedByPartitionAllocNonBRPPool use the bitmaps to judge whether a given
// address is in a pool that supports BackupRefPtr or in a pool that doesn't.
// All PartitionAlloc allocations must be in either of the pools.
class BASE_EXPORT AddressPoolManager {
  static constexpr uint64_t kGiB = 1024 * 1024 * 1024ull;

 public:
  static constexpr uint64_t kBRPPoolMaxSize =
#if defined(PA_HAS_64_BITS_POINTERS)
      // TODO(bartekn): Use kBRPPoolSize from partition_address_space.h
      8 * kGiB;
#else
      4 * kGiB;
#endif

  static AddressPoolManager* GetInstance();

#if defined(PA_HAS_64_BITS_POINTERS)
  pool_handle Add(uintptr_t address, size_t length);
  void Remove(pool_handle handle);
#endif
  // Reserves address space from GigaCage.
  char* Reserve(pool_handle handle, void* requested_address, size_t length);
  // Frees address space back to GigaCage and decommits underlying system pages.
  void UnreserveAndDecommit(pool_handle handle, void* ptr, size_t length);
  void ResetForTesting();

#if !defined(PA_HAS_64_BITS_POINTERS)
  static bool IsManagedByNonBRPPool(const void* address) {
    return AddressPoolManagerBitmap::IsManagedByNonBRPPool(address);
  }

  static bool IsManagedByBRPPool(const void* address) {
    return AddressPoolManagerBitmap::IsManagedByBRPPool(address);
  }

  static constexpr uint16_t kNotInDirectMap =
      std::numeric_limits<uint16_t>::max();

  // (For !defined(PA_HAS_64_BITS_POINTERS))
  // reservation_offset_table_ is used to get the reservation start address
  // when an address is given. Each entry of the table is prepared for each
  // super page. When PartitionAlloc reserves an address space (its required
  // alignment is SuperPageSize but its required size is not
  // SuperPageSize-aligned),
  //
  // |<---------- SuperPage-aligned reserved size -------->|
  // |<---------- Reserved size ------------------>|
  // +----------+----------+-----+-----------+-----+ - - - +
  // |SuperPage0|SuperPage1| ... |SuperPage K|SuperPage K+1|
  // +----------+----------+-----+-----------+-----+ - - -.+
  //                                         |<-X->|<--Y-->|
  //
  // the table entries for reserved SuperPages have the numbers of SuperPages
  // between the reservation start and each reserved SuperPage:
  // +----------+----------+-----+-----------+-------------+
  // |Entry for |Entry for | ... |Entry for  |Entry for    |
  // |SuperPage0|SuperPage1|     |SuperPage K|SuperPage K+1|
  // +----------+----------+-----+-----------+-------------+
  // |     0    |    1     | ... |     K     |   K + 1     |
  // +----------+----------+-----+-----------+-------------+
  // 65535 is used as a special number: "not used for direct-map allocation".
  //
  // So when we have an address Z, ((Z >> SuperPageShift) - (the entry for Z))
  // << SuperPageShift is the reservation start when allocating an address space
  // which contains Z.
  //
  // (*) Y is not used by PartitionAlloc until X is unreserved, because
  // PartitionAlloc always uses SuperPageSize alignment when reserving address
  // spaces. So we don't need to keep any offset for Y. GigaCage helps us to see
  // whether the given address space is used by PA or not.
  static uint16_t* ReservationOffsetTable() {
    return reservation_offset_table_;
  }

  static const uint16_t* EndOfReservationOffsetTable() {
    return reservation_offset_table_ + kReservationOffsetTableSize;
  }
#endif  // !defined(PA_HAS_64_BITS_POINTERS)

 private:
  friend class AddressPoolManagerForTesting;

  AddressPoolManager();
  ~AddressPoolManager();

#if defined(PA_HAS_64_BITS_POINTERS)
  class Pool {
   public:
    Pool();
    ~Pool();

    void Initialize(uintptr_t ptr, size_t length);
    bool IsInitialized();
    void Reset();

    uintptr_t FindChunk(size_t size);
    void FreeChunk(uintptr_t address, size_t size);

    bool TryReserveChunk(uintptr_t address, size_t size);

   private:
    // The bitset stores the allocation state of the address pool. 1 bit per
    // super-page: 1 = allocated, 0 = free.
    static constexpr size_t kMaxBits = kBRPPoolMaxSize / kSuperPageSize;

    base::Lock lock_;
    std::bitset<kMaxBits> alloc_bitset_ GUARDED_BY(lock_);
    // An index of a bit in the bitset before which we know for sure there all
    // 1s. This is a best-effort hint in the sense that there still may be lots
    // of 1s after this index, but at least we know there is no point in
    // starting the search before it.
    size_t bit_hint_ GUARDED_BY(lock_);

    size_t total_bits_ = 0;
    uintptr_t address_begin_ = 0;
#if DCHECK_IS_ON()
    uintptr_t address_end_ = 0;
#endif
  };

  ALWAYS_INLINE Pool* GetPool(pool_handle handle) {
    PA_DCHECK(0 < handle && handle <= kNumPools);
    return &pools_[handle - 1];
  }

  static constexpr size_t kNumPools = 2;
  Pool pools_[kNumPools];

#else   // defined(PA_HAS_64_BITS_POINTERS)

  void MarkUsed(pool_handle handle, const char* address, size_t size);
  void MarkUnused(pool_handle handle, uintptr_t address, size_t size);

  // BRP stands for BackupRefPtr. GigaCage is split into pools, one which
  // supports BackupRefPtr and one that doesn't.
  static constexpr pool_handle kNonBRPPoolHandle = 1;
  static constexpr pool_handle kBRPPoolHandle = 2;
  friend pool_handle GetNonBRPPool();
  friend pool_handle GetBRPPool();

  // Allocation size is a multiple of DirectMapAllocationGranularity(), but
  // alignment is kSuperPageSize.
  static constexpr size_t kReservationOffsetTableSize =
      4 * kGiB / kSuperPageSize;

  static_assert(kReservationOffsetTableSize <
                    std::numeric_limits<uint16_t>::max(),
                "kReservationOffsetTableSize should be smaller than 65536.");

  static uint16_t reservation_offset_table_[kReservationOffsetTableSize];

#endif  // defined(PA_HAS_64_BITS_POINTERS)

  friend struct base::LazyInstanceTraitsBase<AddressPoolManager>;
  DISALLOW_COPY_AND_ASSIGN(AddressPoolManager);
};

#if !defined(PA_HAS_64_BITS_POINTERS)
ALWAYS_INLINE pool_handle GetNonBRPPool() {
  return AddressPoolManager::kNonBRPPoolHandle;
}

ALWAYS_INLINE pool_handle GetBRPPool() {
  return AddressPoolManager::kBRPPoolHandle;
}

ALWAYS_INLINE constexpr uint16_t NotInDirectMapOffsetTag() {
  return AddressPoolManager::kNotInDirectMap;
}

ALWAYS_INLINE uint16_t* ReservationOffsetPointer(uintptr_t address) {
  unsigned table_offset = address >> kSuperPageShift;
  return AddressPoolManager::ReservationOffsetTable() + table_offset;
}

ALWAYS_INLINE const uint16_t* EndOfReservationOffsetTable() {
  return AddressPoolManager::EndOfReservationOffsetTable();
}

// If the given address doesn't point to direct-map allocated memory,
// returns 0.
ALWAYS_INLINE uintptr_t GetDirectMapReservationStart(void* address) {
#if DCHECK_IS_ON()
  bool is_in_brp_pool = AddressPoolManager::IsManagedByBRPPool(address);
  bool is_in_non_brp_pool = AddressPoolManager::IsManagedByNonBRPPool(address);
#endif
  uintptr_t ptr_as_uintptr = reinterpret_cast<uintptr_t>(address);
  uint16_t* offset_ptr = ReservationOffsetPointer(ptr_as_uintptr);
  if (*offset_ptr == NotInDirectMapOffsetTag())
    return 0;
  uintptr_t reservation_start =
      (ptr_as_uintptr & kSuperPageBaseMask) -
      (static_cast<size_t>(*offset_ptr) << kSuperPageShift);
#if DCHECK_IS_ON()

  // Make sure the reservation start is in the same pool as |address|.
  // The beginning of a reservation may be excluded from the BRP pool, so shift
  // the pointer. Non-BRP pool doesn't have logic.
  PA_DCHECK(is_in_brp_pool ==
            AddressPoolManager::IsManagedByBRPPool(reinterpret_cast<void*>(
                reservation_start +
                AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap *
                    AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap)));
  PA_DCHECK(is_in_non_brp_pool ==
            AddressPoolManager::IsManagedByNonBRPPool(
                reinterpret_cast<void*>(reservation_start)));
  PA_DCHECK(*ReservationOffsetPointer(reservation_start) == 0);
#endif  // DCHECK_IS_ON()

  return reservation_start;
}

// Returns true if |address| is the beginning of the first super page of a
// reservation, i.e. either a normal bucket super page, or the first super page
// of direct map.
ALWAYS_INLINE bool IsReservationStart(const void* address) {
  uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
  uint16_t* offset_ptr = ReservationOffsetPointer(address_as_uintptr);
  return ((*offset_ptr == NotInDirectMapOffsetTag()) || (*offset_ptr == 0)) &&
         (address_as_uintptr % kSuperPageSize == 0);
}

// Returns true if |address| belongs to a normal bucket super page.
ALWAYS_INLINE bool IsManagedByNormalBuckets(const void* address) {
  uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
  uint16_t* offset_ptr = ReservationOffsetPointer(address_as_uintptr);
  return *offset_ptr == NotInDirectMapOffsetTag();
}

// Returns true if |address| belongs to a direct map region.
ALWAYS_INLINE bool IsManagedByDirectMap(const void* address) {
  uintptr_t address_as_uintptr = reinterpret_cast<uintptr_t>(address);
  uint16_t* offset_ptr = ReservationOffsetPointer(address_as_uintptr);
  return *offset_ptr != NotInDirectMapOffsetTag();
}

#endif  // !defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_H_
