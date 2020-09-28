// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_COMPOSITOR_FRAME_REPORTER_H_
#define CC_METRICS_COMPOSITOR_FRAME_REPORTER_H_

#include <bitset>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/cc_export.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "cc/scheduler/scheduler.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_timing_details.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
class DroppedFrameCounter;
class LatencyUkmReporter;

// This is used for tracing and reporting the duration of pipeline stages within
// a single frame.
//
// For each stage in the frame pipeline, calling StartStage will start tracing
// that stage (and end any currently running stages).
//
// If the tracked frame is presented (i.e. the frame termination status is
// kPresentedFrame), then the duration of each stage along with the total
// latency will be reported to UMA. If the tracked frame is not presented (i.e.
// the frame termination status is kDidNotPresentFrame or
// kReplacedByNewReporter), then the duration is reported under
// CompositorLatency.DroppedFrame.*.
// The format of each stage reported to UMA is
// "CompositorLatency.[DroppedFrame.][Interaction_name.].<StageName>".
class CC_EXPORT CompositorFrameReporter {
 public:
  enum class FrameTerminationStatus {
    // The tracked compositor frame was presented.
    kPresentedFrame,

    // The tracked compositor frame was submitted to the display compositor but
    // was not presented.
    kDidNotPresentFrame,

    // Reporter that is currently at a stage is replaced by a new one (e.g. two
    // BeginImplFrames can happen without issuing BeginMainFrame, so the first
    // reporter would terminate with this status).
    kReplacedByNewReporter,

    // Frame that was being tracked did not end up being submitting (e.g. frame
    // had no damage or LTHI was ended).
    kDidNotProduceFrame,

    // Default termination status. Should not be reachable.
    kUnknown
  };

  // These values are used for indexing the UMA histograms.
  enum class FrameReportType {
    kNonDroppedFrame = 0,
    kMissedDeadlineFrame = 1,
    kDroppedFrame = 2,
    kCompositorOnlyFrame = 3,
    kMaxValue = kCompositorOnlyFrame
  };

  // These values are used for indexing the UMA histograms.
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
    kStartDrawToSwapStart = 2,
    kSwapStartToSwapEnd = 3,
    kSwapEndToPresentationCompositorFrame = 4,

