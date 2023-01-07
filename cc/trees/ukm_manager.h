// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_UKM_MANAGER_H_
#define CC_TREES_UKM_MANAGER_H_

#include <memory>
#include <vector>

#include "cc/cc_export.h"
#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace cc {

enum class AggregationType;

class CC_EXPORT UkmRecorderFactory {
 public:
  virtual ~UkmRecorderFactory() {}

  virtual std::unique_ptr<ukm::UkmRecorder> CreateRecorder() = 0;
};

// TODO(xidachen): rename the class to CompositorUkmManager.
class CC_EXPORT UkmManager {
 public:
  explicit UkmManager(std::unique_ptr<ukm::UkmRecorder> recorder);
  ~UkmManager();

  void SetSourceId(ukm::SourceId source_id);

  void RecordThroughputUKM(FrameSequenceTrackerType tracker_type,
                           FrameInfo::SmoothEffectDrivingThread thread_type,
                           int64_t throughput) const;
  void RecordAggregateThroughput(AggregationType aggregation_type,
                                 int64_t throughput_percent) const;
  void RecordCompositorLatencyUKM(
      const CompositorFrameReporter::FrameReportTypes& report_types,
      const std::vector<CompositorFrameReporter::StageData>& stage_history,
      const ActiveTrackers& active_trackers,
      const CompositorFrameReporter::ProcessedBlinkBreakdown&
          processed_blink_breakdown,
      const CompositorFrameReporter::ProcessedVizBreakdown&
          processed_viz_breakdown) const;

  void RecordEventLatencyUKM(
      const EventMetrics::List& events_metrics,
      const std::vector<CompositorFrameReporter::StageData>& stage_history,
      const CompositorFrameReporter::ProcessedBlinkBreakdown&
          processed_blink_breakdown,
      const CompositorFrameReporter::ProcessedVizBreakdown&
          processed_viz_breakdown) const;

  ukm::UkmRecorder* recorder_for_testing() { return recorder_.get(); }

 private:
  ukm::SourceId source_id_ = ukm::kInvalidSourceId;
  std::unique_ptr<ukm::UkmRecorder> recorder_;
};

}  // namespace cc

#endif  // CC_TREES_UKM_MANAGER_H_
