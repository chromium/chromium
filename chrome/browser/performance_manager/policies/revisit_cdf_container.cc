// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/revisit_cdf_container.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"

namespace performance_manager {

RevisitCdfContainer::RevisitCdfContainer(std::vector<Entry> entries)
    : cdf_(std::move(entries)) {
  CHECK_GT(cdf_.size(), 0UL);
  CHECK_EQ(cdf_[cdf_.size() - 1].probability, 1.0f);

#if DCHECK_IS_ON()
  // CHECK that each bucket's boundaries and probability is greater than or
  // equal to the previous bucket's, as those are invariants of a cumulative
  // distribution function.
  for (auto it = cdf_.begin(); (it + 1) != cdf_.end(); ++it) {
    DCHECK(it->bucket <= (it + 1)->bucket);
    DCHECK(it->probability <= (it + 1)->probability);
  }
#endif
}

RevisitCdfContainer::RevisitCdfContainer(const RevisitCdfContainer&) = default;
RevisitCdfContainer::~RevisitCdfContainer() = default;

float RevisitCdfContainer::GetProbability(uint64_t value) const {
  auto it = std::lower_bound(
      cdf_.begin(), cdf_.end(), value,
      [](const Entry& entry, uint64_t val) { return entry.bucket < val; });

  if (it == cdf_.end()) {
    // If the value is larger than the last bucket, it's inside that bucket and
    // its cumulative probability is 1.
    return 1.0f;
  }

  // If the value is exactly a bucket's boundary, it's in that bucket.
  if (it->bucket == value) {
    return it->probability;
  }

  if (it == cdf_.begin()) {
    // If the value is smaller than the lowest bucket, its cumulative
    // probability is 0.
    return 0.0f;
  }

  // The value is in the nearest bucket that's lower than the one found by
  // lower_bound
  // TODO(crbug.com/1469337): Consider linear interpolation between buckets
  return (it - 1)->probability;
}

}  // namespace performance_manager
