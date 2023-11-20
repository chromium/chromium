// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/ui_metric_recorder_lacros.h"

#include "base/metrics/histogram_macros.h"

UiMetricRecorderLacros::UiMetricRecorderLacros() = default;
UiMetricRecorderLacros::~UiMetricRecorderLacros() = default;

void UiMetricRecorderLacros::ReportPercentDroppedFramesInOneSecondWindow2(
    double percent) {
  UMA_HISTOGRAM_PERCENTAGE(
      "Chrome.Lacros.Smoothness.PercentDroppedFrames_1sWindow2", percent);
}

// A stub method for CustomMetricRecorder::ReportEventLatency.
void UiMetricRecorderLacros::ReportEventLatency(
    std::vector<cc::EventLatencyTracker::LatencyData> latencies) {}
