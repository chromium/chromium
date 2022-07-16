// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_BUCKET_LOOKUP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_BUCKET_LOOKUP_H_

#include <cstdint>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/bits.h"
#include "base/compiler_specific.h"

namespace base {
namespace internal {
namespace {

// Precalculate some shift and mask constants used in the hot path.
// Example: malloc(41) == 101001 binary.
// Order is 6 (1 << 6-1) == 32 is highest bit set.
// order_index is the next three MSB == 010 == 2.
// sub_order_index_mask is a mask for the remaining bits == 11 (masking to 01
// for the sub_order_index).
constexpr uint8_t OrderIndexShift(uint8_t order) {
  if (order < kNumBucketsPerOrderBits + 1)
    return 0;

  return order - (kNumBucketsPerOrderBits + 1);
}

constexpr size_t OrderSubIndexMask(uint8_t order) {
  if (order == kBitsPerSizeT)
    return static_cast<size_t>(-1) >> (kNumBucketsPerOrderBits + 1);

  return ((static_cast<size_t>(1) << order) - 1) >>
         (kNumBucketsPerOrderBits + 1);
}

#if defined(PA_HAS_64_BITS_POINTERS)
#define BITS_PER_SIZE_T 64
static_assert(kBitsPerSizeT == 64, "");
#else
#define BITS_PER_SIZE_T 32
static_assert(kBitsPerSizeT == 32, "");
#endif  // defined(PA_HAS_64_BITS_POINTERS)

constexpr uint8_t kOrderIndexShift[BITS_PER_SIZE_T + 1] = {
    OrderIndexShift(0),  OrderIndexShift(1),  OrderIndexShift(2),
    OrderIndexShift(3),  OrderIndexShift(4),  OrderIndexShift(5),
    OrderIndexShift(6),  OrderIndexShift(7),  OrderIndexShift(8),
    OrderIndexShift(9),  OrderIndexShift(10), OrderIndexShift(11),
    OrderIndexShift(12), OrderIndexShift(13), OrderIndexShift(14),
    OrderIndexShift(15), OrderIndexShift(16), OrderIndexShift(17),
    OrderIndexShift(18), OrderIndexShift(19), OrderIndexShift(20),
    OrderIndexShift(21), OrderIndexShift(22), OrderIndexShift(23),
    OrderIndexShift(24), OrderIndexShift(25), OrderIndexShift(26),
    OrderIndexShift(27), OrderIndexShift(28), OrderIndexShift(29),
    OrderIndexShift(30), OrderIndexShift(31), OrderIndexShift(32),
#if BITS_PER_SIZE_T == 64
    OrderIndexShift(33), OrderIndexShift(34), OrderIndexShift(35),
    OrderIndexShift(36), OrderIndexShift(37), OrderIndexShift(38),
    OrderIndexShift(39), OrderIndexShift(40), OrderIndexShift(41),
    OrderIndexShift(42), OrderIndexShift(43), OrderIndexShift(44),
    OrderIndexShift(45), OrderIndexShift(46), OrderIndexShift(47),
    OrderIndexShift(48), OrderIndexShift(49), OrderIndexShift(50),
    OrderIndexShift(51), OrderIndexShift(52), OrderIndexShift(53),
    OrderIndexShift(54), OrderIndexShift(55), OrderIndexShift(56),
    OrderIndexShift(57), OrderIndexShift(58), OrderIndexShift(59),
    OrderIndexShift(60), OrderIndexShift(61), OrderIndexShift(62),
    OrderIndexShift(63), OrderIndexShift(64)
#endif
};

constexpr size_t kOrderSubIndexMask[BITS_PER_SIZE_T + 1] = {
    OrderSubIndexMask(0),  OrderSubIndexMask(1),  OrderSubIndexMask(2),
    OrderSubIndexMask(3),  OrderSubIndexMask(4),  OrderSubIndexMask(5),
    OrderSubIndexMask(6),  OrderSubIndexMask(7),  OrderSubIndexMask(8),
    OrderSubIndexMask(9),  OrderSubIndexMask(10), OrderSubIndexMask(11),
    OrderSubIndexMask(12), OrderSubIndexMask(13), OrderSubIndexMask(14),
    OrderSubIndexMask(15), OrderSubIndexMask(16), OrderSubIndexMask(17),
    OrderSubIndexMask(18), OrderSubIndexMask(19), OrderSubIndexMask(20),
    OrderSubIndexMask(21), OrderSubIndexMask(22), OrderSubIndexMask(23),
    OrderSubIndexMask(24), OrderSubIndexMask(25), OrderSubIndexMask(26),
    OrderSubIndexMask(27), OrderSubIndexMask(28), OrderSubIndexMask(29),
    OrderSubIndexMask(30), OrderSubIndexMask(31), OrderSubIndexMask(32),
#if BITS_PER_SIZE_T == 64
    OrderSubIndexMask(33), OrderSubIndexMask(34), OrderSubIndexMask(35),
    OrderSubIndexMask(36), OrderSubIndexMask(37), OrderSubIndexMask(38),
    OrderSubIndexMask(39), OrderSubIndexMask(40), OrderSubIndexMask(41),
    OrderSubIndexMask(42), OrderSubIndexMask(43), OrderSubIndexMask(44),
    OrderSubIndexMask(45), OrderSubIndexMask(46), OrderSubIndexMask(47),
    OrderSubIndexMask(48), OrderSubIndexMask(49), OrderSubIndexMask(50),
    OrderSubIndexMask(51), OrderSubIndexMask(52), OrderSubIndexMask(53),
    OrderSubIndexMask(54), OrderSubIndexMask(55), OrderSubIndexMask(56),
    OrderSubIndexMask(57), OrderSubIndexMask(58), OrderSubIndexMask(59),
    OrderSubIndexMask(60), OrderSubIndexMask(61), OrderSubIndexMask(62),
    OrderSubIndexMask(63), OrderSubIndexMask(64)
#endif
};

}  // namespace

// The class used to generate the bucket lookup table at compile-time.
class BucketIndexLookup final {
 public:
  ALWAYS_INLINE constexpr static size_t GetIndex(size_t size);

