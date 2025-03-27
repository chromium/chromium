// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/bucket_ranges.h"

#include <algorithm>
#include <cmath>

#include "base/containers/span.h"
#include "base/metrics/crc32.h"

namespace base {

namespace {

constexpr bool is_sorted_and_unique(
    base::span<const HistogramBase::Sample32> c) {
  // Return true if we cannot find any adjacent pair {a, b} where a >= b.
  return std::adjacent_find(c.begin(), c.end(),
                            std::greater_equal<HistogramBase::Sample32>()) ==
         c.end();
}

}  // namespace

BucketRanges::BucketRanges(size_t num_ranges)
    : ranges_(num_ranges, 0), checksum_(0) {}

BucketRanges::BucketRanges(base::span<const HistogramBase::Sample32> data)
    : ranges_(data.begin(), data.end()), checksum_(0) {
  // Because the range values must be in sorted order, it suffices to only
  // validate that the first one is non-negative.
  if (!ranges_.empty() && ranges_[0] >= 0 && is_sorted_and_unique(ranges_)) {
    ResetChecksum();
  } else {
    ranges_.clear();
  }
}

BucketRanges::~BucketRanges() = default;

uint32_t BucketRanges::CalculateChecksum() const {
  // Crc of empty ranges_ happens to be 0. This early exit prevents trying to
  // take the address of ranges_[0] which will fail for an empty vector even
  // if that address is never used.
  const size_t ranges_size = ranges_.size();
  if (ranges_size == 0) {
    return 0;
  }

  // Checksum is seeded with the ranges "size".
  return Crc32(static_cast<uint32_t>(ranges_size), base::as_byte_span(ranges_));
}

bool BucketRanges::HasValidChecksum() const {
  return CalculateChecksum() == checksum_;
}

void BucketRanges::ResetChecksum() {
  checksum_ = CalculateChecksum();
}

bool BucketRanges::Equals(const BucketRanges* other) const {
  if (checksum_ != other->checksum_) {
    return false;
  }
  if (ranges_.size() != other->ranges_.size()) {
    return false;
  }
  for (size_t index = 0; index < ranges_.size(); ++index) {
    if (ranges_[index] != other->ranges_[index]) {
      return false;
    }
  }
  return true;
}

}  // namespace base
