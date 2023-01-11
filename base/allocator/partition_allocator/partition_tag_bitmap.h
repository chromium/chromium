// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_BITMAP_H_

#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"

namespace partition_alloc::internal {

#if PA_CONFIG(ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)

namespace tag_bitmap {
// kPartitionTagSize should be equal to sizeof(PartitionTag).
// PartitionTag is defined in partition_tag.h and static_assert there
// checks the condition.
static constexpr size_t kPartitionTagSizeShift = 0;
static constexpr size_t kPartitionTagSize = 1U << kPartitionTagSizeShift;

static constexpr size_t kBytesPerPartitionTagShift = 4;
// One partition tag is assigned per |kBytesPerPartitionTag| bytes in the slot
// spans.
//  +-----------+ 0
//  |           |  ====> 1 partition tag
//  +-----------+ kBytesPerPartitionTag
//  |           |  ====> 1 partition tag
//  +-----------+ 2*kBytesPerPartitionTag
// ...
//  +-----------+ slot_size
static constexpr size_t kBytesPerPartitionTag = 1U
                                                << kBytesPerPartitionTagShift;
static_assert(
    kMinBucketedOrder >= kBytesPerPartitionTagShift + 1,
    "MTECheckedPtr requires kBytesPerPartitionTagShift-bytes alignment.");

static constexpr size_t kBytesPerPartitionTagRatio =
    kBytesPerPartitionTag / kPartitionTagSize;

static_assert(kBytesPerPartitionTag > 0,
              "kBytesPerPartitionTag should be larger than 0");
static_assert(
    kBytesPerPartitionTag % kPartitionTagSize == 0,
    "kBytesPerPartitionTag should be multiples of sizeof(PartitionTag).");

constexpr size_t CeilCountOfUnits(size_t size, size_t unit_size) {
  return (size + unit_size - 1) / unit_size;
}

}  // namespace tag_bitmap

// kTagBitmapSize is calculated in the following way:
// (1) kSuperPageSize - 2 * PartitionPageSize() = kTagBitmapSize +
// SlotSpanSize()
// (2) kTagBitmapSize >= SlotSpanSize() / kBytesPerPartitionTag *
// sizeof(PartitionTag)
//--
// (1)' SlotSpanSize() = kSuperPageSize - 2 * PartitionPageSize() -
// kTagBitmapSize
// (2)' SlotSpanSize() <= kTagBitmapSize * Y
// (3)' Y = kBytesPerPartitionTag / sizeof(PartitionTag) =
// kBytesPerPartitionTagRatio
//
//   kTagBitmapSize * Y >= kSuperPageSize - 2 * PartitionPageSize() -
//   kTagBitmapSize (1 + Y) * kTagBimapSize >= kSuperPageSize - 2 *
//   PartitionPageSize()
// Finally,
//   kTagBitmapSize >= (kSuperPageSize - 2 * PartitionPageSize()) / (1 + Y)
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR PA_ALWAYS_INLINE size_t
NumPartitionPagesPerTagBitmap() {
  return tag_bitmap::CeilCountOfUnits(
      kSuperPageSize / PartitionPageSize() - 2,
      tag_bitmap::kBytesPerPartitionTagRatio + 1);
}

// To make guard pages between the tag bitmap and the slot span, calculate the
// number of SystemPages of TagBitmap. If kNumSystemPagesPerTagBitmap *
// SystemPageSize() < kTagBitmapSize, guard pages will be created. (c.f. no
// guard pages if sizeof(PartitionTag) == 2.)
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR PA_ALWAYS_INLINE size_t
NumSystemPagesPerTagBitmap() {
  return tag_bitmap::CeilCountOfUnits(
      kSuperPageSize / SystemPageSize() -
          2 * PartitionPageSize() / SystemPageSize(),
      tag_bitmap::kBytesPerPartitionTagRatio + 1);
}

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR PA_ALWAYS_INLINE size_t
ActualTagBitmapSize() {
  return NumSystemPagesPerTagBitmap() * SystemPageSize();
}

// PartitionPageSize-aligned tag bitmap size.
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR PA_ALWAYS_INLINE size_t
ReservedTagBitmapSize() {
  return PartitionPageSize() * NumPartitionPagesPerTagBitmap();
}

#if PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR
static_assert(ActualTagBitmapSize() <= ReservedTagBitmapSize(),
              "kActualTagBitmapSize should be smaller than or equal to "
              "kReservedTagBitmapSize.");
static_assert(ReservedTagBitmapSize() - ActualTagBitmapSize() <
                  PartitionPageSize(),
              "Unused space in the tag bitmap should be smaller than "
              "PartitionPageSize()");

// The region available for slot spans is the reminder of the super page, after
// taking away the first and last partition page (for metadata and guard pages)
// and partition pages reserved for the freeslot bitmap and the tag bitmap.
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR PA_ALWAYS_INLINE size_t
SlotSpansSize() {
  return kSuperPageSize - 2 * PartitionPageSize() - ReservedTagBitmapSize();
}

static_assert(ActualTagBitmapSize() * tag_bitmap::kBytesPerPartitionTagRatio >=
                  SlotSpansSize(),
              "bitmap is large enough to cover slot spans");
static_assert((ActualTagBitmapSize() - PartitionPageSize()) *
                      tag_bitmap::kBytesPerPartitionTagRatio <
                  SlotSpansSize(),
              "any smaller bitmap wouldn't suffice to cover slots spans");
#endif  // PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR

#else

constexpr PA_ALWAYS_INLINE size_t NumPartitionPagesPerTagBitmap() {
  return 0;
}

constexpr PA_ALWAYS_INLINE size_t ActualTagBitmapSize() {
  return 0;
}

constexpr PA_ALWAYS_INLINE size_t ReservedTagBitmapSize() {
  return 0;
}

#endif  // PA_CONFIG(ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_BITMAP_H_
