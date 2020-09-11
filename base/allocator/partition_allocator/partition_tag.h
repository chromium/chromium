// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_

#include <string.h>

#include "base/allocator/partition_allocator/checked_ptr_support.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_tag_bitmap.h"
#include "base/base_export.h"
#include "base/notreached.h"
#include "build/build_config.h"

namespace base {

namespace internal {

#if ENABLE_TAG_FOR_CHECKED_PTR2

// Use 16 bits for the partition tag.
// TODO(tasak): add a description about the partition tag.
using PartitionTag = uint8_t;

// Allocate extra space for the partition tag to satisfy the alignment
// requirement.
static constexpr size_t kInSlotTagBufferSize = base::kAlignment;
static_assert(sizeof(PartitionTag) <= kInSlotTagBufferSize,
              "PartitionTag should fit into the in-slot buffer.");

#if DCHECK_IS_ON()
// The layout inside the slot is |tag|cookie|object|(empty)|cookie|.
static constexpr size_t kPartitionTagOffset =
    kInSlotTagBufferSize + kCookieSize;
#else
// The layout inside the slot is |tag|object|(empty)|.
static constexpr size_t kPartitionTagOffset = kInSlotTagBufferSize;
#endif

ALWAYS_INLINE size_t PartitionTagSizeAdjustAdd(size_t size) {
  PA_DCHECK(size + kInSlotTagBufferSize > size);
  return size + kInSlotTagBufferSize;
}

ALWAYS_INLINE size_t PartitionTagSizeAdjustSubtract(size_t size) {
  PA_DCHECK(size >= kInSlotTagBufferSize);
  return size - kInSlotTagBufferSize;
}

ALWAYS_INLINE PartitionTag* PartitionTagPointer(void* ptr) {
  return reinterpret_cast<PartitionTag*>(reinterpret_cast<char*>(ptr) -
                                         kPartitionTagOffset);
}

ALWAYS_INLINE void* PartitionTagPointerAdjustSubtract(void* ptr) {
  return reinterpret_cast<void*>(reinterpret_cast<char*>(ptr) -
                                 kInSlotTagBufferSize);
}

ALWAYS_INLINE void* PartitionTagPointerAdjustAdd(void* ptr) {
  return reinterpret_cast<void*>(reinterpret_cast<char*>(ptr) +
                                 kInSlotTagBufferSize);
}

ALWAYS_INLINE void PartitionTagSetValue(void* ptr, size_t, PartitionTag value) {
  *PartitionTagPointer(ptr) = value;
}

ALWAYS_INLINE PartitionTag PartitionTagGetValue(void* ptr) {
  return *PartitionTagPointer(ptr);
}

ALWAYS_INLINE void PartitionTagClearValue(void* ptr, size_t) {
  PA_DCHECK(PartitionTagGetValue(ptr));
  *PartitionTagPointer(ptr) = 0;
}

#elif ENABLE_TAG_FOR_MTE_CHECKED_PTR

// Use 8 bits for the partition tag.
// TODO(tasak): add a description about the partition tag.
using PartitionTag = uint8_t;

static_assert(
    sizeof(PartitionTag) == tag_bitmap::kPartitionTagSize,
    "sizeof(PartitionTag) must be equal to bitmap::kPartitionTagSize.");

static constexpr size_t kInSlotTagBufferSize = 0;

ALWAYS_INLINE size_t PartitionTagSizeAdjustAdd(size_t size) {
  return size;
}

ALWAYS_INLINE size_t PartitionTagSizeAdjustSubtract(size_t size) {
  return size;
}

ALWAYS_INLINE PartitionTag* PartitionTagPointer(void* ptr) {
  // See the comment explaining the layout in partition_tag_bitmap.h.
  uintptr_t pointer_as_uintptr = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t bitmap_base =
      (pointer_as_uintptr & kSuperPageBaseMask) + kPartitionPageSize;
  uintptr_t offset =
      (pointer_as_uintptr & kSuperPageOffsetMask) - kPartitionPageSize;
  // Not to depend on partition_address_space.h and PartitionAllocGigaCage
  // feature, use "offset" to see whether the given ptr is_direct_mapped or not.
  // DirectMap object should cause this PA_DCHECK's failure, as tags aren't
  // currently supported there.
  PA_DCHECK(offset >= kReservedTagBitmapSize);
  size_t bitmap_offset = (offset - kReservedTagBitmapSize) >>
                         tag_bitmap::kBytesPerPartitionTagShift
                             << tag_bitmap::kPartitionTagSizeShift;
  return reinterpret_cast<PartitionTag* const>(bitmap_base + bitmap_offset);
}

ALWAYS_INLINE void* PartitionTagPointerAdjustSubtract(void* ptr) {
  return ptr;
}

ALWAYS_INLINE void* PartitionTagPointerAdjustAdd(void* ptr) {
  return ptr;
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

#elif ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR

using PartitionTag = uint8_t;

static constexpr PartitionTag kFixedTagValue = 0xAD;

struct PartitionTagWrapper {
  // Add padding before and after the tag, to avoid cacheline false sharing.
  // Assume cacheline is 64B.
  uint8_t unused1[64];
  PartitionTag partition_tag;
  uint8_t unused2[64];
};
extern BASE_EXPORT PartitionTagWrapper g_checked_ptr_single_tag;

static constexpr size_t kInSlotTagBufferSize = 0;

ALWAYS_INLINE size_t PartitionTagSizeAdjustAdd(size_t size) {
  return size;
}

ALWAYS_INLINE size_t PartitionTagSizeAdjustSubtract(size_t size) {
  return size;
}

ALWAYS_INLINE PartitionTag* PartitionTagPointer(void*) {
  return &g_checked_ptr_single_tag.partition_tag;
}

ALWAYS_INLINE void* PartitionTagPointerAdjustSubtract(void* ptr) {
  return ptr;
}

ALWAYS_INLINE void* PartitionTagPointerAdjustAdd(void* ptr) {
  return ptr;
}

ALWAYS_INLINE void PartitionTagSetValue(void*, size_t, PartitionTag) {}

ALWAYS_INLINE PartitionTag PartitionTagGetValue(void* ptr) {
  return *PartitionTagPointer(ptr);
}

ALWAYS_INLINE void PartitionTagClearValue(void* ptr, size_t) {}

#else  // !ENABLE_TAG_FOR_CHECKED_PTR2 && !ENABLE_TAG_FOR_MTE_CHECKED_PTR &&
       // !ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR

using PartitionTag = uint8_t;

static constexpr size_t kInSlotTagBufferSize = 0;

ALWAYS_INLINE size_t PartitionTagSizeAdjustAdd(size_t size) {
  return size;
}

ALWAYS_INLINE size_t PartitionTagSizeAdjustSubtract(size_t size) {
  return size;
}

ALWAYS_INLINE PartitionTag* PartitionTagPointer(void* ptr) {
  NOTREACHED();
  return nullptr;
}

ALWAYS_INLINE void* PartitionTagPointerAdjustSubtract(void* ptr) {
  return ptr;
}

ALWAYS_INLINE void* PartitionTagPointerAdjustAdd(void* ptr) {
  return ptr;
}

ALWAYS_INLINE void PartitionTagSetValue(void*, size_t, PartitionTag) {}

ALWAYS_INLINE PartitionTag PartitionTagGetValue(void*) {
  return 0;
}

ALWAYS_INLINE void PartitionTagClearValue(void* ptr, size_t) {}

#endif  // !ENABLE_TAG_FOR_CHECKED_PTR2 && !ENABLE_TAG_FOR_MTE_CHECKED_PTR &&
        // !ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_
