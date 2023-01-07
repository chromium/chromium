// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_

// This file defines types and functions for `MTECheckedPtr<T>` (cf.
// `tagging.h`, which deals with real ARM MTE).

#include <string.h>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_tag_bitmap.h"
#include "base/allocator/partition_allocator/partition_tag_types.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "build/build_config.h"

namespace partition_alloc {

#if defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)

static_assert(
    sizeof(PartitionTag) == internal::tag_bitmap::kPartitionTagSize,
    "sizeof(PartitionTag) must be equal to bitmap::kPartitionTagSize.");

PA_ALWAYS_INLINE PartitionTag* NormalBucketPartitionTagPointer(uintptr_t addr) {
  uintptr_t bitmap_base =
      internal::SuperPageTagBitmapAddr(addr & internal::kSuperPageBaseMask);
  const size_t bitmap_end_offset =
      internal::PartitionPageSize() + internal::ReservedTagBitmapSize();
  PA_DCHECK((addr & internal::kSuperPageOffsetMask) >= bitmap_end_offset);
  uintptr_t offset_in_super_page =
      (addr & internal::kSuperPageOffsetMask) - bitmap_end_offset;
  size_t offset_in_bitmap = offset_in_super_page >>
                            internal::tag_bitmap::kBytesPerPartitionTagShift
                                << internal::tag_bitmap::kPartitionTagSizeShift;
  // No need to tag, as the tag bitmap region isn't protected by MTE.
  return reinterpret_cast<PartitionTag*>(bitmap_base + offset_in_bitmap);
}

PA_ALWAYS_INLINE PartitionTag* DirectMapPartitionTagPointer(uintptr_t addr) {
  uintptr_t first_super_page = internal::GetDirectMapReservationStart(addr);
  PA_DCHECK(first_super_page) << "not managed by a direct map: " << addr;
  auto* subsequent_page_metadata = GetSubsequentPageMetadata(
      internal::PartitionSuperPageToMetadataArea<internal::ThreadSafe>(
          first_super_page));
  return &subsequent_page_metadata->direct_map_tag;
}

PA_ALWAYS_INLINE PartitionTag* PartitionTagPointer(uintptr_t addr) {
  // UNLIKELY because direct maps are far less common than normal buckets.
  if (PA_UNLIKELY(internal::IsManagedByDirectMap(addr))) {
    return DirectMapPartitionTagPointer(addr);
  }
  return NormalBucketPartitionTagPointer(addr);
}

PA_ALWAYS_INLINE PartitionTag* PartitionTagPointer(const void* ptr) {
  // Disambiguation: UntagPtr relates to hwardware MTE, and it strips the tag
  // from the pointer. Whereas, PartitionTagPointer relates to software MTE
  // (i.e. MTECheckedPtr) and it returns a pointer to the tag in memory.
  return PartitionTagPointer(UntagPtr(ptr));
}

namespace internal {

PA_ALWAYS_INLINE void DirectMapPartitionTagSetValue(uintptr_t addr,
                                                    PartitionTag value) {
  *DirectMapPartitionTagPointer(addr) = value;
}

PA_ALWAYS_INLINE void NormalBucketPartitionTagSetValue(uintptr_t slot_start,
                                                       size_t size,
                                                       PartitionTag value) {
  PA_DCHECK((size % tag_bitmap::kBytesPerPartitionTag) == 0);
  PA_DCHECK((slot_start % tag_bitmap::kBytesPerPartitionTag) == 0);
  size_t tag_count = size >> tag_bitmap::kBytesPerPartitionTagShift;
  PartitionTag* tag_ptr = NormalBucketPartitionTagPointer(slot_start);
  if (sizeof(PartitionTag) == 1) {
    memset(tag_ptr, value, tag_count);
  } else {
    while (tag_count-- > 0)
      *tag_ptr++ = value;
  }
}

PA_ALWAYS_INLINE PartitionTag PartitionTagGetValue(void* ptr) {
  return *PartitionTagPointer(ptr);
}

PA_ALWAYS_INLINE void PartitionTagIncrementValue(uintptr_t slot_start,
                                                 size_t size) {
  PartitionTag tag = *PartitionTagPointer(slot_start);
  PartitionTag new_tag = tag;
  ++new_tag;
  new_tag += !new_tag;  // Avoid 0.
#if BUILDFLAG(PA_DCHECK_IS_ON)
  PA_DCHECK(internal::IsManagedByNormalBuckets(slot_start));
  // This verifies that tags for the entire slot have the same value and that
  // |size| doesn't exceed the slot size.
  size_t tag_count = size >> tag_bitmap::kBytesPerPartitionTagShift;
  PartitionTag* tag_ptr = PartitionTagPointer(slot_start);
  while (tag_count-- > 0) {
    PA_DCHECK(tag == *tag_ptr);
    tag_ptr++;
  }
#endif
  NormalBucketPartitionTagSetValue(slot_start, size, new_tag);
}

}  // namespace internal

#else  // No-op versions

PA_ALWAYS_INLINE PartitionTag* PartitionTagPointer(void* ptr) {
  PA_NOTREACHED();
  return nullptr;
}

namespace internal {

PA_ALWAYS_INLINE PartitionTag PartitionTagGetValue(void*) {
  return 0;
}

PA_ALWAYS_INLINE void PartitionTagIncrementValue(uintptr_t slot_start,
                                                 size_t size) {}

}  // namespace internal

#endif  // defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_
