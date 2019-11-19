// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SESSION_STATISTIC_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SESSION_STATISTIC_H_

#include "base/sequence_checker.h"
#include "base/values.h"

// OptimizationGuideSessionStatistic calculates running statistics, mean and
// variance, for real valued inputs one sample at a time.
class OptimizationGuideSessionStatistic {
 public:
  OptimizationGuideSessionStatistic();
  ~OptimizationGuideSessionStatistic();

  // Update the statistics using the provided |sample|.
  void AddSample(float sample);

  // Return the current mean for the samples collected.
  float GetMean() const;

  // Return the current variance for the samples collected if the number of
  // samples is at least 2, otherwise return 0.
  float GetVariance() const;

  // Return the current standard deviation for the samples collected if the
  // number of samples is at least 2, otherwise return 0.
  float GetStdDev() const;

  // Return the number of samples that |this| has calculated statistics for.
  size_t GetNumberOfSamples() const;

 private:
  size_t num_samples_;
  float mean_;
  // This holds the sum of the squared differences of the current sample with
  // current sample mean. This is not the actual variance, that is provided
  // through GetVariance().
  float variance_sum_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideSessionStatistic);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SESSION_STATISTIC_H_
