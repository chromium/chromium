// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_METRICS_UTIL_H_
#define ASH_PUBLIC_CPP_METRICS_UTIL_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace ash::metrics_util {

using ReportCallback = base::RepeatingCallback<void(
    const cc::FrameSequenceMetrics::CustomReportData&)>;
using SmoothnessCallback = base::RepeatingCallback<void(int smoothness)>;

// Animation smoothness data collected between `StartDataCollection` and
// `StopDataCollection`, and reported from `GetCollectedData`.
struct ASH_PUBLIC_EXPORT AnimationData {
  // When an animation starts. It is recorded when `ForSmoothness` is
  // called, which happens when the animation is created.
  base::TimeTicks start_tick;
  // When an animation stops. It is recorded when smoothness data is reported.
  base::TimeTicks stop_tick;
  // Collected smoothness data.
  cc::FrameSequenceMetrics::CustomReportData smoothness_data;
};

// Returns a ReportCallback that could be passed to ui::ThroughputTracker
// or ui::AnimationThroughputReporter. The returned callback picks up the
// cc::FrameSequenceMetrics::ThroughputData, calculates the smoothness
// out of it and forward it to the smoothness report callback.
ASH_PUBLIC_EXPORT ReportCallback
ForSmoothnessV3(SmoothnessCallback callback,
                bool exclude_from_data_collection = false);

// Starts to collect data reported by all trackers unless they opt out.
// Note this DCHECKs if called again without StopDataCollection().
ASH_PUBLIC_EXPORT void StartDataCollection();

// Stops data collection and returns the data collected since starting.
ASH_PUBLIC_EXPORT std::vector<AnimationData> StopDataCollection();

// Gets the currently collected data and clears it after return.
ASH_PUBLIC_EXPORT std::vector<AnimationData> GetCollectedData();

// Returns smoothness calculated from given data.
ASH_PUBLIC_EXPORT int CalculateSmoothnessV3(
    const cc::FrameSequenceMetrics::CustomReportData& data);

// Returns jank percentage calculated from given data.
ASH_PUBLIC_EXPORT int CalculateJankV3(
    const cc::FrameSequenceMetrics::CustomReportData& data);

}  // namespace ash::metrics_util

#endif  // ASH_PUBLIC_CPP_METRICS_UTIL_H_
