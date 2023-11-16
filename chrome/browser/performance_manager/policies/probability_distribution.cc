// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/probability_distribution.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"

namespace performance_manager {

// static
ProbabilityDistribution ProbabilityDistribution::FromCDFData(
    std::vector<Entry> entries) {
  CHECK_EQ(entries[entries.size() - 1].probability, 1.0f);

#if DCHECK_IS_ON()
  // CHECK that each bucket's boundaries and probability is greater than or
  // equal to the previous bucket's, as those are invariants of a cumulative
  // distribution function.
  for (auto it = entries.begin(); (it + 1) != entries.end(); ++it) {
    DCHECK(it->bucket <= (it + 1)->bucket);
    DCHECK(it->probability <= (it + 1)->probability);
    DCHECK(it->probability <= 1.0f);
    DCHECK(it->probability >= 0.0f);
  }
#endif

  return ProbabilityDistribution(std::move(entries));
}

// static
ProbabilityDistribution ProbabilityDistribution::FromOrderedData(
    std::vector<Entry> entries) {
#if DCHECK_IS_ON()
  // CHECK that each bucket's boundary is greater than or
  // equal to the previous bucket's.
  for (auto it = entries.begin(); (it + 1) != entries.end(); ++it) {
    DCHECK(it->bucket <= (it + 1)->bucket);
    DCHECK(it->probability <= 1.0f);
    DCHECK(it->probability >= 0.0f);
  }

  // The loop above only goes through the first N - 1 elements, also check the
  // last one
  DCHECK(entries.rbegin()->probability <= 1.0f);
  DCHECK(entries.rbegin()->probability >= 0.0f);
#endif

  return ProbabilityDistribution(std::move(entries));
}

ProbabilityDistribution::ProbabilityDistribution(std::vector<Entry> entries)
    : data_(std::move(entries)) {
  CHECK_GT(data_.size(), 0UL);
}

ProbabilityDistribution::ProbabilityDistribution(
    const ProbabilityDistribution&) = default;
ProbabilityDistribution::~ProbabilityDistribution() = default;

float ProbabilityDistribution::GetProbability(uint64_t value) const {
  auto it = std::lower_bound(
      data_.begin(), data_.end(), value,
      [](const Entry& entry, uint64_t val) { return entry.bucket < val; });

  if (it != data_.end()) {
    // If the value is exactly a bucket's boundary, it's in that bucket.
    if (it->bucket == value) {
      return it->probability;
    }

    if (it == data_.begin()) {
      // If the value is smaller than the lowest bucket, default to a
      // probability of 0.
      return 0.0f;
    }
  }

  // The value is in the nearest bucket that's lower than the one found by
  // lower_bound. If it's past the last bucket, its probability is the
  // probability of the last bucket.
  // TODO(crbug.com/1469337): Consider linear interpolation between buckets
  return (it - 1)->probability;
}

}  // namespace performance_manager
