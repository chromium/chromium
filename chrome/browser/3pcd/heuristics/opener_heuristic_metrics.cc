// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/3pcd/heuristics/opener_heuristic_metrics.h"

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

namespace {

std::unique_ptr<base::BucketRanges> CreateBucketRanges(
    size_t bucket_count,
    base::HistogramBase::Sample maximum) {
  auto ranges = std::make_unique<base::BucketRanges>(bucket_count + 1);
  base::Histogram::InitializeBucketRanges(1, maximum, ranges.get());
  return ranges;
}

base::Histogram::Sample Bucketize(base::Histogram::Sample value,
                                  const base::BucketRanges& bucket_ranges) {
  // Copied from SampleVectorBase::GetBucketIndex()
  size_t under = 0;
  size_t over = bucket_ranges.size();
  size_t mid;
  do {
    DCHECK_GE(over, under);
    mid = under + (over - under) / 2;
    if (mid == under) {
      break;
    }
    if (bucket_ranges.range(mid) <= value) {
      under = mid;
    } else {
      over = mid;
    }
  } while (true);

  DCHECK_LE(bucket_ranges.range(mid), value);
  if (mid < bucket_ranges.size() - 1) {
    CHECK_GT(bucket_ranges.range(mid + 1), value);
  }
  return bucket_ranges.range(mid);
}

}  // namespace

int32_t Bucketize3PCDHeuristicTimeDelta(
    base::TimeDelta td,
    base::TimeDelta maximum,
    base::RepeatingCallback<int64_t(const base::TimeDelta*)> cast_time_delta) {
  constexpr size_t bucket_count = 50;

  // Initialize BucketRanges only once:
  static const base::BucketRanges& bucket_ranges = *[=]() {
    auto ranges =
        CreateBucketRanges(bucket_count, cast_time_delta.Run(&maximum));
    ANNOTATE_LEAKING_OBJECT_PTR(ranges.get());
    return ranges.release();
  }();

  td = std::clamp(td, TimeDelta(), maximum);
  int64_t td_cast = cast_time_delta.Run(&td);
  if (td_cast > INT32_MAX) {
    td_cast = INT32_MAX;
  }

  return Bucketize(td_cast, bucket_ranges);
}
