// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_ref_count.h"

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/partition_alloc_buildflags.h"

namespace base {

namespace internal {

#if BUILDFLAG(REF_COUNT_AT_END_OF_ALLOCATION)

PartitionRefCount::PartitionRefCount(PartitionRefCount&& other) {
  // TODO(tasak): This code will cause race conditoin, because raw size and
  // reference count is not in one atomic operation. To avoid this, since only
  // |CanStoreRawSize()| needs this relocation, store the reference count along
  // with the raw size in |SubsequentPageMetadata|. So we don't need the
  // relocation.
  count_.store(other.count_.load(std::memory_order_acquire),
               std::memory_order_release);
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  brp_cookie_ = CalculateCookie();
#endif
}

// TODO(tasak): Optimize this function. There's potential for optimization in
// all callers of |PartitionRefCountPointer| (apart from marking it
// |ALWAYS_INLINE|):
//   - |AllocFlagsNoHooks| and |FreeNoHooksImmediate| already "know"
//   |usable_size|.
//   - |AcquireInternal| and | ReleaseInternal| know |slot_span| (through the
//   inlined |PartitionAllocGetSlotStart|).
PartitionRefCount* PartitionRefCountPointer(void* slot_start) {
  DCheckGetSlotOffsetIsZero(slot_start);
  //  |<---------------------- (b) ----------------------------->
  //  |<------------------- (a) --------------------->
  //  | cookie | data | cookie | [align] | ref count | [unused] |
  //  ^                                  ^
  //  |                                  |
  // slot_start               partition_ref_count_ptr
  //
  // (a): slot_span->GetUtilizedSlotSize()
  // (b): slot_span->bucket->slot_size
  auto* slot_span = SlotSpanMetadata<ThreadSafe>::FromSlotStartPtr(slot_start);
  PA_DCHECK(slot_span);
#if DCHECK_IS_ON()
  PartitionCookieCheckValue(slot_start);
#endif
  char* slot_start_ptr = reinterpret_cast<char*>(slot_start);
  size_t ref_count_offset =
      slot_span->GetUtilizedSlotSize() - kInSlotRefCountBufferSize;
  char* partition_ref_count_ptr = slot_start_ptr + ref_count_offset;
  PA_DCHECK(reinterpret_cast<uintptr_t>(partition_ref_count_ptr) %
                alignof(PartitionRefCount) ==
            0);
  PA_DCHECK(partition_ref_count_ptr + kInSlotRefCountBufferSize <=
            slot_start_ptr + slot_span->bucket->slot_size);
  return reinterpret_cast<PartitionRefCount*>(partition_ref_count_ptr);
}

#endif  //  BUILDFLAG(REF_COUNT_AT_END_OF_ALLOCATION)

}  // namespace internal
}  // namespace base
