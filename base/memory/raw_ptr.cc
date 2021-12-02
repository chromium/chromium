// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#include "base/allocator/buildflags.h"

// USE_BACKUP_REF_PTR implies USE_PARTITION_ALLOC, needed for code under
// allocator/partition_allocator/ to be built.
#if BUILDFLAG(USE_BACKUP_REF_PTR)

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/check.h"
#include "base/dcheck_is_on.h"

namespace base {

namespace internal {

void BackupRefPtrImpl::AcquireInternal(uintptr_t address) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  CHECK(IsManagedByPartitionAllocBRPPool(address));
#endif
  void* slot_start = PartitionAllocGetSlotStartInBRPPool(address);
  PartitionRefCountPointer(slot_start)->Acquire();
}

void BackupRefPtrImpl::ReleaseInternal(uintptr_t address) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  CHECK(IsManagedByPartitionAllocBRPPool(address));
#endif
  void* slot_start = PartitionAllocGetSlotStartInBRPPool(address);
  if (PartitionRefCountPointer(slot_start)->Release())
    PartitionAllocFreeForRefCounting(slot_start);
}

bool BackupRefPtrImpl::IsPointeeAlive(uintptr_t address) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  CHECK(IsManagedByPartitionAllocBRPPool(address));
#endif
  void* slot_start = PartitionAllocGetSlotStartInBRPPool(address);
  return PartitionRefCountPointer(slot_start)->IsAlive();
}

bool BackupRefPtrImpl::IsValidDelta(uintptr_t address,
                                    ptrdiff_t delta_in_bytes) {
  return PartitionAllocIsValidPtrDelta(address, delta_in_bytes);
}

#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
void CheckThatAddressIsntWithinFirstPartitionPage(uintptr_t address) {
  if (IsManagedByDirectMap(address)) {
    uintptr_t reservation_start = GetDirectMapReservationStart(address);
    CHECK(address - reservation_start >= PartitionPageSize());
  } else {
    CHECK(IsManagedByNormalBuckets(address));
    CHECK(address % kSuperPageSize >= PartitionPageSize());
  }
}
#endif  // DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)

}  // namespace internal

}  // namespace base

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
