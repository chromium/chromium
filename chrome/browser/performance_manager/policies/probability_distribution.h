// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROBABILITY_DISTRIBUTION_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROBABILITY_DISTRIBUTION_H_

#include <utility>
#include <vector>

namespace performance_manager {

// This class represents a probability distribution function as a collection of
// buckets where for each bucket, `bucket` is the independent variable of the
// function and `probability` is the associated probability value.
class ProbabilityDistribution {
 public:
  struct Entry {
    uint64_t bucket;
    float probability;
  };

  ProbabilityDistribution(const ProbabilityDistribution&);
  ~ProbabilityDistribution();

  // Constructs a probability distribution from the collection of bucket
  // entries. This function validates that the data represents a valid
  // cumulative distribution function, that is the buckets are ordered, the
  // probability value in each bucket N is greater or equal to the probability
  // value in bucket N - 1, and the probability value of the last bucket is
  // equal to 1.
  static ProbabilityDistribution FromCDFData(std::vector<Entry> entries);

  // Constructs a probability distribution from the collection of bucket
  // entries. This function only validates that the buckets are ordered and
  // otherwise doesn't assert anything about the shape of the data expect that
  // no probability value is above 1 or below 0.
  static ProbabilityDistribution FromOrderedData(std::vector<Entry> entries);

  // Returns the cumulative probability of the bucket `value` belongs to, that
  // is the highest bucket for which `bucket_lower_bound <= value`
  float GetProbability(uint64_t value) const;

 private:
  explicit ProbabilityDistribution(std::vector<Entry> entries);

  std::vector<Entry> data_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROBABILITY_DISTRIBUTION_H_
