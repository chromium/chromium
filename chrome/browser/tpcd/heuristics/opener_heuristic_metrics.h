// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_
#define CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_

#include <cstdint>
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

// Bucketize `sample` into 50 buckets, capped at maximum and distributed
// non-linearly similarly to base::Histogram::InitializeBucketRanges.
int32_t Bucketize3PCDHeuristicTimeDelta(
    base::TimeDelta sample_td,
    base::TimeDelta maximum_td,
    base::RepeatingCallback<int64_t(const base::TimeDelta*)> cast_time_delta);

#endif  // CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_
