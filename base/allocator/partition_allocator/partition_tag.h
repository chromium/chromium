// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_

// This file defines types and functions for `MTECheckedPtr<T>` (cf.
// `tagging.h`, which deals with real ARM MTE).

#include <string.h>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_tag_bitmap.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "build/build_config.h"

namespace partition_alloc {

#if defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)

// Use 8 bits for the partition tag.
// TODO(tasak): add a description about the partition tag.
using PartitionTag = uint8_t;

static_assert(
    sizeof(PartitionTag) == internal::tag_bitmap::kPartitionTagSize,
    "sizeof(PartitionTag) must be equal to bitmap::kPartitionTagSize.");

PA_ALWAYS_INLINE PartitionTag* PartitionTagPointer(uintptr_t addr) {
  // TODO(crbug.com/1307514): Add direct map support. For now, just assume
  // that direct maps don't have tags.
  PA_DCHECK(internal::IsManagedByNormalBuckets(addr));

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
  return reinterpret_cast<PartitionTag*>(bitmap_base + offset_in_bitmap);
}

PA_ALWAYS_INLINE PartitionTag* PartitionTagPointer(const void* ptr) {
  return PartitionTagPointer(
      internal::UnmaskPtr(reinterpret_cast<uintptr_t>(ptr)));
}

namespace internal {

PA_ALWAYS_INLINE void PartitionTagSetValue(uintptr_t addr,
                                           size_t size,
                                           PartitionTag value) {
  PA_DCHECK((size % tag_bitmap::kBytesPerPartitionTag) == 0);
  size_t tag_count = size >> tag_bitmap::kBytesPerPartitionTagShift;
  PartitionTag* tag_ptr = PartitionTagPointer(addr);
  if (sizeof(PartitionTag) == 1) {
    memset(tag_ptr, value, tag_count);
  } else {
    while (tag_count-- > 0)
      *tag_ptr++ = value;
  }
}

PA_ALWAYS_INLINE void PartitionTagSetValue(void* ptr,
                                           size_t size,
                                           PartitionTag value) {
  PartitionTagSetValue(reinterpret_cast<uintptr_t>(ptr), size, value);
}

PA_ALWAYS_INLINE PartitionTag PartitionTagGetValue(void* ptr) {
  return *PartitionTagPointer(ptr);
}

PA_ALWAYS_INLINE void PartitionTagClearValue(void* ptr, size_t size) {
  size_t tag_region_size = size >> tag_bitmap::kBytesPerPartitionTagShift
                                       << tag_bitmap::kPartitionTagSizeShift;
  PA_DCHECK(!memchr(PartitionTagPointer(ptr), 0, tag_region_size));
  memset(PartitionTagPointer(ptr), 0, tag_region_size);
}

PA_ALWAYS_INLINE void PartitionTagIncrementValue(void* ptr, size_t size) {
  PartitionTag tag = PartitionTagGetValue(ptr);
  PartitionTag new_tag = tag;
  ++new_tag;
  new_tag += !new_tag;  // Avoid 0.
#if BUILDFLAG(PA_DCHECK_IS_ON)
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

}  // namespace internal

#else  // No-op versions

using PartitionTag = uint8_t;

PA_ALWAYS_INLINE PartitionTag* PartitionTagPointer(void* ptr) {
  PA_NOTREACHED();
  return nullptr;
}

namespace internal {

PA_ALWAYS_INLINE void PartitionTagSetValue(void*, size_t, PartitionTag) {}

PA_ALWAYS_INLINE PartitionTag PartitionTagGetValue(void*) {
  return 0;
}

PA_ALWAYS_INLINE void PartitionTagClearValue(void* ptr, size_t) {}

PA_ALWAYS_INLINE void PartitionTagIncrementValue(void* ptr, size_t size) {}

}  // namespace internal

#endif  // defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_
