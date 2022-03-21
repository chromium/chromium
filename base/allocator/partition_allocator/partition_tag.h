// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_

// This file defines types and functions for `MTECheckedPtr<T>` (cf.
// `tagging.h`, which deals with real ARM MTE).

#include <string.h>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_tag_bitmap.h"
#include "base/base_export.h"
#include "build/build_config.h"

namespace partition_alloc::internal {

#if defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)

// Use 8 bits for the partition tag.
// TODO(tasak): add a description about the partition tag.
using PartitionTag = uint8_t;

static_assert(
    sizeof(PartitionTag) == tag_bitmap::kPartitionTagSize,
    "sizeof(PartitionTag) must be equal to bitmap::kPartitionTagSize.");

static constexpr size_t kInSlotTagBufferSize = 0;

ALWAYS_INLINE PartitionTag* PartitionTagPointer(void* ptr) {
  // See the comment explaining the layout in partition_tag_bitmap.h.
  uintptr_t pointer_as_uintptr = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t bitmap_base =
      (pointer_as_uintptr & kSuperPageBaseMask) + PartitionPageSize();
  uintptr_t offset =
      (pointer_as_uintptr & kSuperPageOffsetMask) - PartitionPageSize();
  // Not to depend on partition_address_space.h and PartitionAllocGigaCage
  // feature, use "offset" to see whether the given ptr is_direct_mapped or not.
  // DirectMap object should cause this PA_DCHECK's failure, as tags aren't
  // currently supported there.
  PA_DCHECK(offset >= ReservedTagBitmapSize());
  size_t bitmap_offset = (offset - ReservedTagBitmapSize()) >>
                         tag_bitmap::kBytesPerPartitionTagShift
                             << tag_bitmap::kPartitionTagSizeShift;
  return reinterpret_cast<PartitionTag* const>(bitmap_base + bitmap_offset);
}

ALWAYS_INLINE void PartitionTagSetValue(void* ptr,
                                        size_t size,
                                        PartitionTag value) {
  PA_DCHECK((size % tag_bitmap::kBytesPerPartitionTag) == 0);
  size_t tag_count = size >> tag_bitmap::kBytesPerPartitionTagShift;
  PartitionTag* tag_ptr = PartitionTagPointer(ptr);
  if (sizeof(PartitionTag) == 1) {
    memset(tag_ptr, value, tag_count);
  } else {
    while (tag_count-- > 0)
      *tag_ptr++ = value;
  }
}

ALWAYS_INLINE PartitionTag PartitionTagGetValue(void* ptr) {
  return *PartitionTagPointer(ptr);
}

ALWAYS_INLINE void PartitionTagClearValue(void* ptr, size_t size) {
  size_t tag_region_size = size >> tag_bitmap::kBytesPerPartitionTagShift
                                       << tag_bitmap::kPartitionTagSizeShift;
  PA_DCHECK(!memchr(PartitionTagPointer(ptr), 0, tag_region_size));
  memset(PartitionTagPointer(ptr), 0, tag_region_size);
}

ALWAYS_INLINE void PartitionTagIncrementValue(void* ptr, size_t size) {
  PartitionTag tag = PartitionTagGetValue(ptr);
  PartitionTag new_tag = tag;
  ++new_tag;
  new_tag += !new_tag;  // Avoid 0.
#if DCHECK_IS_ON()
  // This verifies that tags for the entire slot have the same value and that
  // |size| doesn't exceed the slot size.
  size_t tag_count = size >> tag_bitmap::kBytesPerPartitionTagShift;
  PartitionTag* tag_ptr = PartitionTagPointer(ptr);
  while (tag_count-- > 0) {
    PA_DCHECK(tag == *tag_ptr);
    tag_ptr++;
  }
#endif
  PartitionTagSetValue(ptr, size, new_tag);
}

#else  // No-op versions

using PartitionTag = uint8_t;

static constexpr size_t kInSlotTagBufferSize = 0;

ALWAYS_INLINE PartitionTag* PartitionTagPointer(void* ptr) {
  PA_NOTREACHED();
  return nullptr;
}

ALWAYS_INLINE void PartitionTagSetValue(void*, size_t, PartitionTag) {}

ALWAYS_INLINE PartitionTag PartitionTagGetValue(void*) {
  return 0;
}

ALWAYS_INLINE void PartitionTagClearValue(void* ptr, size_t) {}

ALWAYS_INLINE void PartitionTagIncrementValue(void* ptr, size_t size) {}

#endif  // defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)

constexpr size_t kPartitionTagSizeAdjustment = kInSlotTagBufferSize;
constexpr size_t kPartitionTagOffsetAdjustment = kInSlotTagBufferSize;

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_