    // This is a breakdown of SwapStartToSwapEnd stage which is optionally
    // recorded if querying these timestamps is supported by the platform.
    kSwapStartToBufferAvailable = 5,
    kBufferAvailableToBufferReady = 6,
    kBufferReadyToLatch = 7,
    kLatchToSwapEnd = 8,
    kBreakdownCount
  };

  enum class BlinkBreakdown {
    kHandleInputEvents = 0,
    kAnimate = 1,
    kStyleUpdate = 2,
    kLayoutUpdate = 3,
    kPrepaint = 4,
    kCompositingInputs = 5,
    kCompositingAssignments = 6,
    kPaint = 7,
    kScrollingCoordinator = 8,
    kCompositeCommit = 9,
    kUpdateLayers = 10,
    kBeginMainSentToStarted = 11,
    kBreakdownCount
  };

  struct CC_EXPORT StageData {
    StageType stage_type;
    base::TimeTicks start_time;
    base::TimeTicks end_time;
    StageData();
    StageData(StageType stage_type,
              base::TimeTicks start_time,
              base::TimeTicks end_time);
    StageData(const StageData&);
    ~StageData();
  };

  enum SmoothThread {
    kSmoothNone,
    kSmoothCompositor,
    kSmoothMain,
    kSmoothBoth
  };

  using ActiveTrackers =
      std::bitset<static_cast<size_t>(FrameSequenceTrackerType::kMaxType)>;

  CompositorFrameReporter(const ActiveTrackers& active_trackers,
                          const viz::BeginFrameArgs& args,
                          LatencyUkmReporter* latency_ukm_reporter,
                          bool should_report_metrics,
                          SmoothThread smooth_thread,
                          int layer_tree_host_id);
  ~CompositorFrameReporter();

  CompositorFrameReporter(const CompositorFrameReporter& reporter) = delete;
  CompositorFrameReporter& operator=(const CompositorFrameReporter& reporter) =
      delete;

  std::unique_ptr<CompositorFrameReporter> CopyReporterAtBeginImplStage();

  // Note that the started stage may be reported to UMA. If the histogram is
  // intended to be reported then the histograms.xml file must be updated too.
  void StartStage(StageType stage_type, base::TimeTicks start_time);
  void TerminateFrame(FrameTerminationStatus termination_status,
                      base::TimeTicks termination_time);
  void SetBlinkBreakdown(std::unique_ptr<BeginMainFrameMetrics> blink_breakdown,
                         base::TimeTicks begin_main_start);
  void SetVizBreakdown(const viz::FrameTimingDetails& viz_breakdown);
  void SetEventsMetrics(std::vector<EventMetrics> events_metrics);

  int StageHistorySizeForTesting() { return stage_history_.size(); }

  void OnFinishImplFrame(base::TimeTicks timestamp);
  void OnAbortBeginMainFrame(base::TimeTicks timestamp);
  void OnDidNotProduceFrame(FrameSkippedReason skip_reason);
  void EnableCompositorOnlyReporting();
  bool did_finish_impl_frame() const { return did_finish_impl_frame_; }
  base::TimeTicks impl_frame_finish_time() const {
    return impl_frame_finish_time_;
  }

  bool did_not_produce_frame() const {
    return did_not_produce_frame_time_.has_value();
  }
  base::TimeTicks did_not_produce_frame_time() const {
    return *did_not_produce_frame_time_;
  }

  bool did_abort_main_frame() const {
    return main_frame_abort_time_.has_value();
  }
  base::TimeTicks main_frame_abort_time() const {
    return *main_frame_abort_time_;
  }

  FrameSkippedReason frame_skip_reason() const { return *frame_skip_reason_; }

  void set_tick_clock(const base::TickClock* tick_clock) {
    DCHECK(tick_clock);
    tick_clock_ = tick_clock;
  }

  void SetDroppedFrameCounter(DroppedFrameCounter* counter) {
    dropped_frame_counter_ = counter;
  }

  bool has_partial_update() const { return has_partial_update_; }
  void set_has_partial_update(bool has_partial_update) {
    has_partial_update_ = has_partial_update;
  }

  const viz::BeginFrameId& frame_id() const { return args_.frame_id; }

  // Adopts |cloned_reporter|, i.e. keeps |cloned_reporter| alive until after
  // this reporter terminates. Note that the |cloned_reporter| must have been
  // created from this reporter using |CopyReporterAtBeginImplStage()|.
  void AdoptReporter(std::unique_ptr<CompositorFrameReporter> cloned_reporter);

  // If this is a cloned reporter, then this returns a weak-ptr to the original
  // reporter this was cloned from (using |CopyReporterAtBeginImplStage()|).
  base::WeakPtr<CompositorFrameReporter> cloned_from() { return cloned_from_; }

 protected:
  base::WeakPtr<CompositorFrameReporter> GetWeakPtr();

 private:
  void TerminateReporter();
  void EndCurrentStage(base::TimeTicks end_time);

  void ReportCompositorLatencyHistograms() const;
  void ReportStageHistogramWithBreakdown(
      const StageData& stage,
      FrameSequenceTrackerType frame_sequence_tracker_type =
          FrameSequenceTrackerType::kMaxType) const;
  void ReportCompositorLatencyBlinkBreakdowns(
      FrameSequenceTrackerType frame_sequence_tracker_type) const;
  void ReportCompositorLatencyVizBreakdowns(
      FrameSequenceTrackerType frame_sequence_tracker_type) const;
  void ReportCompositorLatencyHistogram(
      FrameSequenceTrackerType intraction_type,
      const int stage_type_index,
      base::TimeDelta time_delta) const;

  void ReportEventLatencyHistograms() const;
  void ReportEventLatencyBlinkBreakdowns(
      int histogram_base_index,
      const std::string& histogram_base_name) const;
  void ReportEventLatencyVizBreakdowns(
      int histogram_base_index,
      const std::string& histogram_base_name) const;
  void ReportEventLatencyHistogram(int histogram_base_index,
                                   const std::string& histogram_base_name,
                                   int stage_type_index,
                                   base::TimeDelta latency) const;

  void ReportCompositorLatencyTraceEvents() const;
  void ReportEventLatencyTraceEvents() const;

  void EnableReportType(FrameReportType report_type) {
    report_types_.set(static_cast<size_t>(report_type));
  }
  bool TestReportType(FrameReportType report_type) const {
    return report_types_.test(static_cast<size_t>(report_type));
  }

  void PopulateBlinkBreakdownList();
  void PopulateVizBreakdownList();

  // This method is only used for DCheck
  base::TimeDelta SumOfStageHistory() const;

  base::TimeTicks Now() const;

  bool IsDroppedFrameAffectingSmoothness() const;

  const bool should_report_metrics_;
  const viz::BeginFrameArgs args_;

  StageData current_stage_;

  BeginMainFrameMetrics blink_breakdown_;
  base::TimeTicks blink_start_time_;
  base::TimeDelta
      blink_breakdown_list_[static_cast<int>(BlinkBreakdown::kBreakdownCount)];

  viz::FrameTimingDetails viz_breakdown_;
  base::TimeTicks viz_start_time_;
  base::Optional<std::pair<base::TimeTicks, base::TimeTicks>>
      viz_breakdown_list_[static_cast<int>(VizBreakdown::kBreakdownCount)];

  // Stage data is recorded here. On destruction these stages will be reported
  // to UMA if the termination status is |kPresentedFrame|. Reported data will
  // be divided based on the frame submission status.
  std::vector<StageData> stage_history_;

  // List of metrics for events affecting this frame.
  std::vector<EventMetrics> events_metrics_;

  std::bitset<static_cast<size_t>(FrameReportType::kMaxValue) + 1>
      report_types_;

  base::TimeTicks frame_termination_time_;
  base::TimeTicks begin_main_frame_start_;
  FrameTerminationStatus frame_termination_status_ =
      FrameTerminationStatus::kUnknown;

  const ActiveTrackers active_trackers_;

  LatencyUkmReporter* latency_ukm_reporter_;

  // Indicates if work on Impl frame is finished.
  bool did_finish_impl_frame_ = false;
  // The time that work on Impl frame is finished. It's only valid if the
  // reporter is in a stage other than begin impl frame.
  base::TimeTicks impl_frame_finish_time_;

  // The timestamp of when the frame was marked as not having produced a frame
  // (through a call to DidNotProduceFrame()).
  base::Optional<base::TimeTicks> did_not_produce_frame_time_;
  base::Optional<FrameSkippedReason> frame_skip_reason_;
  base::Optional<base::TimeTicks> main_frame_abort_time_;

  const base::TickClock* tick_clock_ = base::DefaultTickClock::GetInstance();

  DroppedFrameCounter* dropped_frame_counter_ = nullptr;
  bool has_partial_update_ = false;

  const SmoothThread smooth_thread_;
  const int layer_tree_host_id_;

  // If this is a cloned pointer, then |cloned_from_| is a weak pointer to the
  // original reporter this was cloned from.
  base::WeakPtr<CompositorFrameReporter> cloned_from_;

  // If this reporter was cloned, then |cloned_to_| is a weak pointer to the
  // cloned repoter.
  base::WeakPtr<CompositorFrameReporter> cloned_to_;

  // A cloned reporter is not originally owned by the original reporter.
  // However, it can 'adopt' it (using |AdoptReporter()| if the cloned reporter
  // needs to stay alive until the original reporter terminates.
  std::unique_ptr<CompositorFrameReporter> own_cloned_to_;

  base::WeakPtrFactory<CompositorFrameReporter> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_METRICS_COMPOSITOR_FRAME_REPORTER_H_"
