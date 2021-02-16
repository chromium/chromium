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

// TODO(tasak): Optimize this function. There's potential for optimization in
// all callers of |PartitionRefCountPointer| (apart from marking it
// |ALWAYS_INLINE|):
//   - |AllocFlagsNoHooks| and |FreeNoHooksImmediate| already "know"
//   |usable_size|.
//   - |AcquireInternal| and | ReleaseInternal| know |slot_span| (through the
//   inlined |PartitionAllocGetSlotStart|).
PartitionRefCount* PartitionRefCountPointer(void* slot_start) {
  DCheckGetSlotOffsetIsZero(slot_start);
  // Layout inside the slot of small buckets:
  //  |<---------------- slot size ----------------->
  //  |[cookie]|...data...|[empty]|[cookie]|[refcnt]|
  //  ^                                    ^
  //  |                                    |
  // slot_start                   partition_ref_count_ptr
  //
  // Layout inside the slot of single-slot spans (raw size is available)
  //  |<---------------------- slot size ------------------------>
  //  |[cookie]|...data...|[cookie]|[refcnt_placeholder]|[unused]|
  //
  // refcount is not stored in the slot (even though the space for it is still
  // reserved). Instead, refcount is stored in the subsequent page metadata.

  auto* slot_span = SlotSpanMetadata<ThreadSafe>::FromSlotStartPtr(slot_start);
  PA_DCHECK(slot_span);
#if DCHECK_IS_ON()
  PartitionCookieCheckValue(slot_start);
#endif
  uint8_t* partition_ref_count_ptr;
  if (UNLIKELY(slot_span->CanStoreRawSize())) {
    auto* the_next_page =
        reinterpret_cast<PartitionPage<ThreadSafe>*>(slot_span) + 1;
    partition_ref_count_ptr =
        the_next_page->subsequent_page_metadata.ref_count_buffer;
  } else {
    uint8_t* slot_start_ptr = reinterpret_cast<uint8_t*>(slot_start);
    size_t ref_count_offset =
        slot_span->bucket->slot_size - kInSlotRefCountBufferSize;
    partition_ref_count_ptr = slot_start_ptr + ref_count_offset;
  }
  PA_DCHECK(reinterpret_cast<uintptr_t>(partition_ref_count_ptr) %
                alignof(PartitionRefCount) ==
            0);
  return reinterpret_cast<PartitionRefCount*>(partition_ref_count_ptr);
}

#endif  //  BUILDFLAG(REF_COUNT_AT_END_OF_ALLOCATION)

}  // namespace internal
}  // namespace base
