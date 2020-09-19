// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_BITMAP_H_

#include "base/allocator/partition_allocator/checked_ptr_support.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"

namespace base {

namespace internal {

#if ENABLE_TAG_FOR_MTE_CHECKED_PTR

// Normal bucket layout
// +----------------+ super_page_base
// | PartitionPage  |
// | (Meta+Guard)   |
// +----------------+ super_page_base + PartitionPageSize() (=bitmap_base)
// |  TagBitmap     |
// ....
// +- - - - - - - - + bitmap_base + kActualTagBitmapSize
// | guard pages(*) | (kActualTagBitmapSize is SystemPageSize()-aligned.)
// +----------------+ bitmap_base + kReservedTagBitmapSize
// |   Slot Span    | (kReservedTagBitmapSize is PartitionPageSize()-aligned.)
// ....
// ....
// +----------------+
// |   Slot Span    |
// ....
// ....
// +----------------+
// | PartitionPage  |
// |  (GuardPage)   |
// +----------------+ super_page_base + kSuperPageSize
// (*) If kActualTagBitmapSize < kReservedTagBitmapSize, the
// unused pages are guard pages. This depends on sizeof(PartitionTag).
// TODO(tasak): Consider guaranteeing guard pages after the tag bitmap, if
// needed.

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
// (1) kSuperPageSize - 2 * PartitionPageSize() = kTagBitmapSize + kSlotSpanSize
// (2) kTagBitmapSize >= kSlotSpanSize / kBytesPerPartitionTag *
// sizeof(PartitionTag)
//--
// (1)' kSlotSpanSize = kSuperPageSize - 2 * PartitionPageSize() -
// kTagBitmapSize
// (2)' kSlotSpanSize <= kTagBitmapSize * Y
// (3)' Y = kBytesPerPartitionTag / sizeof(PartitionTag) =
// kBytesPerPartitionTagRatio
//
//   kTagBitmapSize * Y >= kSuperPageSize - 2 * PartitionPageSize() -
//   kTagBitmapSize (1 + Y) * kTagBimapSize >= kSuperPageSize - 2 *
//   PartitionPageSize()
// Finally,
//   kTagBitmapSize >= (kSuperPageSize - 2 * PartitionPageSize()) / (1 + Y)
static constexpr size_t kNumPartitionPagesPerTagBitmap =
    tag_bitmap::CeilCountOfUnits(kSuperPageSize / PartitionPageSize() - 2,
                                 tag_bitmap::kBytesPerPartitionTagRatio + 1);

// To make guard pages between the tag bitmap and the slot span, calculate the
// number of SystemPages of TagBitmap. If kNumSystemPagesPerTagBitmap *
// SystemPageSize() < kTagBitmapSize, guard pages will be created. (c.f. no
// guard pages if sizeof(PartitionTag) == 2.)
static constexpr size_t kNumSystemPagesPerTagBitmap =
    tag_bitmap::CeilCountOfUnits(kSuperPageSize / SystemPageSize() -
                                     2 * PartitionPageSize() / SystemPageSize(),
                                 tag_bitmap::kBytesPerPartitionTagRatio + 1);

static constexpr size_t kActualTagBitmapSize =
    kNumSystemPagesPerTagBitmap * SystemPageSize();

// PartitionPageSize-aligned tag bitmap size.
static constexpr size_t kReservedTagBitmapSize =
    PartitionPageSize() * kNumPartitionPagesPerTagBitmap;

static_assert(kActualTagBitmapSize <= kReservedTagBitmapSize,
              "kActualTagBitmapSize should be smaller than or equal to "
              "kReservedTagBitmapSize.");
static_assert(kReservedTagBitmapSize - kActualTagBitmapSize <
                  PartitionPageSize(),
              "Unused space in the tag bitmap should be smaller than "
              "PartitionPageSize()");

// The region available for slot spans is the reminder of the super page, after
// taking away the first and last partition page (for metadata and guard pages)
// and partition pages reserved for the tag bitmap.
static constexpr size_t kSlotSpansSize =
    kSuperPageSize - 2 * PartitionPageSize() - kReservedTagBitmapSize;
static_assert(kActualTagBitmapSize * tag_bitmap::kBytesPerPartitionTagRatio >=
                  kSlotSpansSize,
              "bitmap is large enough to cover slot spans");
static_assert((kActualTagBitmapSize - PartitionPageSize()) *
                      tag_bitmap::kBytesPerPartitionTagRatio <
                  kSlotSpansSize,
              "any smaller bitmap wouldn't suffice to cover slots spans");

#else  // !ENABLE_TAG_FOR_MTE_CHECKED_PTR

static constexpr size_t kNumPartitionPagesPerTagBitmap = 0;
static constexpr size_t kActualTagBitmapSize = 0;
static constexpr size_t kReservedTagBitmapSize = 0;

#endif

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_BITMAP_H_
