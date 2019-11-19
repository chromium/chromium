// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_COMPOSITOR_FRAME_REPORTER_H_
#define CC_METRICS_COMPOSITOR_FRAME_REPORTER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "cc/base/base_export.h"
#include "cc/cc_export.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "components/viz/common/frame_timing_details.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
class RollingTimeDeltaHistory;

// This is used for tracing and reporting the duration of pipeline stages within
// a single frame.
//
// For each stage in the frame pipeline, calling StartStage will start tracing
// that stage (and end any currently running stages).
//
// If the tracked frame is submitted (i.e. the frame termination status is
// kSubmittedFrame or kSubmittedFrameMissedDeadline), then the duration of each
// stage along with the total latency will be reported to UMA. These reported
// durations will be differentiated by whether the compositor is single threaded
// and whether the submitted frame missed the deadline. The format of each stage
// reported to UMA is "[SingleThreaded]Compositor.[MissedFrame.].<StageName>".
class CC_EXPORT CompositorFrameReporter {
 public:
  enum class FrameTerminationStatus {
    // The tracked compositor frame was presented.
    kPresentedFrame,

    // The tracked compositor frame was submitted to the display compositor but
    // was not presented.
    kDidNotPresentFrame,

    // Main frame was aborted; the reporter will not continue reporting.
    kMainFrameAborted,

    // Reporter that is currently at a stage is replaced by a new one (e.g. two
    // BeginImplFrames can happen without issuing BeginMainFrame, so the first
    // reporter would terminate with this status).
    // TODO(alsan): Track impl-only frames.
    kReplacedByNewReporter,

    // Frame that was being tracked did not end up being submitting (e.g. frame
    // had no damage or LTHI was ended).
    kDidNotProduceFrame,

    // Default termination status. Should not be reachable.
    kUnknown
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class MissedFrameReportTypes {
    kNonMissedFrame = 0,
    kMissedFrame = 1,
    kDeprecatedMissedFrameLatencyIncrease = 2,
    kMissedFrameReportTypeCount
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class StageType {
    kBeginImplFrameToSendBeginMainFrame = 0,
    kSendBeginMainFrameToCommit = 1,
    kCommit = 2,
    kEndCommitToActivation = 3,
    kActivation = 4,
    kEndActivateToSubmitCompositorFrame = 5,
    kSubmitCompositorFrameToPresentationCompositorFrame = 6,
    kTotalLatency = 7,
    kStageTypeCount
  };

  enum class VizBreakdown {
    kSubmitToReceiveCompositorFrame = 0,
    kReceivedCompositorFrameToStartDraw = 1,
    kStartDrawToSwapEnd = 2,
    kSwapEndToPresentationCompositorFrame = 3,
    kBreakdownCount
  };

  enum class BlinkBreakdown {
    kHandleInputEvents = 0,
    kAnimate = 1,
    kStyleUpdate = 2,
    kLayoutUpdate = 3,
    kPrepaint = 4,
    kComposite = 5,
    kPaint = 6,
    kScrollingCoordinator = 7,
    kCompositeCommit = 8,
    kUpdateLayers = 9,
    kBreakdownCount
  };

  CompositorFrameReporter(
      const base::flat_set<FrameSequenceTrackerType>* active_trackers,
      bool is_single_threaded = false);
  ~CompositorFrameReporter();

  CompositorFrameReporter(const CompositorFrameReporter& reporter) = delete;
  CompositorFrameReporter& operator=(const CompositorFrameReporter& reporter) =
      delete;

  void MissedSubmittedFrame();

  // Note that the started stage may be reported to UMA. If the histogram is
  // intended to be reported then the histograms.xml file must be updated too.
  void StartStage(StageType stage_type, base::TimeTicks start_time);
  void TerminateFrame(FrameTerminationStatus termination_status,
                      base::TimeTicks termination_time);
  void SetBlinkBreakdown(
      std::unique_ptr<BeginMainFrameMetrics> blink_breakdown);
  void SetVizBreakdown(const viz::FrameTimingDetails& viz_breakdown);

  int StageHistorySizeForTesting() { return stage_history_.size(); }

  void OnFinishImplFrame(base::TimeTicks timestamp);
  void OnAbortBeginMainFrame();
  bool did_finish_impl_frame() const { return did_finish_impl_frame_; }
  bool did_abort_main_frame() const { return did_abort_main_frame_; }
  base::TimeTicks impl_frame_finish_time() const {
    return impl_frame_finish_time_;
  }

 protected:
  struct StageData {
    StageType stage_type;
    base::TimeTicks start_time;
    base::TimeTicks end_time;
    BeginMainFrameMetrics blink_breakdown;
    viz::FrameTimingDetails viz_breakdown;
    StageData();
    StageData(StageType stage_type,
              base::TimeTicks start_time,
              base::TimeTicks end_time);
    StageData(const StageData&);
    ~StageData();
  };

  StageData current_stage_;

  // Stage data is recorded here. On destruction these stages will be reported
  // to UMA if the termination status is |kPresentedFrame|. Reported data will
  // be divided based on the frame submission status.
  std::vector<StageData> stage_history_;

 private:
  void TerminateReporter();
  void EndCurrentStage(base::TimeTicks end_time);
  void ReportStageHistograms(bool missed_frame) const;
  void ReportStageHistogramWithBreakdown(
      CompositorFrameReporter::MissedFrameReportTypes report_type,
      FrameSequenceTrackerType frame_sequence_tracker_type,
      const CompositorFrameReporter::StageData& stage) const;
  void ReportBlinkBreakdown(
      CompositorFrameReporter::MissedFrameReportTypes report_type,
      FrameSequenceTrackerType frame_sequence_tracker_type,
      const CompositorFrameReporter::StageData& stage) const;
  void ReportVizBreakdown(
      CompositorFrameReporter::MissedFrameReportTypes report_type,
      FrameSequenceTrackerType frame_sequence_tracker_type,
      const CompositorFrameReporter::StageData& stage) const;
  void ReportHistogram(
      CompositorFrameReporter::MissedFrameReportTypes report_type,
      FrameSequenceTrackerType intraction_type,
      const int stage_type_index,
      base::TimeDelta time_delta) const;

  // Returns true if the stage duration is greater than |kAbnormalityPercentile|
  // of its RollingTimeDeltaHistory.
  base::TimeDelta GetStateNormalUpperLimit(const StageData& stage) const;

  const bool is_single_threaded_;
  bool submitted_frame_missed_deadline_ = false;
  base::TimeTicks frame_termination_time_;
  FrameTerminationStatus frame_termination_status_ =
      FrameTerminationStatus::kUnknown;

  const base::flat_set<FrameSequenceTrackerType>* active_trackers_;

  // Indicates if work on Impl frame is finished.
  bool did_finish_impl_frame_ = false;
  // Indicates if main frame is aborted after begin.
  bool did_abort_main_frame_ = false;
  // The time that work on Impl frame is finished. It's only valid if the
  // reporter is in a stage other than begin impl frame.
  base::TimeTicks impl_frame_finish_time_;
};
}  // namespace cc

#endif  // CC_METRICS_COMPOSITOR_FRAME_REPORTER_H_"
