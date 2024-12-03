// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_
#define CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_

#include <stdint.h>

// Bucketize `sample` into 50 buckets, capped at maximum and distributed
// non-linearly similarly to base::Histogram::InitializeBucketRanges.
int32_t Bucketize3PCDHeuristicSample(int64_t sample, int64_t maximum);

#endif  // CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_METRICS_H_
