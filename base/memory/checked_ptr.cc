// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/checked_ptr.h"

#include "base/check.h"
#include "base/partition_alloc_buildflags.h"

// USE_BACKUP_REF_PTR implies USE_PARTITION_ALLOC, needed for code under
// allocator/partition_allocator/ to be built.
#if BUILDFLAG(USE_BACKUP_REF_PTR)

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"

namespace base {

namespace internal {

void BackupRefPtrImpl::AcquireInternal(void* ptr) {
  DCHECK(features::IsPartitionAllocGigaCageEnabled() &&
         IsManagedByPartitionAllocBRPPool(ptr));
  void* slot_start = PartitionAllocGetSlotStart(ptr);
  PartitionRefCountPointer(slot_start)->Acquire();
}

void BackupRefPtrImpl::ReleaseInternal(void* ptr) {
  DCHECK(features::IsPartitionAllocGigaCageEnabled() &&
         IsManagedByPartitionAllocBRPPool(ptr));
  void* slot_start = PartitionAllocGetSlotStart(ptr);
  if (PartitionRefCountPointer(slot_start)->Release())
    PartitionAllocFreeForRefCounting(slot_start);
}

bool BackupRefPtrImpl::IsPointeeAlive(void* ptr) {
  DCHECK(features::IsPartitionAllocGigaCageEnabled() &&
         IsManagedByPartitionAllocBRPPool(ptr));
  void* slot_start = PartitionAllocGetSlotStart(ptr);
  return PartitionRefCountPointer(slot_start)->IsAlive();
}

bool BackupRefPtrImpl::IsValidDelta(void* ptr, ptrdiff_t delta) {
  DCHECK(features::IsPartitionAllocGigaCageEnabled());
  return PartitionAllocIsValidPtrDelta(ptr, delta);
}

}  // namespace internal

}  // namespace base

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
