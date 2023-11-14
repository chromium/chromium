// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_CDF_CONTAINER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_CDF_CONTAINER_H_

#include <utility>
#include <vector>

namespace performance_manager {

// This class represents a cumulative distribution function as a sorted
// collection of buckets, where the bucket specifies its lower (inclusive) bound
// and the probability of a sample belonging to that bucket or any of the lower
// ones.
class RevisitCdfContainer {
 public:
  struct Entry {
    uint64_t bucket;
    float probability;
  };

  // Constructs a CDF container from the collection of bucket entries. The
  // caller is responsible for ensuring that the data represents a valid CDF,
  // that is the buckets are ordered, the probability value in each bucket N is
  // greater or equal to the probability value in bucket N - 1, and the
  // probability value of the last bucket is equal to 1.
  explicit RevisitCdfContainer(std::vector<Entry> entries);
  RevisitCdfContainer(const RevisitCdfContainer&);
  ~RevisitCdfContainer();

  // Returns the cumulative probability of the bucket `value` belongs to, that
  // is the highest bucket for which `bucket_lower_bound <= value`
  float GetProbability(uint64_t value) const;

 private:
  std::vector<Entry> cdf_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_CDF_CONTAINER_H_
