// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_BUCKET_LOOKUP_H_
#define PARTITION_ALLOC_BUCKET_LOOKUP_H_

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_alloc_public_constants.h"

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
// Constants in this file must be kept in sync with
// //tools/memory/partition_allocator/objects_per_size.py.
// LINT.IfChange

namespace partition_alloc {

enum class BucketDistribution : uint8_t { kNeutral, kDenser };

class BucketIndexLookup final {
  // 8 buckets per order (for the higher orders).
  // Note: this is not what is used by neutral distribution, but the maximum
  // amount of buckets per order. For neutral distribution, only 4 are used.
  static constexpr size_t kNumBucketsPerOrderBits = 3;
  static constexpr size_t kNumBucketsPerOrder = 1 << kNumBucketsPerOrderBits;

  // PartitionAlloc should return memory properly aligned for any type, to
  // behave properly as a generic allocator. This is not strictly required as
  // long as types are explicitly allocated with PartitionAlloc, but is to use
  // it as a malloc() implementation, and generally to match malloc()'s
  // behavior. In practice, this means 8 bytes alignment on 32 bit
  // architectures, and 16 bytes on 64 bit ones. We use linear curve iff
  // `size` is too small for exponential distribution to violate fundamental
  // alignment.
  //
  // Linear      | <-> | <------------> |
  // Exponential |     | <------------> | <--------> |
  //            ^     ^                ^            ^
  //            0     kMinExponential  kMaxLinear   kMaxBucketSize
  static constexpr size_t kMinBucketSizeBits =
      std::countr_zero(internal::kAlignment);
  static constexpr size_t kMinExponential = internal::kAlignment
                                            << kNumBucketsPerOrderBits;
  static constexpr size_t kMaxLinear = kMinExponential << 1;
  static constexpr size_t kMaxLinearIndex =
      (kMaxLinear >> kMinBucketSizeBits) - 1;

  // The largest bucketed order is 20, storing nearly 1 MiB (983040 bytes
  // precisely).
  static constexpr size_t kMaxBucketedOrder = 20;

 public:
  BucketIndexLookup() = delete;

  static constexpr size_t kMinBucketSize = internal::kAlignment;
  static constexpr size_t kMaxBucketSize =
      (size_t{1} << kMaxBucketedOrder) -
      (size_t{1} << (kMaxBucketedOrder - kNumBucketsPerOrderBits - 1));
  static constexpr uint16_t kNumBuckets =
      (kMaxBucketedOrder - kMinBucketSizeBits - kNumBucketsPerOrderBits + 1) *
          kNumBucketsPerOrder -
      1;

  PA_ALWAYS_INLINE static constexpr uint16_t GetIndexForDenserBuckets(
      size_t size) {
    // Each bucket covers `(prev_bucket_size, bucket_size]`. Subtracting 1 maps
    // this half-open interval to `[prev_bucket_size, bucket_size - 1]`, where
    // all sizes in the same bucket share the same upper bits (`d >> shift`),
    // avoiding a separate remainder check for exact bucket boundaries.
    const size_t d = size == 0 ? 0 : size - 1;

    // Calculate the log2 step size `shift` for `d`'s power-of-2 order `[2^k,
    // 2^(k+1)]`. Since each order is divided into `kNumBucketsPerOrder = 8`
    // (`2^3`) buckets, the step within order `k` is `2^(k-3) = 1 << shift`, so
    // `shift = k - 3`.
    // OR-ing `d` with `kMinExponential` (`1 << (kMinBucketSizeBits + 3)`)
    // clamps `shift` at `kMinBucketSizeBits` for `size <= kMinExponential` so
    // the step size never shrinks below `kMinBucketSize` (`kAlignment`).
    const size_t shift =
        (internal::kBitsPerSizeT - 1 - kNumBucketsPerOrderBits) -
        static_cast<size_t>(std::countl_zero(d | kMinExponential));

    // Combine the order base `(shift - kMinBucketSizeBits) * 8` with the
    // sub-order offset `d >> shift`:
    // - For `size > kMinExponential` (`d >= kMinExponential`), shifting `d`
    //   right by `shift` puts the MSB (`1`) at bit 3 (`8`) and the 3-bit order
    //   index in bits `2..0`, yielding `8 + order_index`:
    //
    //                                     ┌──────── Order (k): 7 (MSB at bit 7)
    //                                     │┌─┬───── Order Index (3 bits): 5
    //   Size 216 - 1 = 215 = 0b0000...000011010111
    //                         32.........987654321  (n-th bit, 1-indexed)
    //   After >> shift (4) = 0b0000...000000001101  (= 8 + Order Index = 13)
    //                                         │└─┴─ Order Index (0..7)
    //                                         └──── MSB (1 for d >= 128)
    //
    // - For `size <= kMinExponential` (`d < kMinExponential`), `shift` is
    //   clamped at `kMinBucketSizeBits` while bit 3 of `d >> shift` is `0`,
    //   yielding the linear index `0..7` seamlessly before `8..15`.
    const size_t index = (shift << kNumBucketsPerOrderBits) + (d >> shift) -
                         (kMinBucketSizeBits << kNumBucketsPerOrderBits);

    // Clamp any allocation larger than `kMaxBucketSize` to the sentinel bucket
    // at `kNumBuckets`.
    return static_cast<uint16_t>(std::min(index, size_t{kNumBuckets}));
  }

