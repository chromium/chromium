// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_UI_THROUGHPUT_RECORDER_LACROS_H_
#define CHROME_BROWSER_LACROS_UI_THROUGHPUT_RECORDER_LACROS_H_

#include "cc/metrics/custom_metrics_recorder.h"

// Records cc metrics for lacros browser UI.
class UiThroughputRecorderLacros : public cc::CustomMetricRecorder {
 public:
  UiThroughputRecorderLacros();
  ~UiThroughputRecorderLacros() override;

  // cc::CustomMetricRecorder:
  void ReportPercentDroppedFramesInOneSecoundWindow(double percentage) override;
};

#endif  // CHROME_BROWSER_LACROS_UI_THROUGHPUT_RECORDER_LACROS_H_
