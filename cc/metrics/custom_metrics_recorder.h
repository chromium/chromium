// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_CUSTOM_METRICS_RECORDER_H_
#define CC_METRICS_CUSTOM_METRICS_RECORDER_H_

#include <vector>

#include "cc/cc_export.h"
#include "cc/metrics/event_latency_tracker.h"

namespace cc {

// Customize cc metric recording. E.g. reporting metrics under different names.
class CC_EXPORT CustomMetricRecorder {
 public:
  static CustomMetricRecorder* Get();

  // Invoked to report "PercentDroppedFrames_1sWindow2".
  virtual void ReportPercentDroppedFramesInOneSecondWindow2(double percent) = 0;

  // Invoked to report event latencies.
  virtual void ReportEventLatency(
      std::vector<EventLatencyTracker::LatencyData> latencies) = 0;

 protected:
  CustomMetricRecorder();
  virtual ~CustomMetricRecorder();
};

}  // namespace cc

#endif  // CC_METRICS_CUSTOM_METRICS_RECORDER_H_
