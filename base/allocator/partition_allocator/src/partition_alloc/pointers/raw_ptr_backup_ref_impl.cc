// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/slot_start.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

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

template <bool AllowDangling>
void RawPtrBackupRefImpl<AllowDangling>::AcquireInternal(uintptr_t address) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(UseBrp(address));
#endif
  auto [slot_start, slot_size] =
      partition_alloc::PartitionAllocGetSlotStartAndSizeInBRPPool(address);
  if constexpr (AllowDangling) {
    partition_alloc::PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
        partition_alloc::internal::UntaggedSlotStart(slot_start), slot_size)
        ->AcquireFromUnprotectedPtr();
  } else {
    partition_alloc::PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
        partition_alloc::internal::UntaggedSlotStart(slot_start), slot_size)
        ->Acquire();
  }
}

template <bool AllowDangling>
void RawPtrBackupRefImpl<AllowDangling>::ReleaseInternal(uintptr_t address) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(UseBrp(address));
#endif
  auto [slot_start, slot_size] =
      partition_alloc::PartitionAllocGetSlotStartAndSizeInBRPPool(address);
  if constexpr (AllowDangling) {
    if (partition_alloc::PartitionRoot::
            InSlotMetadataPointerFromSlotStartAndSize(
                partition_alloc::internal::UntaggedSlotStart(slot_start),
                slot_size)
                ->ReleaseFromUnprotectedPtr()) {
      partition_alloc::PartitionRoot::FreeAfterBRPQuarantine(
          partition_alloc::internal::UntaggedSlotStart(slot_start), slot_size);
    }
  } else {
    if (partition_alloc::PartitionRoot::
            InSlotMetadataPointerFromSlotStartAndSize(
                partition_alloc::internal::UntaggedSlotStart(slot_start),
                slot_size)
                ->Release()) {
      partition_alloc::PartitionRoot::FreeAfterBRPQuarantine(
          partition_alloc::internal::UntaggedSlotStart(slot_start), slot_size);
    }
  }
}

template <bool AllowDangling>
void RawPtrBackupRefImpl<AllowDangling>::ReportIfDanglingInternal(
    uintptr_t address) {
  if (partition_alloc::internal::IsUnretainedDanglingRawPtrCheckEnabled()) {
    if (IsSupportedAndNotNull(address)) {
      auto [slot_start, slot_size] =
          partition_alloc::PartitionAllocGetSlotStartAndSizeInBRPPool(address);
      partition_alloc::PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
          partition_alloc::internal::UntaggedSlotStart(slot_start), slot_size)
          ->ReportIfDangling();
    }
  }
}

// static
template <bool AllowDangling>
bool RawPtrBackupRefImpl<AllowDangling>::CheckPointerWithinSameAlloc(
    uintptr_t before_addr,
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

template <bool AllowDangling>
bool RawPtrBackupRefImpl<AllowDangling>::IsPointeeAlive(uintptr_t address) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(UseBrp(address));
#endif
  auto [slot_start, slot_size] =
      partition_alloc::PartitionAllocGetSlotStartAndSizeInBRPPool(address);
  return partition_alloc::PartitionRoot::
      InSlotMetadataPointerFromSlotStartAndSize(
             partition_alloc::internal::UntaggedSlotStart(slot_start),
             slot_size)
          ->IsAlive();
}

// Explicitly instantiates the two BackupRefPtr variants in the .cc. This
// ensures the definitions not visible from the .h are available in the binary.
template struct RawPtrBackupRefImpl</*AllowDangling=*/false>;
template struct RawPtrBackupRefImpl</*AllowDangling=*/true>;

#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
void CheckThatAddressIsntWithinFirstPartitionPage(uintptr_t address) {
  auto reservation_offset_table =
      partition_alloc::internal::ReservationOffsetTable::Get(address);
  if (reservation_offset_table.IsManagedByDirectMap(address)) {
    uintptr_t reservation_start =
        reservation_offset_table.GetDirectMapReservationStart(address);
    PA_BASE_CHECK(address - reservation_start >=
                  partition_alloc::PartitionPageSize());
  } else {
    PA_BASE_CHECK(reservation_offset_table.IsManagedByNormalBuckets(address));
    PA_BASE_CHECK(address % partition_alloc::kSuperPageSize >=
                  partition_alloc::PartitionPageSize());
  }
}
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON) ||
        // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)

}  // namespace base::internal
