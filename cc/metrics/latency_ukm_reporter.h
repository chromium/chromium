// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_LATENCY_UKM_REPORTER_H_
#define CC_METRICS_LATENCY_UKM_REPORTER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/event_metrics.h"

namespace cc {
class UkmManager;

// A helper class that takes latency data from a CompositorFrameReporter and
// talks to UkmManager to report it.
class CC_EXPORT LatencyUkmReporter {
 public:
  LatencyUkmReporter();
  ~LatencyUkmReporter();

  LatencyUkmReporter(const LatencyUkmReporter&) = delete;
  LatencyUkmReporter& operator=(const LatencyUkmReporter&) = delete;

  void ReportCompositorLatencyUkm(
      const CompositorFrameReporter::FrameReportTypes& report_types,
      const std::vector<CompositorFrameReporter::StageData>& stage_history,
      const ActiveTrackers& active_trackers,
      const CompositorFrameReporter::ProcessedBlinkBreakdown&
          processed_blink_breakdown,
      const CompositorFrameReporter::ProcessedVizBreakdown&
          processed_viz_breakdown);

  void ReportEventLatencyUkm(
      const EventMetrics::List& events_metrics,
      const std::vector<CompositorFrameReporter::StageData>& stage_history,
      const CompositorFrameReporter::ProcessedBlinkBreakdown&
          processed_blink_breakdown,
      const CompositorFrameReporter::ProcessedVizBreakdown&
          processed_viz_breakdown);

  void set_ukm_manager(UkmManager* manager) { ukm_manager_ = manager; }

 private:
  class SamplingController;

  // This is pointing to the LayerTreeHostImpl::ukm_manager_, which is
  // initialized right after the LayerTreeHostImpl is created. So when this
  // pointer is initialized, there should be no trackers yet. Moreover, the
  // LayerTreeHostImpl::ukm_manager_ lives as long as the LayerTreeHostImpl, so
  // this pointer should never be null as long as LayerTreeHostImpl is alive.
  raw_ptr<UkmManager> ukm_manager_ = nullptr;

  std::unique_ptr<SamplingController> compositor_latency_sampling_controller_;
  std::unique_ptr<SamplingController> event_latency_sampling_controller_;
};

}  // namespace cc

#endif  // CC_METRICS_LATENCY_UKM_REPORTER_H_
