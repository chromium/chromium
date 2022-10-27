// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_UI_METRIC_RECORDER_LACROS_H_
#define CHROME_BROWSER_LACROS_UI_METRIC_RECORDER_LACROS_H_

#include "cc/metrics/custom_metrics_recorder.h"

// Records cc metrics for lacros browser UI.
class UiMetricRecorderLacros : public cc::CustomMetricRecorder {
 public:
  UiMetricRecorderLacros();
  ~UiMetricRecorderLacros() override;

  // cc::CustomMetricRecorder:
  void ReportPercentDroppedFramesInOneSecondWindow(double percentage) override;
  void ReportEventLatency(
      std::vector<cc::EventLatencyTracker::LatencyData> latencies) override;
};

#endif  // CHROME_BROWSER_LACROS_UI_METRIC_RECORDER_LACROS_H_
