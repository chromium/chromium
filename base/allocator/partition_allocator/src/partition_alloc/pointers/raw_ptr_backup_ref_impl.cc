// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/pointers/raw_ptr_backup_ref_impl.h"

#include <cstdint>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/dangling_raw_ptr_checks.h"
#include "partition_alloc/in_slot_metadata.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/reservation_offset_table.h"

namespace base::internal {

template <bool AllowDangling, bool DisableBRP>
void RawPtrBackupRefImpl<AllowDangling, DisableBRP>::AcquireInternal(
    uintptr_t address) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(UseBrp(address));
#endif
  auto [slot_start, slot_size] =
      partition_alloc::PartitionAllocGetSlotStartAndSizeInBRPPool(address);
  if constexpr (AllowDangling) {
    partition_alloc::PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
        slot_start, slot_size)
        ->AcquireFromUnprotectedPtr();
  } else {
    partition_alloc::PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
        slot_start, slot_size)
        ->Acquire();
  }
}

template <bool AllowDangling, bool DisableBRP>
void RawPtrBackupRefImpl<AllowDangling, DisableBRP>::ReleaseInternal(
    uintptr_t address) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(UseBrp(address));
#endif
  auto [slot_start, slot_size] =
      partition_alloc::PartitionAllocGetSlotStartAndSizeInBRPPool(address);
  if constexpr (AllowDangling) {
    if (partition_alloc::PartitionRoot::
            InSlotMetadataPointerFromSlotStartAndSize(slot_start, slot_size)
                ->ReleaseFromUnprotectedPtr()) {
      partition_alloc::internal::PartitionAllocFreeForRefCounting(slot_start);
    }
  } else {
    if (partition_alloc::PartitionRoot::
            InSlotMetadataPointerFromSlotStartAndSize(slot_start, slot_size)
                ->Release()) {
      partition_alloc::internal::PartitionAllocFreeForRefCounting(slot_start);
    }
  }
}

template <bool AllowDangling, bool DisableBRP>
void RawPtrBackupRefImpl<AllowDangling, DisableBRP>::ReportIfDanglingInternal(
    uintptr_t address) {
  if (partition_alloc::internal::IsUnretainedDanglingRawPtrCheckEnabled()) {
    if (IsSupportedAndNotNull(address)) {
      auto [slot_start, slot_size] =
          partition_alloc::PartitionAllocGetSlotStartAndSizeInBRPPool(address);
      partition_alloc::PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
          slot_start, slot_size)
          ->ReportIfDangling();
    }
  }
}

// static
template <bool AllowDangling, bool DisableBRP>
bool RawPtrBackupRefImpl<AllowDangling, DisableBRP>::
    CheckPointerWithinSameAlloc(uintptr_t before_addr,
                                uintptr_t after_addr,
                                size_t type_size) {
  partition_alloc::internal::PtrPosWithinAlloc ptr_pos_within_alloc =
      partition_alloc::internal::IsPtrWithinSameAlloc(before_addr, after_addr,
                                                      type_size);
  // No need to check that |new_ptr| is in the same pool, as
  // IsPtrWithinSameAlloc() checks that it's within the same allocation, so
  // must be the same pool.
  PA_BASE_CHECK(ptr_pos_within_alloc !=
                partition_alloc::internal::PtrPosWithinAlloc::kFarOOB);

#if PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
  return ptr_pos_within_alloc ==
         partition_alloc::internal::PtrPosWithinAlloc::kAllocEnd;
#else
  return false;
#endif
}

template <bool AllowDangling, bool DisableBRP>
bool RawPtrBackupRefImpl<AllowDangling, DisableBRP>::IsPointeeAlive(
    uintptr_t address) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(UseBrp(address));
#endif
  auto [slot_start, slot_size] =
      partition_alloc::PartitionAllocGetSlotStartAndSizeInBRPPool(address);
  return partition_alloc::PartitionRoot::
      InSlotMetadataPointerFromSlotStartAndSize(slot_start, slot_size)
          ->IsAlive();
}

// Explicitly instantiates the two BackupRefPtr variants in the .cc. This
// ensures the definitions not visible from the .h are available in the binary.
template struct RawPtrBackupRefImpl</*AllowDangling=*/false,
                                    /*DisableBRP=*/false>;
template struct RawPtrBackupRefImpl</*AllowDangling=*/false,
                                    /*DisableBRP=*/true>;
template struct RawPtrBackupRefImpl</*AllowDangling=*/true,
                                    /*DisableBRP=*/false>;
template struct RawPtrBackupRefImpl</*AllowDangling=*/true,
                                    /*DisableBRP=*/true>;

#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
void CheckThatAddressIsntWithinFirstPartitionPage(uintptr_t address) {
  if (partition_alloc::internal::IsManagedByDirectMap(address)) {
    uintptr_t reservation_start =
        partition_alloc::internal::GetDirectMapReservationStart(address);
    PA_BASE_CHECK(address - reservation_start >=
                  partition_alloc::PartitionPageSize());
  } else {
    PA_BASE_CHECK(partition_alloc::internal::IsManagedByNormalBuckets(address));
    PA_BASE_CHECK(address % partition_alloc::kSuperPageSize >=
                  partition_alloc::PartitionPageSize());
  }
}
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON) ||
        // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)

}  // namespace base::internal
