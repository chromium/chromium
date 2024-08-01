// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
#define CC_METRICS_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_

#include <map>
#include <memory>
#include <queue>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "cc/metrics/predictor_jank_tracker.h"
#include "cc/metrics/scroll_jank_dropped_frame_tracker.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ukm {
class UkmRecorder;
}

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
class DroppedFrameCounter;
class EventLatencyTracker;
struct BeginMainFrameMetrics;
struct FrameInfo;

// This is used for managing simultaneous CompositorFrameReporter instances
// in the case that the compositor has high latency. Calling one of the
// event functions will begin recording the time of the corresponding
// phase and trace it. If the frame is eventually submitted, then the
// recorded times of each phase will be reported in UMA.
// See CompositorFrameReporter.
class CC_EXPORT CompositorFrameReportingController {
 public:
  // Used as indices for accessing CompositorFrameReporters.
  enum PipelineStage {
    kBeginImplFrame = 0,
    kBeginMainFrame,
    kReadyToCommit,
    kCommit,
    kActivate,
    kNumPipelineStages
  };

  CompositorFrameReportingController(bool should_report_histograms,
                                     bool should_report_ukm,
                                     int layer_tree_host_id);
  virtual ~CompositorFrameReportingController();

  CompositorFrameReportingController(
      const CompositorFrameReportingController&) = delete;
  CompositorFrameReportingController& operator=(
      const CompositorFrameReportingController&) = delete;