  constexpr BucketIndexLookup() {
    constexpr uint16_t sentinel_bucket_index = kNumBuckets;

    InitBucketSizes();

    uint16_t* bucket_index_ptr = &bucket_index_lookup_[0];
    uint16_t bucket_index = 0;

    // Very small allocations, smaller than the first bucketed order ->
    // everything goes to the first bucket.
    for (uint8_t order = 0; order < kMinBucketedOrder; ++order) {
      for (uint16_t j = 0; j < kNumBucketsPerOrder; ++j) {
        *bucket_index_ptr++ = 0;
      }
    }

    // Normal buckets.
    for (uint8_t order = kMinBucketedOrder; order <= kMaxBucketedOrder;
         ++order) {
      size_t size = static_cast<size_t>(1) << (order - 1);
      size_t current_increment = size >> kNumBucketsPerOrderBits;
      for (uint16_t j = 0; j < kNumBucketsPerOrder; ++j) {
        *bucket_index_ptr++ = bucket_index;

        // For small sizes, buckets are close together (current_increment is
        // small). For instance, for:
        // - kAlignment == 16 (which is the case on most 64 bit systems)
        // - kNumBucketsPerOrder == 4
        //
        // The 3 next buckets after 16 are {20, 24, 28}. None of these are a
        // multiple of kAlignment, so they use the next bucket, that is 32 here.
        if (size % kAlignment != 0) {
          PA_DCHECK(bucket_sizes_[bucket_index] > size);
          // Do not increment bucket_index, since in the example above
          // current_size may be 20, and bucket_sizes_[bucket_index] == 32.
        } else {
          PA_DCHECK(bucket_sizes_[bucket_index] == size);
          bucket_index++;
        }

        size += current_increment;
      }
    }

    // Direct-mapped, and overflow.
    for (uint8_t order = kMaxBucketedOrder + 1; order <= kBitsPerSizeT;
         ++order) {
      for (uint16_t j = 0; j < kNumBucketsPerOrder; ++j) {
        *bucket_index_ptr++ = sentinel_bucket_index;
      }
    }

    // Smaller because some buckets are not valid due to alignment constraints.
    PA_DCHECK(bucket_index < kNumBuckets);
    PA_DCHECK(bucket_index_ptr == bucket_index_lookup_ + ((kBitsPerSizeT + 1) *
                                                          kNumBucketsPerOrder));
    // And there's one last bucket lookup that will be hit for e.g. malloc(-1),
    // which tries to overflow to a non-existent order.
    *bucket_index_ptr = sentinel_bucket_index;
  }
  constexpr const size_t* bucket_sizes() const { return &bucket_sizes_[0]; }

 private:
  constexpr void InitBucketSizes() {
    size_t current_size = kSmallestBucket;
    size_t current_increment = kSmallestBucket >> kNumBucketsPerOrderBits;
    size_t* bucket_size = &bucket_sizes_[0];
    for (size_t i = 0; i < kNumBucketedOrders; ++i) {
      for (size_t j = 0; j < kNumBucketsPerOrder; ++j) {
        // All bucket sizes have to be multiples of kAlignment, skip otherwise.
        if (current_size % kAlignment == 0) {
          *bucket_size = current_size;
          ++bucket_size;
        }
        current_size += current_increment;
      }
      current_increment <<= 1;
    }

    // The remaining buckets are invalid.
    while (bucket_size < bucket_sizes_ + kNumBuckets) {
      *(bucket_size++) = kInvalidBucketSize;
    }
  }

  size_t bucket_sizes_[kNumBuckets]{};
  // The bucket lookup table lets us map a size_t to a bucket quickly.
  // The trailing +1 caters for the overflow case for very large allocation
  // sizes.  It is one flat array instead of a 2D array because in the 2D
  // world, we'd need to index array[blah][max+1] which risks undefined
  // behavior.
  uint16_t
      bucket_index_lookup_[((kBitsPerSizeT + 1) * kNumBucketsPerOrder) + 1]{};
};

// static
ALWAYS_INLINE constexpr size_t BucketIndexLookup::GetIndex(size_t size) {
  // This forces the bucket table to be constant-initialized and immediately
  // materialized in the binary.
  constexpr BucketIndexLookup lookup{};
  const uint8_t order = kBitsPerSizeT - bits::CountLeadingZeroBitsSizeT(size);
  // The order index is simply the next few bits after the most significant
  // bit.
  const size_t order_index =
      (size >> kOrderIndexShift[order]) & (kNumBucketsPerOrder - 1);
  // And if the remaining bits are non-zero we must bump the bucket up.
  const size_t sub_order_index = size & kOrderSubIndexMask[order];
  const uint16_t index =
      lookup.bucket_index_lookup_[(order << kNumBucketsPerOrderBits) +
                                  order_index + !!sub_order_index];
  PA_DCHECK(index <= kNumBuckets);  // Last one is the sentinel bucket.
  return index;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_BUCKET_LOOKUP_H_
