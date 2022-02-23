// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENT_LATENCY_TRACING_RECORDER_H_
#define CC_METRICS_EVENT_LATENCY_TRACING_RECORDER_H_

#include <vector>

#include "base/time/time.h"
#include "cc/metrics/compositor_frame_reporter.h"

namespace cc {
class EventMetrics;

class EventLatencyTracingRecorder {
 public:
  static void RecordEventLatencyTraceEvent(
      EventMetrics* event_metrics,
      base::TimeTicks termination_time,
      const std::vector<CompositorFrameReporter::StageData>* stage_history,
      const CompositorFrameReporter::ProcessedVizBreakdown* viz_breakdown);
};

}  // namespace cc

#endif  // CC_METRICS_EVENT_LATENCY_TRACING_RECORDER_H_
