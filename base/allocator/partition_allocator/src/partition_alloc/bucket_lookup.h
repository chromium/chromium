// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef PARTITION_ALLOC_BUCKET_LOOKUP_H_
#define PARTITION_ALLOC_BUCKET_LOOKUP_H_

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_forward.h"

// `BucketIndexLookup` class provides 2-way mapping between "allocation size"
// and "bucket index".
// https://chromium.googlesource.com/chromium/src/+/HEAD/base/allocator/partition_allocator/buckets.md
//
// We have two different mappings; Neutral Bucket
// Distribution and Denser Bucket Distribution. As the name implies, Denser one
// has about twice as many buckets. Neutral Bucket Distribution leaves some
// buckets unused. This structure allows us to switch from Neutral to Denser at
// runtime easily. To simplify implementation, Neutral is implemented by
// rounding up indices from Denser (see `GetIndexForNeutralBuckets()`).
//
// Denser distribution is mixture of linear and exponential curve.
// For small size, we have a bucket for every `kAlignment` bytes linearly.
// For larger size, we have `kNumBucketsPerOrder` buckets for every power
// of two ("order"), exponentially.
//
// The linear curve and the exponential curve are implemented  as
// `LinearBucketMapping` and `ExponentialBucketMapping` respectively, and merged
// in `BucketIndexLookup`.
//
// Constants in this file must be kept in sync with
// //tools/memory/partition_allocator/objects_per_size.py.
// LINT.IfChange

namespace partition_alloc {

namespace internal {

class LinearBucketMapping final {
 public:
  static constexpr size_t kStep = internal::kAlignment;

  LinearBucketMapping() = delete;

  PA_ALWAYS_INLINE static constexpr size_t GetIndex(size_t size) {
    return size == 0 ? 0 : (size - 1) / kStep;
  }

  PA_ALWAYS_INLINE static constexpr size_t GetSize(uint16_t index) {
    return (static_cast<size_t>(index) + 1) * kStep;
  }
};

class ExponentialBucketMapping final {
 public:
  // 8 buckets per order (for the higher orders).
  // Note: this is not what is used by neutral distribution, but the maximum
  // amount of buckets per order. For neutral distribution, only 4 are used.
  static constexpr size_t kNumBucketsPerOrderBits = 3;
  static constexpr size_t kNumBucketsPerOrder = 1 << kNumBucketsPerOrderBits;

  ExponentialBucketMapping() = delete;

  PA_ALWAYS_INLINE static constexpr size_t GetIndex(size_t size) {
    // The "order" of an allocation is closely related to the power-of-2 size of
    // the allocation. More precisely, the order is the bit index of the
    // most-significant-bit in the allocation size, where the bit numbers starts
    // at index 1 for the least-significant-bit.
    //
    // Obtain index of MSB and rotate to extract Order Indices.
    //
    //                                        ┌──────── Order: 8
    //                                        │ ┌────── Order Index: 5
    //                                        │┌┴┐┌──┬─ Order Sub-Index: true
    //   Size 216 = 0b00000000000000000000000011011000
    //               32......................987654321  (n-th bit, 1-indexed)
    // After RotR = 0b10010000000000000000000000001101
    //                └─────────────────────────┬┘ └─┴─ Order Index
    //                                          └────── Order Sub-Index
    //
    // This rotation allows to extract indices with compile-time constant
    // masks.
    const size_t order =
        kBitsPerSizeT -
        static_cast<size_t>(internal::base::bits::CountlZero(size));
    const size_t rot = internal::base::bits::RotR(
        size, order - kNumBucketsPerOrderBits + kBitsPerSizeT - 1);

    // Index is the lowest `kNumBucketsPerOrderBits` bits after rotation.
    constexpr size_t kIndexMask = (size_t{1} << kNumBucketsPerOrderBits) - 1;
    const size_t order_index = rot & kIndexMask;

    // Sub-Index is the highest `kBitsPerSizeT - kNumBucketsPerOrderBits - 1`
    // bits after rotation. If it is non-zero, we should increase index by
    // one.
    constexpr size_t kSubIndexMask =
        ~((size_t{1} << (kNumBucketsPerOrderBits + 1)) - 1);
    const size_t sub_order_index = !!(rot & kSubIndexMask);

    return order * kNumBucketsPerOrder + order_index + sub_order_index;
  }

  PA_ALWAYS_INLINE static constexpr size_t GetSize(uint16_t index) {
    const size_t order = index / kNumBucketsPerOrder;
    const size_t order_index = index % kNumBucketsPerOrder;

    size_t size = kNumBucketsPerOrder | order_index;
    size <<= order == 0 ? 0 : order - 1 - kNumBucketsPerOrderBits;

    return size;
  }
};
}  // namespace internal

class BucketIndexLookup final {
  using LinearMap = internal::LinearBucketMapping;
  using ExponentialMap = internal::ExponentialBucketMapping;

