// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/pointers/raw_ptr_backup_ref_impl.h"

#include <cstdint>

#include "base/allocator/partition_allocator/dangling_raw_ptr_checks.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_base/check.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"

namespace base::internal {

template <bool AllowDangling>
void RawPtrBackupRefImpl<AllowDangling>::AcquireInternal(uintptr_t address) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(partition_alloc::IsManagedByPartitionAllocBRPPool(address));
#endif
  uintptr_t slot_start =
      partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
  if constexpr (AllowDangling) {
    partition_alloc::internal::PartitionRefCountPointer(slot_start)
        ->AcquireFromUnprotectedPtr();
  } else {
    partition_alloc::internal::PartitionRefCountPointer(slot_start)->Acquire();
  }
}

template <bool AllowDangling>
void RawPtrBackupRefImpl<AllowDangling>::ReleaseInternal(uintptr_t address) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(partition_alloc::IsManagedByPartitionAllocBRPPool(address));
#endif
  uintptr_t slot_start =
      partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
  if constexpr (AllowDangling) {
    if (partition_alloc::internal::PartitionRefCountPointer(slot_start)
            ->ReleaseFromUnprotectedPtr()) {
      partition_alloc::internal::PartitionAllocFreeForRefCounting(slot_start);
    }
  } else {
    if (partition_alloc::internal::PartitionRefCountPointer(slot_start)
            ->Release()) {
      partition_alloc::internal::PartitionAllocFreeForRefCounting(slot_start);
    }
  }
}

template <bool AllowDangling>
void RawPtrBackupRefImpl<AllowDangling>::ReportIfDanglingInternal(
    uintptr_t address) {
  if (partition_alloc::internal::IsUnretainedDanglingRawPtrCheckEnabled()) {
    if (IsSupportedAndNotNull(address)) {
      uintptr_t slot_start =
          partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
      partition_alloc::internal::PartitionRefCountPointer(slot_start)
          ->ReportIfDangling();
    }
  }
}

template <bool AllowDangling>
bool RawPtrBackupRefImpl<AllowDangling>::IsPointeeAlive(uintptr_t address) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  PA_BASE_CHECK(partition_alloc::IsManagedByPartitionAllocBRPPool(address));
#endif
  uintptr_t slot_start =
      partition_alloc::PartitionAllocGetSlotStartInBRPPool(address);
  return partition_alloc::internal::PartitionRefCountPointer(slot_start)
      ->IsAlive();
}

template <bool AllowDangling>
template <typename Z>
partition_alloc::PtrPosWithinAlloc
RawPtrBackupRefImpl<AllowDangling>::IsValidDelta(
    uintptr_t address,
    partition_alloc::internal::PtrDelta<Z> delta) {
  return partition_alloc::internal::PartitionAllocIsValidPtrDelta(address,
                                                                  delta);
}

// Explicitly instantiates the two BackupRefPtr variants in the .cc. This
// ensures the definitions not visible from the .h are available in the binary.
template struct RawPtrBackupRefImpl</*AllowDangling=*/false>;
template struct RawPtrBackupRefImpl</*AllowDangling=*/true>;

template PA_COMPONENT_EXPORT(RAW_PTR)
    partition_alloc::PtrPosWithinAlloc RawPtrBackupRefImpl<false>::IsValidDelta(
        uintptr_t,
        partition_alloc::internal::PtrDelta<size_t>);
template PA_COMPONENT_EXPORT(RAW_PTR)
    partition_alloc::PtrPosWithinAlloc RawPtrBackupRefImpl<false>::IsValidDelta(
        uintptr_t,
        partition_alloc::internal::PtrDelta<ptrdiff_t>);
template PA_COMPONENT_EXPORT(RAW_PTR)
    partition_alloc::PtrPosWithinAlloc RawPtrBackupRefImpl<true>::IsValidDelta(
        uintptr_t,
        partition_alloc::internal::PtrDelta<size_t>);
template PA_COMPONENT_EXPORT(RAW_PTR)
    partition_alloc::PtrPosWithinAlloc RawPtrBackupRefImpl<true>::IsValidDelta(
        uintptr_t,
        partition_alloc::internal::PtrDelta<ptrdiff_t>);

#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
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
#endif  // BUILDFLAG(PA_DCHECK_IS_ON) ||
        // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)

}  // namespace base::internal
