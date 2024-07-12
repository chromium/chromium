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
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ukm {
class UkmRecorder;
}

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

  void InitializeUkmManager(std::unique_ptr<ukm::UkmRecorder> recorder);
  void SetSourceId(ukm::SourceId source_id);

  UkmManager* ukm_manager() { return ukm_manager_.get(); }

 private:
  class SamplingController;

  std::unique_ptr<UkmManager> ukm_manager_;
  std::unique_ptr<SamplingController> compositor_latency_sampling_controller_;
  std::unique_ptr<SamplingController> event_latency_sampling_controller_;
};

}  // namespace cc

#endif  // CC_METRICS_LATENCY_UKM_REPORTER_H_
