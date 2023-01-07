// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/metrics_util.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {
namespace metrics_util {

namespace {

bool g_data_collection_enabled = false;

std::vector<cc::FrameSequenceMetrics::CustomReportData>& GetDataCollector() {
  static base::NoDestructor<
      std::vector<cc::FrameSequenceMetrics::CustomReportData>>
      data;
  return *data;
}

void CollectDataAndForwardReport(
    ReportCallback callback,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  // An arbitrary cap on the maximum number of animations being collected.
  DCHECK_LT(GetDataCollector().size(), 1000u);

  GetDataCollector().push_back(data);
  std::move(callback).Run(data);
}

// Calculates smoothness from |throughput| and sends to |callback|.
void ForwardSmoothness(SmoothnessCallback callback,
                       const cc::FrameSequenceMetrics::CustomReportData& data) {
  bool animation_in_test =
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() !=
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION;
  // Do not report if tracker is started and stopped closely in time.
  // There are 2 cases:
  // *   frame_expected == 0
  //        when there is no BeginFrame between start and stop.
  // *   frame_expected == 1 && frames_produced == 0
  //        when there is one BeginFrame but no frame generated between
  //        start and stop; should not included this case in tests.
  if (!data.frames_expected ||
      (!animation_in_test && data.frames_expected == 1 &&
       data.frames_produced == 0)) {
    return;
  }
  callback.Run(CalculateSmoothness(data));
}

}  // namespace

ReportCallback ForSmoothness(SmoothnessCallback callback,
                             bool exclude_from_data_collection) {
  auto forward_smoothness =
      base::BindRepeating(&ForwardSmoothness, std::move(callback));
  if (!g_data_collection_enabled || exclude_from_data_collection)
    return forward_smoothness;

  return base::BindRepeating(&CollectDataAndForwardReport,
                             std::move(forward_smoothness));
}

void StartDataCollection() {
  DCHECK(!g_data_collection_enabled);
  g_data_collection_enabled = true;
}

std::vector<cc::FrameSequenceMetrics::CustomReportData> StopDataCollection() {
  DCHECK(g_data_collection_enabled);
  g_data_collection_enabled = false;

  return GetCollectedData();
}

std::vector<cc::FrameSequenceMetrics::CustomReportData> GetCollectedData() {
  std::vector<cc::FrameSequenceMetrics::CustomReportData> data;
  data.swap(GetDataCollector());
  return data;
}

int CalculateSmoothness(
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  DCHECK(data.frames_expected);
  return std::floor(100.0f * data.frames_produced / data.frames_expected);
}

int CalculateJank(const cc::FrameSequenceMetrics::CustomReportData& data) {
  DCHECK(data.frames_expected);
  return std::floor(100.0f * data.jank_count / data.frames_expected);
}

}  // namespace metrics_util
}  // namespace ash
