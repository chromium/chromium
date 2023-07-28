// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_3PCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_
#define CHROME_BROWSER_3PCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_

#include <cstdint>
#include <functional>
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

// Quantize `td` into 50 buckets, distributed non-linearly similarly to
// UmaHistogramMediumTimes().
int32_t Bucketize3PCDHeuristicTimeDelta(
    base::TimeDelta td,
    base::TimeDelta maximum,
    base::RepeatingCallback<int64_t(const base::TimeDelta*)> cast_time_delta);

#endif  // CHROME_BROWSER_3PCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_