  // Events to signal Beginning/Ending of phases.
  virtual void WillBeginImplFrame(const viz::BeginFrameArgs& args);
  virtual void WillBeginMainFrame(const viz::BeginFrameArgs& args);
  virtual void BeginMainFrameAborted(const viz::BeginFrameId& id,
                                     CommitEarlyOutReason reason);
  virtual void WillInvalidateOnImplSide();
  virtual void WillCommit();
  virtual void DidCommit();
  virtual void WillActivate();
  virtual void DidActivate();
  virtual void DidSubmitCompositorFrame(
      SubmitInfo& submit_info,
      const viz::BeginFrameId& current_frame_id,
      const viz::BeginFrameId& last_activated_frame_id);
  virtual void DidNotProduceFrame(const viz::BeginFrameId& id,
                                  FrameSkippedReason skip_reason);
  virtual void OnFinishImplFrame(const viz::BeginFrameId& id);
  virtual void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details);
  void OnStoppedRequestingBeginFrames();

  void NotifyReadyToCommit(std::unique_ptr<BeginMainFrameMetrics> details);

  void InitializeUkmManager(std::unique_ptr<ukm::UkmRecorder> recorder);
  void SetSourceId(ukm::SourceId source_id);

  void AddActiveTracker(FrameSequenceTrackerType type);
  void RemoveActiveTracker(FrameSequenceTrackerType type);
  void SetScrollingThread(FrameInfo::SmoothEffectDrivingThread thread);

  void SetThreadAffectsSmoothness(
      FrameInfo::SmoothEffectDrivingThread thread_type,
      bool affects_smoothness);

  void set_tick_clock(const base::TickClock* tick_clock) {
    DCHECK(tick_clock);
    tick_clock_ = tick_clock;
  }

  std::unique_ptr<CompositorFrameReporter>* reporters() { return reporters_; }

  void SetDroppedFrameCounter(DroppedFrameCounter* counter);

  void SetFrameSequenceTrackerCollection(
      FrameSequenceTrackerCollection* frame_sequence_trackers) {
    global_trackers_.frame_sequence_trackers = frame_sequence_trackers;
  }

  void set_event_latency_tracker(EventLatencyTracker* event_latency_tracker) {
    global_trackers_.event_latency_tracker = event_latency_tracker;
  }

  void BeginMainFrameStarted(base::TimeTicks begin_main_frame_start_time) {
    begin_main_frame_start_time_ = begin_main_frame_start_time;
  }

  bool HasReporterAt(PipelineStage stage) const;

  void SetVisible(bool visible);

 protected:
  struct SubmittedCompositorFrame {
    uint32_t frame_token;
    std::unique_ptr<CompositorFrameReporter> reporter;
    SubmittedCompositorFrame();
    SubmittedCompositorFrame(uint32_t frame_token,
                             std::unique_ptr<CompositorFrameReporter> reporter);
    SubmittedCompositorFrame(SubmittedCompositorFrame&& other);
    ~SubmittedCompositorFrame();
  };
  base::TimeTicks Now() const;

  bool next_activate_has_invalidation() const {
    return next_activate_has_invalidation_;
  }

 private:
  void AdvanceReporterStage(PipelineStage start, PipelineStage target);
  bool CanSubmitImplFrame(const viz::BeginFrameId& id) const;
  bool CanSubmitMainFrame(const viz::BeginFrameId& id) const;
  std::unique_ptr<CompositorFrameReporter> RestoreReporterAtBeginImpl(
      const viz::BeginFrameId& id);
  CompositorFrameReporter::SmoothThread GetSmoothThread() const;
  CompositorFrameReporter::SmoothThread GetSmoothThreadAtTime(
      base::TimeTicks timestamp) const;

  // Checks whether there are reporters containing updates from the main
  // thread, and returns a pointer to that reporter (if any). Otherwise
  // returns nullptr.
  CompositorFrameReporter* GetOutstandingUpdatesFromMain(
      const viz::BeginFrameId& id) const;

  // If the display-compositor skips over some frames (e.g. when the gpu is
  // busy, or the client is non-responsive), then it will not issue any
  // |BeginFrameArgs| for those frames. However, |CompositorFrameReporter|
  // instances should still be created for these frames. The following
  // functions accomplish this.
  void ProcessSkippedFramesIfNecessary(const viz::BeginFrameArgs& args);
  void MaybePassEventMetricsFromDroppedFrames(
      CompositorFrameReporter& reporter,
      uint32_t frame_token,
      bool next_reporter_from_same_frame);
  void StoreEventMetricsFromDroppedFrames(CompositorFrameReporter& reporter,
                                          uint32_t frame_token);
  void CreateReportersForDroppedFrames(
      const viz::BeginFrameArgs& old_args,
      const viz::BeginFrameArgs& new_args) const;

  // The arg is a reference to the unique_ptr, because depending on the state
  // that reporter is in, its ownership might be pass or not.
  void SetPartialUpdateDeciderWhenWaitingOnMain(
      std::unique_ptr<CompositorFrameReporter>& reporter);

  void AddSortedFrame(const viz::BeginFrameArgs& args,
                      const FrameInfo& frame_info);

  const bool should_report_histograms_;
  const int layer_tree_host_id_;

  viz::BeginFrameId last_submitted_frame_id_;

  bool next_activate_has_invalidation_ = false;
  ActiveTrackers active_trackers_;
  FrameInfo::SmoothEffectDrivingThread scrolling_thread_ =
      FrameInfo::SmoothEffectDrivingThread::kUnknown;

  bool is_compositor_thread_driving_smoothness_ = false;
  bool is_main_thread_driving_smoothness_ = false;
  // Sorted history of smooththread. Element i indicating the smooththread
  // from timestamp of element i-1 until timestamp of element i.
  std::map<base::TimeTicks, CompositorFrameReporter::SmoothThread>
      smooth_thread_history_;

  // Must outlive `reporters_` and `submitted_compositor_frames_` (which also
  // have reporters), since destroying the reporters can flush frames to
  // `global_trackers_`.
  GlobalMetricsTrackers global_trackers_;

  // The latency reporter passed to each CompositorFrameReporter. Owned here
  // because it must be common among all reporters.
  // DO NOT reorder this line and the ones below. The latency_ukm_reporter_
  // must outlive the objects in |submitted_compositor_frames_|.
  std::unique_ptr<LatencyUkmReporter> latency_ukm_reporter_;
  std::unique_ptr<PredictorJankTracker> predictor_jank_tracker_;
  std::unique_ptr<ScrollJankDroppedFrameTracker>
      scroll_jank_dropped_frame_tracker_;
  std::unique_ptr<ScrollJankUkmReporter> scroll_jank_ukm_reporter_;

  std::unique_ptr<CompositorFrameReporter>
      reporters_[PipelineStage::kNumPipelineStages];

  // Mapping of frame token to pipeline reporter for submitted compositor
  // frames.
  // DO NOT reorder this line and the one above. The latency_ukm_reporter_
  // must outlive the objects in |submitted_compositor_frames_|.
  base::circular_deque<SubmittedCompositorFrame> submitted_compositor_frames_;

  // Contains information about the latest frame that was started, and the state
  // during that frame. This is used to process skipped frames, as well as
  // making sure a CompositorFrameReporter object for a delayed main-frame is
  // created with the correct state.
  struct {
    viz::BeginFrameArgs args;
    FrameInfo::SmoothEffectDrivingThread scrolling_thread =
        FrameInfo::SmoothEffectDrivingThread::kUnknown;
    ActiveTrackers active_trackers;
    CompositorFrameReporter::SmoothThread smooth_thread =
        CompositorFrameReporter::SmoothThread::kSmoothNone;
  } last_started_compositor_frame_;

  base::TimeTicks begin_main_frame_start_time_;

  raw_ptr<const base::TickClock> tick_clock_ =
      base::DefaultTickClock::GetInstance();

  // When a frame with events metrics fails to be presented, its events metrics
  // will be added to this map. The first following presented frame will get
  // these metrics and report them. The key of map is submission frame token.
  // Frame token is chosen over BeginFrameId as key due to the fact that frames
  // can drop while a long running main still eventually presents, in which
  // cases its more appropriate to check against frame_token instead of
  // BeginFrameId.
  std::map<uint32_t, EventMetricsSet> events_metrics_from_dropped_frames_;

  CompositorFrameReporter::CompositorLatencyInfo
      previous_latency_predictions_main_;
  CompositorFrameReporter::CompositorLatencyInfo
      previous_latency_predictions_impl_;

  // Container that stores the EventLatency stage latency predictions based on
  // previous event traces.
  CompositorFrameReporter::EventLatencyInfo event_latency_predictions_;

  // Reporting controller needs to track transition of the page from invisible
  // to visible in order to discard EventsMetrics impacted by duration of page
  // being invisible
  bool visible_ = true;
  bool waiting_for_did_present_after_visible_ = false;
};

}  // namespace cc

#endif  // CC_METRICS_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
