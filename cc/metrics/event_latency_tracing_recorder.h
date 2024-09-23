// Copyright 2022 The Chromium Authors
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
  // Returns the name of the event dispatch breakdown of EventLatency trace
  // events between `start_stage` and `end_stage`.
  static const char* GetDispatchBreakdownName(
      EventMetrics::DispatchStage start_stage,
      EventMetrics::DispatchStage end_stage);

  // Returns the name of EventLatency breakdown between `dispatch_stage` and
  // `compositor_stage`.
  static const char* GetDispatchToCompositorBreakdownName(
      EventMetrics::DispatchStage dispatch_stage,
      CompositorFrameReporter::StageType compositor_stage);

  // Returns the name of EventLatency breakdown between `dispatch_stage` and
  // termination for events not associated with a frame update.
  static const char* GetDispatchToTerminationBreakdownName(
      EventMetrics::DispatchStage dispatch_stage);

  static void RecordEventLatencyTraceEvent(
      EventMetrics* event_metrics,
      base::TimeTicks termination_time,
      base::TimeDelta vsync_interval,
      const std::vector<CompositorFrameReporter::StageData>* stage_history,
      const CompositorFrameReporter::ProcessedVizBreakdown* viz_breakdown);

  static bool IsEventLatencyTracingEnabled();

 private:
  // We do not want the emitting of traces to have any side-effects, so the
  // actual emitting uses `const EventMetrics*`.
  static void RecordEventLatencyTraceEventInternal(
      const EventMetrics* event_metrics,
      base::TimeTicks termination_time,
      base::TimeDelta vsync_interval,
      const std::vector<CompositorFrameReporter::StageData>* stage_history,
      const CompositorFrameReporter::ProcessedVizBreakdown* viz_breakdown);
};

}  // namespace cc

#endif  // CC_METRICS_EVENT_LATENCY_TRACING_RECORDER_H_
