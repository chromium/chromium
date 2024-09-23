// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_BUCKET_LOOKUP_H_
#define PARTITION_ALLOC_PARTITION_BUCKET_LOOKUP_H_

#include <array>
#include <cstdint>
#include <utility>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"

namespace partition_alloc::internal {

// Don't use an anonymous namespace for the constants because it can inhibit
// collapsing them together, even when they are tagged as inline.

// Precalculate some shift and mask constants used in the hot path.
// Example: malloc(41) == 101001 binary.
// Order is 6 (1 << 6-1) == 32 is highest bit set.
// order_index is the next three MSB == 010 == 2.
// sub_order_index_mask is a mask for the remaining bits == 11 (masking to 01
// for the sub_order_index).
constexpr uint8_t OrderIndexShift(uint8_t order) {
  if (order < kNumBucketsPerOrderBits + 1) {
    return 0;
  }

  return order - (kNumBucketsPerOrderBits + 1);
}

constexpr size_t OrderSubIndexMask(uint8_t order) {
  if (order == kBitsPerSizeT) {
    return static_cast<size_t>(-1) >> (kNumBucketsPerOrderBits + 1);
  }

  return ((static_cast<size_t>(1) << order) - 1) >>
         (kNumBucketsPerOrderBits + 1);
}

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
static_assert(kBitsPerSizeT == 64, "");
#else
static_assert(kBitsPerSizeT == 32, "");
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

// Orders range from 0 to `kBitsPerSizeT`, inclusive.
inline constexpr uint8_t kNumOrders = kBitsPerSizeT + 1;

template <typename SizeClass, SizeClass... Index>
constexpr auto MakeOrderArray(SizeClass (*order_function)(uint8_t),
                              std::integer_sequence<SizeClass, Index...> seq) {
  return std::array{order_function(Index)...};
}

inline constexpr auto kOrderIndexShift =
    MakeOrderArray(OrderIndexShift,
                   std::make_integer_sequence<uint8_t, kNumOrders>{});

inline constexpr auto kOrderSubIndexMask =
    MakeOrderArray(OrderSubIndexMask,
                   std::make_integer_sequence<size_t, kNumOrders>{});

// The class used to generate the bucket lookup table at compile-time.
class BucketIndexLookup final {
 public:
  PA_ALWAYS_INLINE static constexpr uint16_t GetIndexForNeutralBuckets(
      size_t size);
  PA_ALWAYS_INLINE static constexpr uint16_t GetIndexForDenserBuckets(
      size_t size);
  PA_ALWAYS_INLINE static constexpr uint16_t GetIndex(size_t size);

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

PA_ALWAYS_INLINE constexpr size_t RoundUpToPowerOfTwo(size_t size) {
  const size_t n = 1 << base::bits::Log2Ceiling(static_cast<uint32_t>(size));
  PA_CHECK(size <= n);
  return n;
}

PA_ALWAYS_INLINE constexpr size_t RoundUpSize(size_t size) {
  const size_t next_power = RoundUpToPowerOfTwo(size);
  const size_t prev_power = next_power >> 1;
  PA_CHECK(size <= next_power);
  PA_CHECK(prev_power < size);
  if (size <= prev_power * 5 / 4) {
    return prev_power * 5 / 4;
  } else {
    return next_power;
  }
}

PA_ALWAYS_INLINE constexpr uint16_t RoundUpToOdd(uint16_t size) {
  return (size % 2 == 0) + size;
}

// static
PA_ALWAYS_INLINE constexpr uint16_t BucketIndexLookup::GetIndexForDenserBuckets(
    size_t size) {
  // This forces the bucket table to be constant-initialized and immediately
  // materialized in the binary.
  constexpr BucketIndexLookup lookup{};
  const size_t order =
      kBitsPerSizeT - static_cast<size_t>(base::bits::CountlZero(size));
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

// static
PA_ALWAYS_INLINE constexpr uint16_t
BucketIndexLookup::GetIndexForNeutralBuckets(size_t size) {
  const auto index = GetIndexForDenserBuckets(size);
  // Below the minimum size, 4 and 8 bucket distributions are the same, since we
  // can't fit any more buckets per order; this is due to alignment
  // requirements: each bucket must be a multiple of the alignment, which
  // implies the difference between buckets must also be a multiple of the
  // alignment. In smaller orders, this limits the number of buckets we can
  // have per order. So, for these small order, we do not want to skip every
  // second bucket.
  //
  // We also do not want to go about the index for the max bucketed size.
  if (size > kAlignment * kNumBucketsPerOrder &&
      index < GetIndexForDenserBuckets(kMaxBucketed)) {
    return RoundUpToOdd(index);
  } else {
    return index;
  }
}

// static
PA_ALWAYS_INLINE constexpr uint16_t BucketIndexLookup::GetIndex(size_t size) {
  // For any order 2^N, under the denser bucket distribution ("Distribution A"),
  // we have 4 evenly distributed buckets: 2^N, 1.25*2^N, 1.5*2^N, and 1.75*2^N.
  // These numbers represent the maximum size of an allocation that can go into
  // a given bucket.
  //
  // Under the less dense bucket distribution ("Distribution B"), we only have
  // 2 buckets for the same order 2^N: 2^N and 1.25*2^N.
  //
  // Everything that would be mapped to the last two buckets of an order under
  // Distribution A is instead mapped to the first bucket of the next order
  // under Distribution B. The following diagram shows roughly what this looks
  // like for the order starting from 2^10, as an example.
  //
  // A: ... | 2^10 | 1.25*2^10 | 1.5*2^10 | 1.75*2^10 | 2^11 | ...
  // B: ... | 2^10 | 1.25*2^10 | -------- | --------- | 2^11 | ...
  //
  // So, an allocation of size 1.4*2^10 would go into the 1.5*2^10 bucket under
  // Distribution A, but to the 2^11 bucket under Distribution B.
  if (1 << 8 < size && size < kHighThresholdForAlternateDistribution) {
    return BucketIndexLookup::GetIndexForNeutralBuckets(RoundUpSize(size));
  }
  return BucketIndexLookup::GetIndexForNeutralBuckets(size);
}

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PARTITION_BUCKET_LOOKUP_H_
