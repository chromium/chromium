// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"

#include <cmath>

OptimizationGuideSessionStatistic::OptimizationGuideSessionStatistic()
    : num_samples_(0u), mean_(0.0), variance_sum_(0.0) {}

OptimizationGuideSessionStatistic::~OptimizationGuideSessionStatistic() =
    default;

void OptimizationGuideSessionStatistic::AddSample(float sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  num_samples_++;
  float new_mean = mean_ + (sample - mean_) / static_cast<float>(num_samples_);
  variance_sum_ += (sample - mean_) * (sample - new_mean);
  mean_ = new_mean;
}

float OptimizationGuideSessionStatistic::GetMean() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mean_;
}

float OptimizationGuideSessionStatistic::GetVariance() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return num_samples_ > 1 ? variance_sum_ / static_cast<float>(num_samples_ - 1)
                          : 0.0;
}

float OptimizationGuideSessionStatistic::GetStdDev() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return num_samples_ > 1 ? std::sqrt(GetVariance()) : 0.0;
}

size_t OptimizationGuideSessionStatistic::GetNumberOfSamples() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return num_samples_;
}
