// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/heuristics/opener_heuristic_metrics.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "base/debug/leak_annotations.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"

using base::TimeDelta;

int32_t Bucketize3PCDHeuristicTimeDelta(
    base::TimeDelta sample_td,
    base::TimeDelta maximum_td,
    base::RepeatingCallback<int64_t(const base::TimeDelta*)> cast_time_delta) {
  constexpr size_t bucket_count = 50;
  int64_t sample = cast_time_delta.Run(&sample_td);
  int64_t maximum = cast_time_delta.Run(&maximum_td);

  // Clamp the sample between 0 and maximum, and to the max int32 value (only
  // int32 is supported by histograms).
  if (sample <= 0) {
    return 0;
  }
  if (sample > std::min(maximum, static_cast<int64_t>(INT32_MAX))) {
    return std::min(maximum, static_cast<int64_t>(INT32_MAX));
  }

  // This bucketing implementation is based heavily on
  // base::Histogram::InitializeBucketRanges, but without allocating extra
  // memory.
  base::Histogram::Sample current = 1;
  double log_current = 0;
  double log_max = log(static_cast<double>(maximum));
  // Iterate over buckets and return the one closest to the sample.
  // Two of the buckets are 0 and `maximum`. Loop over the remaining buckets.
  size_t cutoff_count = bucket_count - 2;
  for (size_t cutoff_index = 0; cutoff_index < cutoff_count; cutoff_index++) {
    // Increment the log of the bucket proportional to the current log over the
    // number of remaining buckets.
    double log_next =
        log_current + (log_max - log_current) / (cutoff_count - cutoff_index);
    base::Histogram::Sample next = static_cast<int>(std::round(exp(log_next)));

    // If the difference between the buckets is too close, just add 1 to the
    // previous bucket.
    if (next <= current) {
      next = current + 1;
    }

    // Check if the sample falls in the bucket, and return the lower bound if
    // it does.
    if (sample < next) {
      return current;
    }

    // Increment the current values to the next values.
    current = next;
    log_current = log_next;
  }
  return maximum;
}