  // PartitionAlloc should return memory properly aligned for any type, to
  // behave properly as a generic allocator. This is not strictly required as
  // long as types are explicitly allocated with PartitionAlloc, but is to use
  // it as a malloc() implementation, and generally to match malloc()'s
  // behavior. In practice, this means 8 bytes alignment on 32 bit
  // architectures, and 16 bytes on 64 bit ones. We use linear curve iff
  // `size` is too small for exponential distribution to violate fundamental
  // alignment.
  //
  // For size no greater than `kMaxLinear`, `LinearMap` is used. For size no
  // less than `kMinExponential`, `ExponentialMap` is  used. There is small
  // overlap between linear and exponential.
  //
  // LinearMap      | <-> | <------------> |
  // ExponentialMap |     | <------------> | <--------> |
  //                ^     ^                ^            ^
  //                0     kMinExponential  kMaxLinear   kMaxBucketSize

  static constexpr size_t kMinExponential =
      LinearMap::kStep << ExponentialMap::kNumBucketsPerOrderBits;
  static constexpr size_t kMaxLinear =
      LinearMap::kStep << (ExponentialMap::kNumBucketsPerOrderBits + 1);
  static_assert(kMinExponential < kMaxLinear);

  // There is a gap between Linear's index and Exponential's index at
  // `kMinExponential`. To reduce waste by holes, offset exponential index to
  // make "smooth" curve.
  static constexpr size_t kExponentialIndexOffset =
      ExponentialMap::GetIndex(kMinExponential) -
      LinearMap::GetIndex(kMinExponential);
  static_assert(kExponentialIndexOffset ==
                ExponentialMap::GetIndex(kMaxLinear) -
                    LinearMap::GetIndex(kMaxLinear));

 public:
  BucketIndexLookup() = delete;

  static constexpr size_t kMinBucketSize = LinearMap::kStep;
  // The largest bucketed order is 20, storing nearly 1 MiB (983040 bytes
  // precisely).
  static constexpr size_t kMaxBucketSize = ExponentialMap::GetSize(
      (20 + 1) * ExponentialMap::kNumBucketsPerOrder - 1);

  static constexpr uint16_t kNumBuckets = static_cast<uint16_t>(
      ExponentialMap::GetIndex(kMaxBucketSize) - kExponentialIndexOffset + 1);

  PA_ALWAYS_INLINE static constexpr uint16_t GetIndexForDenserBuckets(
      size_t size) {
    size_t index_if_linear = LinearMap::GetIndex(size);
    size_t index_if_exponential =
        std::min(ExponentialMap::GetIndex(size) - kExponentialIndexOffset,
                 size_t{kNumBuckets});

    // Ternary operator will likely to be compiled as conditional move.
    size_t index = size <= kMaxLinear ? index_if_linear : index_if_exponential;

    // Last one is the sentinel bucket.
    PA_DCHECK(index <= kNumBuckets);
    return static_cast<uint16_t>(index);
  }

  PA_ALWAYS_INLINE static constexpr uint16_t GetIndexForNeutralBuckets(
      size_t size) {
    uint16_t index = GetIndexForDenserBuckets(size);
    // Below the minimum size, 4 and 8 bucket distributions are the same, since
    // we can't fit any more buckets per order; this is due to alignment
    // requirements: each bucket must be a multiple of the alignment, which
    // implies the difference between buckets must also be a multiple of the
    // alignment. In smaller orders, this limits the number of buckets we can
    // have per order. So, for these small order, we do not want to skip every
    // second bucket.
    //
    // We also do not want to go about the index for the max bucketed size.
    if (size >= kMaxLinear && index + 1 < kNumBuckets) {
      index += (index % 2 == 0);
    }
    return index;
  }

  PA_ALWAYS_INLINE static constexpr size_t GetBucketSize(uint16_t index) {
    PA_DCHECK(index < kNumBuckets);

    constexpr size_t kMaxLinearIndex = LinearMap::GetIndex(kMaxLinear);
    size_t size_if_linear = LinearMap::GetSize(index);
    size_t size_if_exponential =
        ExponentialMap::GetSize(index + kExponentialIndexOffset);

    // Ternary operator will likely to be compiled as conditional move.
    return index <= kMaxLinearIndex ? size_if_linear : size_if_exponential;
  }

  PA_CONSTINIT static const std::array<size_t, kNumBuckets> kBucketSizes;
};

}  // namespace partition_alloc

// LINT.ThenChange(//tools/memory/partition_allocator/objects_per_size.py)

#endif  // PARTITION_ALLOC_BUCKET_LOOKUP_H_
