// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "components/viz/common/frame_timing_details.h"
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

  // These metrics are recorded while a user interaction is in progress.
  void SetUserInteractionInProgress(bool in_progress);
  void AddCheckerboardStatsForFrame(int64_t checkerboard_area,
                                    int64_t num_missing_tiles,
                                    int64_t total_visible_area);

  // These metrics are recorded until the source URL changes.
  void AddCheckerboardedImages(int num_of_checkerboarded_images);

  void RecordThroughputUKM(FrameSequenceTrackerType tracker_type,
                           FrameSequenceMetrics::ThreadType thread_type,
                           int64_t throughput) const;
  void RecordAggregateThroughput(AggregationType aggregation_type,
                                 int64_t throughput_percent) const;
  void RecordCompositorLatencyUKM(
      CompositorFrameReporter::FrameReportType report_type,
      const std::vector<CompositorFrameReporter::StageData>& stage_history,
      const CompositorFrameReporter::ActiveTrackers& active_trackers,
      const viz::FrameTimingDetails& viz_breakdown) const;

  void RecordEventLatencyUKM(
      const std::vector<EventMetrics>& events_metrics,
      const std::vector<CompositorFrameReporter::StageData>& stage_history,
      const viz::FrameTimingDetails& viz_breakdown) const;

  ukm::UkmRecorder* recorder_for_testing() { return recorder_.get(); }

 private:
  void RecordCheckerboardUkm();
  void RecordRenderingUkm();

  bool user_interaction_in_progress_ = false;
  int64_t num_of_images_checkerboarded_during_interaction_ = 0;
  int64_t checkerboarded_content_area_ = 0;
  int64_t num_missing_tiles_ = 0;
  int64_t total_visible_area_ = 0;
  int64_t num_of_frames_ = 0;

  int total_num_of_checkerboarded_images_ = 0;

  ukm::SourceId source_id_ = ukm::kInvalidSourceId;
  std::unique_ptr<ukm::UkmRecorder> recorder_;
};

}  // namespace cc

#endif  // CC_TREES_UKM_MANAGER_H_
