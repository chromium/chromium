// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/3pcd_heuristics/opener_heuristic_metrics.h"

#include <memory>

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram.h"

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
  CHECK_GT(bucket_ranges.range(mid + 1), value);
  return bucket_ranges.range(mid);
}

}  // namespace

base::Histogram::Sample BucketizeHoursSinceLastInteraction(TimeDelta td) {
  constexpr size_t bucket_count = 50;
  constexpr TimeDelta maximum = base::Days(30);

  // Initialize BucketRanges only once:
  static const base::BucketRanges& bucket_ranges = *[=]() {
    auto ranges = CreateBucketRanges(bucket_count, maximum.InHours());
    ANNOTATE_LEAKING_OBJECT_PTR(ranges.get());
    return ranges.release();
  }();

  td = std::clamp(td, TimeDelta(), maximum);
  return Bucketize(td.InHours(), bucket_ranges);
}

base::Histogram::Sample BucketizeSecondsSinceCommitted(TimeDelta td) {
  constexpr size_t bucket_count = 50;
  constexpr TimeDelta maximum = base::Minutes(3);

  // Initialize BucketRanges only once:
  static const base::BucketRanges& bucket_ranges = *[=]() {
    auto ranges = CreateBucketRanges(bucket_count, maximum.InSeconds());
    ANNOTATE_LEAKING_OBJECT_PTR(ranges.get());
    return ranges.release();
  }();

  td = std::clamp(td, TimeDelta(), maximum);
  return Bucketize(td.InSeconds(), bucket_ranges);
}