  PA_ALWAYS_INLINE static constexpr uint16_t GetIndex(
      size_t size,
      BucketDistribution bucket_distribution) {
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
    if (bucket_distribution == BucketDistribution::kNeutral &&
        index > kMaxLinearIndex && index + 1 < kNumBuckets) {
      index |= 1;
    }
    return index;
  }

  PA_ALWAYS_INLINE static constexpr uint16_t GetIndexForNeutralBuckets(
      size_t size) {
    return GetIndex(size, BucketDistribution::kNeutral);
  }

  PA_ALWAYS_INLINE static constexpr size_t GetBucketSize(uint16_t index) {
    PA_DCHECK(index < kNumBuckets);

    // Bucket `index` returns the upper bound of its size range, which
    // corresponds to `(index + 1)` in `GetIndexForDenserBuckets` (`d = size -
    // 1`). Split `index + 1` into the power-of-2 order (`order`) and 3-bit
    // sub-order offset (`order_index` in `0..7`).
    const size_t order = (index + 1) / kNumBucketsPerOrder;
    const size_t order_index = (index + 1) % kNumBucketsPerOrder;

    // - For `order > 0` (`index >= 7`, sizes `>= kMinExponential`), restore the
    //   implicit leading MSB at bit 3 (`kNumBucketsPerOrder | order_index`, in
    //   `8..15`) and shift left by `order + kMinBucketSizeBits - 1`.
    // - For `order == 0` (`index < 7`, linear buckets below `kMinExponential`),
    //   pre-shifting `order_index` by 1 compensates for the `- 1` in the shift
    //   amount, yielding `order_index << kMinBucketSizeBits` with the same
    //   shift expression.
    const size_t mantissa =
        order == 0 ? order_index << 1 : (kNumBucketsPerOrder | order_index);
    return mantissa << (order + kMinBucketSizeBits - 1);
  }
};

static_assert(BucketIndexLookup::GetBucketSize(BucketIndexLookup::kNumBuckets -
                                               1) ==
              BucketIndexLookup::kMaxBucketSize);

}  // namespace partition_alloc

// LINT.ThenChange(//tools/memory/partition_allocator/objects_per_size.py)

namespace partition_alloc::internal {

// A slot's bucket index in `PartitionRoot::buckets_` and its slot size.
// `bucket_index` is only meaningful for non-direct-mapped slots.
//
// The two values are packed into a single 64-bit word rather than declared as
// separate members. A `uint16_t` next to a `size_t` leaves a six-byte interior
// padding hole, and Chromium builds with `-ftrivial-auto-var-init=pattern`, so
// every construction emits extra stores purely to poison that hole. This type
// is constructed on the deallocation fast path and passed by value, so it
// should also fit in a single general-purpose register.
class BucketSizeDetails {
 public:
  // The largest `slot_size` the packed representation holds.
  static constexpr uint64_t kMaxSlotSize = (uint64_t{1} << 48) - 1;

  constexpr BucketSizeDetails() = default;
  constexpr BucketSizeDetails(uint16_t bucket_index, size_t slot_size)
      : bits_((static_cast<uint64_t>(bucket_index) << kSlotSizeBits) |
              static_cast<uint64_t>(slot_size)) {}

  PA_ALWAYS_INLINE constexpr uint16_t bucket_index() const {
    return static_cast<uint16_t>(bits_ >> kSlotSizeBits);
  }
  PA_ALWAYS_INLINE constexpr size_t slot_size() const {
    return static_cast<size_t>(bits_ & kMaxSlotSize);
  }

 private:
  static constexpr int kSlotSizeBits = std::bit_width(kMaxSlotSize);

  // A slot is at most the largest allocation PartitionAlloc hands out, rounded
  // up to the direct-map reservation granularity. `kSuperPageSize` bounds that
  // rounding, so this is a conservative ceiling. Allocations with
  // `AllocFlags::kAllowGigaAllocations` can be larger; they are checked next
  // to `MaxGigaAllocationSize()`.
  static_assert(MaxAllocationSize() + kSuperPageSize <= kMaxSlotSize,
                "slot_size no longer fits in the packed representation");

  uint64_t bits_ = 0;
};

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_BUCKET_LOOKUP_H_
