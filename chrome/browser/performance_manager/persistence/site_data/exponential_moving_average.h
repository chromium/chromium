// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_EXPONENTIAL_MOVING_AVERAGE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_EXPONENTIAL_MOVING_AVERAGE_H_

#include <cstddef>

namespace performance_manager {

// This class is an implementation of an exponential moving average,
// as described in
// https://en.wikipedia.org/wiki/Moving_average#Exponential_moving_average.
class ExponentialMovingAverage {
 public:
  // Create a new moving average with |alpha| sample weight factor.
  // 0.0 < |alpha| < 1.0.
  explicit ExponentialMovingAverage(float alpha);

  // Append a datum to the moving average.
  void AppendDatum(float datum);

  // Prepend a datum to the moving average.
  // This is generally only used once to update the average with the average
  // from a string of datums from a previous session or the like.
  void PrependDatum(float datum);

  // Set to no datums, zero value.
  void Clear();

  // Accessors.
  float alpha() const { return alpha_; }
  float value() const { return value_; }
  size_t num_datums() const { return num_datums_; }

 private:
  // The sample weight.
  const float alpha_;

  // The first datum.
  float first_datum_ = 0.0;
  // The current value of the moving average.
  float value_ = 0.0;
  // The number of datums added to this average.
  size_t num_datums_ = 0;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_EXPONENTIAL_MOVING_AVERAGE_H_
