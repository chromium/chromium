// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_METRICS_UTIL_H_
#define ASH_PUBLIC_CPP_METRICS_UTIL_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace ash {
namespace metrics_util {

using ReportCallback = base::RepeatingCallback<void(
    const cc::FrameSequenceMetrics::CustomReportData&)>;
using SmoothnessCallback = base::RepeatingCallback<void(int smoothness)>;

// Returns a ReportCallback that could be passed to ui::ThroughputTracker
// or ui::AnimationThroughputReporter. The returned callback picks up the
// cc::FrameSequenceMetrics::ThroughputData, calculates the smoothness
// out of it and forward it to the smoothness report callback.
ASH_PUBLIC_EXPORT ReportCallback
ForSmoothness(SmoothnessCallback callback,
              bool exclude_from_data_collection = false);

// Starts to collect data reported by all trackers unless they opt out.
// Note this DCHECKs if called again without StopDataCollection().
ASH_PUBLIC_EXPORT void StartDataCollection();

// Stops data collection and returns the data collected since starting.
ASH_PUBLIC_EXPORT std::vector<cc::FrameSequenceMetrics::CustomReportData>
StopDataCollection();

// Gets the currently collected data and clears it after return.
ASH_PUBLIC_EXPORT std::vector<cc::FrameSequenceMetrics::CustomReportData>
GetCollectedData();

// Returns smoothness calculated from given data.
ASH_PUBLIC_EXPORT int CalculateSmoothness(
    const cc::FrameSequenceMetrics::CustomReportData& data);

// Returns jank percentage calculated from given data.
ASH_PUBLIC_EXPORT int CalculateJank(
    const cc::FrameSequenceMetrics::CustomReportData& data);

}  // namespace metrics_util
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_METRICS_UTIL_H_
