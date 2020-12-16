// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/checked_ptr.h"

#if ENABLE_BACKUP_REF_PTR_IMPL

#include "base/allocator/partition_allocator/partition_alloc.h"

namespace base {

namespace internal {

void BackupRefPtrImpl::AcquireInternal(void* ptr) {
  void* slot_start = PartitionAllocGetSlotStart(ptr);
  PartitionRefCountPointer(slot_start)->Acquire();
}

void BackupRefPtrImpl::ReleaseInternal(void* ptr) {
  void* slot_start = PartitionAllocGetSlotStart(ptr);
  if (PartitionRefCountPointer(slot_start)->Release())
    PartitionAllocFreeForRefCounting(slot_start);
}

bool BackupRefPtrImpl::IsPointeeAlive(void* ptr) {
  void* slot_start = PartitionAllocGetSlotStart(ptr);
  return PartitionRefCountPointer(slot_start)->IsAlive();
}

}  // namespace internal

}  // namespace base

#endif  // ENABLE_BACKUP_REF_PTR_IMPL
