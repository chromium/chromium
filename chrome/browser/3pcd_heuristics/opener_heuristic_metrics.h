// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_3PCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_
#define CHROME_BROWSER_3PCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_

#include <cstdint>
#include "base/time/time.h"

// Quantize `td` to a number of hours between 0 and 720 (30 days), and placed in
// one of 50 buckets. The buckets are distributed non-linearly by
// base::Histogram::InitializeBucketRanges().
int32_t BucketizeHoursSinceLastInteraction(base::TimeDelta td);

// Quantize `td` similar to UmaHistogramMediumTimes() -- number of seconds up to
// 3 minutes, in 50 buckets.
int32_t BucketizeSecondsSinceCommitted(base::TimeDelta td);

#endif  // CHROME_BROWSER_3PCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_
