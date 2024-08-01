// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/metrics/compositor_frame_reporting_controller.h"

#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/dropped_frame_counter.h"
#include "cc/metrics/event_latency_tracing_recorder.h"
#include "cc/metrics/frame_sequence_tracker_collection.h"
#include "cc/metrics/latency_ukm_reporter.h"
#include "cc/metrics/scroll_jank_dropped_frame_tracker.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/tracing/public/cpp/perfetto/macros.h"

namespace cc {
namespace {
using SmoothThread = CompositorFrameReporter::SmoothThread;
using StageType = CompositorFrameReporter::StageType;
using FrameTerminationStatus = CompositorFrameReporter::FrameTerminationStatus;

constexpr int kNumOfCompositorStages =
    static_cast<int>(StageType::kStageTypeCount) - 1;
constexpr int kNumDispatchStages =
    static_cast<int>(EventMetrics::DispatchStage::kMaxValue);
constexpr base::TimeDelta kDefaultLatencyPredictionDeviationThreshold =
    viz::BeginFrameArgs::DefaultInterval() / 2;
}  // namespace

CompositorFrameReportingController::CompositorFrameReportingController(
    bool should_report_histograms,
    bool should_report_ukm,
    int layer_tree_host_id)
    : should_report_histograms_(should_report_histograms),
      layer_tree_host_id_(layer_tree_host_id),
      latency_ukm_reporter_(std::make_unique<LatencyUkmReporter>()),
      predictor_jank_tracker_(std::make_unique<PredictorJankTracker>()),
      scroll_jank_dropped_frame_tracker_(
          std::make_unique<ScrollJankDroppedFrameTracker>()),
      scroll_jank_ukm_reporter_(std::make_unique<ScrollJankUkmReporter>()),
      previous_latency_predictions_main_(base::Microseconds(-1)),
      previous_latency_predictions_impl_(base::Microseconds(-1)),
      event_latency_predictions_(
          CompositorFrameReporter::EventLatencyInfo(kNumDispatchStages,
                                                    kNumOfCompositorStages)) {
  if (should_report_ukm) {
    // UKM metrics should be reported if and only if `latency_ukm_reporter` is
    // set on `global_trackers_`.
    global_trackers_.latency_ukm_reporter = latency_ukm_reporter_.get();

    global_trackers_.scroll_jank_ukm_reporter = scroll_jank_ukm_reporter_.get();
    predictor_jank_tracker_->set_scroll_jank_ukm_reporter(
        scroll_jank_ukm_reporter_.get());
    scroll_jank_dropped_frame_tracker_->set_scroll_jank_ukm_reporter(
        scroll_jank_ukm_reporter_.get());
  }
  global_trackers_.predictor_jank_tracker = predictor_jank_tracker_.get();
  global_trackers_.scroll_jank_dropped_frame_tracker =
      scroll_jank_dropped_frame_tracker_.get();
}

CompositorFrameReportingController::~CompositorFrameReportingController() {
  base::TimeTicks now = Now();
  for (int i = 0; i < PipelineStage::kNumPipelineStages; ++i) {
    if (reporters_[i]) {
      reporters_[i]->TerminateFrame(FrameTerminationStatus::kDidNotProduceFrame,
                                    now);
    }
  }
  for (auto& pair : submitted_compositor_frames_) {
    pair.reporter->TerminateFrame(FrameTerminationStatus::kDidNotPresentFrame,
                                  Now());
  }

  predictor_jank_tracker_->set_scroll_jank_ukm_reporter(nullptr);
  scroll_jank_dropped_frame_tracker_->set_scroll_jank_ukm_reporter(nullptr);
}

void CompositorFrameReportingController::SetVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }

  visible_ = visible;
  if (visible_) {
    // Note:`waiting_for_did_present_after_visible_` will be set to false
    // inside `CompositorFrameReportingController::DidPresentCompositorFrame`
    // after `events_metrics_from_dropped_frames_` is clear
    waiting_for_did_present_after_visible_ = true;
  }
}

CompositorFrameReportingController::SubmittedCompositorFrame::
    SubmittedCompositorFrame() = default;
CompositorFrameReportingController::SubmittedCompositorFrame::
    SubmittedCompositorFrame(uint32_t frame_token,
                             std::unique_ptr<CompositorFrameReporter> reporter)
    : frame_token(frame_token), reporter(std::move(reporter)) {}
CompositorFrameReportingController::SubmittedCompositorFrame::
    ~SubmittedCompositorFrame() = default;

CompositorFrameReportingController::SubmittedCompositorFrame::
    SubmittedCompositorFrame(SubmittedCompositorFrame&& other) = default;

base::TimeTicks CompositorFrameReportingController::Now() const {
  return tick_clock_->NowTicks();
}

bool CompositorFrameReportingController::HasReporterAt(
    PipelineStage stage) const {
  return !!reporters_[stage].get();
}

void CompositorFrameReportingController::ProcessSkippedFramesIfNecessary(
    const viz::BeginFrameArgs& args) {
  const auto& previous_frame = last_started_compositor_frame_.args;
  if (previous_frame.IsValid() &&
      previous_frame.frame_id.source_id == args.frame_id.source_id) {
    CreateReportersForDroppedFrames(previous_frame, args);
  }

  last_started_compositor_frame_.args = args;
  last_started_compositor_frame_.scrolling_thread = scrolling_thread_;
  last_started_compositor_frame_.active_trackers = active_trackers_;
  last_started_compositor_frame_.smooth_thread = GetSmoothThread();
}

void CompositorFrameReportingController::WillBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  ProcessSkippedFramesIfNecessary(args);

  base::TimeTicks begin_time = Now();
  if (reporters_[PipelineStage::kBeginImplFrame]) {
    auto& reporter = reporters_[PipelineStage::kBeginImplFrame];
    DCHECK(reporter->did_finish_impl_frame());
    // TODO(crbug.com/40728802): This is a speculative fix. This code should
    // only be reached after the previous frame have been explicitly marked as
    // 'did not produce frame', i.e. this code should have a DCHECK instead of a
    // conditional:
    //   DCHECK(reporter->did_not_produce_frame()).
    if (reporter->did_not_produce_frame()) {
      reporter->TerminateFrame(FrameTerminationStatus::kDidNotProduceFrame,
                               reporter->did_not_produce_frame_time());
    } else {
      reporter->TerminateFrame(FrameTerminationStatus::kReplacedByNewReporter,
                               Now());
    }
  }
  auto reporter = std::make_unique<CompositorFrameReporter>(
      active_trackers_, args, should_report_histograms_, GetSmoothThread(),
      scrolling_thread_, layer_tree_host_id_, global_trackers_);
  reporter->set_tick_clock(tick_clock_);
  reporter->StartStage(StageType::kBeginImplFrameToSendBeginMainFrame,
                       begin_time);
  reporters_[PipelineStage::kBeginImplFrame] = std::move(reporter);
}

void CompositorFrameReportingController::WillBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  if (reporters_[PipelineStage::kBeginImplFrame]) {
    // We need to use .get() below because operator<< in std::unique_ptr is a
    // C++20 feature.
    DCHECK_NE(reporters_[PipelineStage::kBeginMainFrame].get(),
              reporters_[PipelineStage::kBeginImplFrame].get());
    DCHECK_EQ(reporters_[PipelineStage::kBeginImplFrame]->frame_id(),
              args.frame_id);
    reporters_[PipelineStage::kBeginImplFrame]->StartStage(
        StageType::kSendBeginMainFrameToCommit, Now());
    AdvanceReporterStage(PipelineStage::kBeginImplFrame,
                         PipelineStage::kBeginMainFrame);
  } else {
    // In this case we have already submitted the ImplFrame, but we received
    // beginMain frame before next BeginImplFrame (Not reached the ImplFrame
    // deadline yet). So will start a new reporter at BeginMainFrame, and use
    // the state(s) from the ImplFrame where necessary.
    auto scrolling_thread = scrolling_thread_;
    auto active_trackers = active_trackers_;
    auto smooth_thread = GetSmoothThread();
    if (args.frame_id == last_started_compositor_frame_.args.frame_id) {
      // TODO(crbug.com/40207819): Instead of replacing all current information
      // with the older information from when the impl-frame started, merge the
      // two sets of information that makes sense.
      scrolling_thread = last_started_compositor_frame_.scrolling_thread;
      active_trackers = last_started_compositor_frame_.active_trackers;
      smooth_thread = last_started_compositor_frame_.smooth_thread;
    }
    auto reporter = std::make_unique<CompositorFrameReporter>(
        active_trackers, args, should_report_histograms_, smooth_thread,
        scrolling_thread, layer_tree_host_id_, global_trackers_);
    reporter->set_tick_clock(tick_clock_);
    reporter->StartStage(StageType::kSendBeginMainFrameToCommit, Now());
    reporters_[PipelineStage::kBeginMainFrame] = std::move(reporter);
  }
}

void CompositorFrameReportingController::BeginMainFrameAborted(
    const viz::BeginFrameId& id,
    CommitEarlyOutReason reason) {
  auto& reporter = reporters_[PipelineStage::kBeginMainFrame];
  DCHECK(reporter);
  DCHECK_EQ(reporter->frame_id(), id);
  reporter->OnAbortBeginMainFrame(Now());

  if (reason == CommitEarlyOutReason::kFinishedNoUpdates) {
    DidNotProduceFrame(id, FrameSkippedReason::kNoDamage);
  }
}

void CompositorFrameReportingController::WillCommit() {
  DCHECK(reporters_[PipelineStage::kReadyToCommit]);
  reporters_[PipelineStage::kReadyToCommit]->StartStage(StageType::kCommit,
                                                        Now());
}

void CompositorFrameReportingController::DidCommit() {
  DCHECK(reporters_[PipelineStage::kReadyToCommit]);
  reporters_[PipelineStage::kReadyToCommit]->StartStage(
      StageType::kEndCommitToActivation, Now());
  AdvanceReporterStage(PipelineStage::kReadyToCommit, PipelineStage::kCommit);
}

void CompositorFrameReportingController::WillInvalidateOnImplSide() {
  // Allows for activation without committing.
  // TODO(alsan): Report latency of impl side invalidations.
  next_activate_has_invalidation_ = true;
}

void CompositorFrameReportingController::WillActivate() {
  DCHECK(reporters_[PipelineStage::kCommit] || next_activate_has_invalidation_);
  if (!reporters_[PipelineStage::kCommit])
    return;
  reporters_[PipelineStage::kCommit]->StartStage(StageType::kActivation, Now());
}

void CompositorFrameReportingController::DidActivate() {
  DCHECK(reporters_[PipelineStage::kCommit] || next_activate_has_invalidation_);
  next_activate_has_invalidation_ = false;
  if (!reporters_[PipelineStage::kCommit])
    return;
  reporters_[PipelineStage::kCommit]->StartStage(
      StageType::kEndActivateToSubmitCompositorFrame, Now());
  AdvanceReporterStage(PipelineStage::kCommit, PipelineStage::kActivate);
}

void CompositorFrameReportingController::DidSubmitCompositorFrame(
    SubmitInfo& submit_info,
    const viz::BeginFrameId& current_frame_id,
    const viz::BeginFrameId& last_activated_frame_id) {
  bool is_activated_frame_new =
      (last_activated_frame_id != last_submitted_frame_id_);

  // It is possible to submit a CompositorFrame containing outputs from two
  // different begin-frames: an begin-main-frame that was blocked on the
  // main-thread, and another one for the compositor thread.
  std::unique_ptr<CompositorFrameReporter> main_reporter;
  std::unique_ptr<CompositorFrameReporter> impl_reporter;

  // If |is_activated_frame_new| is true, |main_reporter| is guaranteed to
  // be set, and |impl_reporter| may or may not be set; otherwise,
  // |impl_reporter| is guaranteed to be set, and |main_reporter| will not be
  // set.
  if (is_activated_frame_new) {
    DCHECK_EQ(reporters_[PipelineStage::kActivate]->frame_id(),
              last_activated_frame_id);
    // The reporter in activate state can be submitted
    main_reporter = std::move(reporters_[PipelineStage::kActivate]);
    last_submitted_frame_id_ = last_activated_frame_id;
  } else {
    DCHECK(!reporters_[PipelineStage::kActivate]);
  }

  // |main_reporter| can be for a previous BeginFrameArgs (i.e. not for
  // |current_frame_id|), in which case it is necessary to also report metrics
  // for the reporter representing |current_frame_id|. Following are the
  // possibilities:
  //  1) the main-thread did not request any updates (i.e. a 'begin main frame'
  //     was not issued). The reporter for |current_frame_id| should still be in
  //     the 'impl frame' stage.
  //  2) the 'begin main frame' was issued, but the main-thread did not have any
  //     updates (i.e. the 'begin main frame' was aborted). The reporter for
  //     |current_frame_id| should be in the 'main frame' stage, and it will
  //     have been aborted.
  //  3) main-thread is still processing 'begin main frame'. The reporter for
  //     |current_frame_id| should be in either the 'main frame' or 'commit'
  //     stage.
  if (CanSubmitImplFrame(current_frame_id)) {
    auto& reporter = reporters_[PipelineStage::kBeginImplFrame];
    reporter->StartStage(StageType::kEndActivateToSubmitCompositorFrame,
                         reporter->impl_frame_finish_time());
    AdvanceReporterStage(PipelineStage::kBeginImplFrame,
                         PipelineStage::kActivate);
    impl_reporter = std::move(reporters_[PipelineStage::kActivate]);
    CompositorFrameReporter* partial_update_decider =
        GetOutstandingUpdatesFromMain(current_frame_id);
    if (partial_update_decider)
      impl_reporter->SetPartialUpdateDecider(partial_update_decider);
  } else if (CanSubmitMainFrame(current_frame_id)) {
    auto& reporter = reporters_[PipelineStage::kBeginMainFrame];
    reporter->StartStage(StageType::kEndActivateToSubmitCompositorFrame,
                         reporter->impl_frame_finish_time());
    AdvanceReporterStage(PipelineStage::kBeginMainFrame,
                         PipelineStage::kActivate);
    impl_reporter = std::move(reporters_[PipelineStage::kActivate]);
  } else {
    auto reporter = RestoreReporterAtBeginImpl(current_frame_id);
    // The method will return nullptr if Impl reporter has been submitted
    // prior to BeginMainFrame.
    if (reporter) {
      reporter->StartStage(StageType::kEndActivateToSubmitCompositorFrame,
                           reporter->impl_frame_finish_time());
      impl_reporter = std::move(reporter);
    }
  }

#if DCHECK_IS_ON()
  if (!submit_info.events_metrics.main_event_metrics.empty()) {
    DCHECK(main_reporter);
  }

  if (impl_reporter) {
    DCHECK_EQ(impl_reporter->frame_id(), current_frame_id);
    if (main_reporter) {
      DCHECK_NE(main_reporter->frame_id(), current_frame_id);
    }
  }
#endif

  // When |impl_reporter| does not exist, but there are still impl-side metrics,
  // merge the main and impl metrics and pass the combined vector into
  // |main_reporter|.
  if (!impl_reporter &&
      !submit_info.events_metrics.impl_event_metrics.empty()) {
    DCHECK(main_reporter);
    // If there are impl events, there must be a reporter with
    // |current_frame_id|.
    DCHECK_EQ(main_reporter->frame_id(), current_frame_id);
    submit_info.events_metrics.main_event_metrics.reserve(
        submit_info.events_metrics.main_event_metrics.size() +
        submit_info.events_metrics.impl_event_metrics.size());
    submit_info.events_metrics.main_event_metrics.insert(
        submit_info.events_metrics.main_event_metrics.end(),
        std::make_move_iterator(
            submit_info.events_metrics.impl_event_metrics.begin()),
        std::make_move_iterator(
            submit_info.events_metrics.impl_event_metrics.end()));
  }

  if (main_reporter) {
    main_reporter->StartStage(
        StageType::kSubmitCompositorFrameToPresentationCompositorFrame,
        submit_info.time);
    main_reporter->AddEventsMetrics(
        std::move(submit_info.events_metrics.main_event_metrics));
    main_reporter->set_checkerboarded_needs_raster(
        submit_info.checkerboarded_needs_raster);
    main_reporter->set_checkerboarded_needs_record(
        submit_info.checkerboarded_needs_record);
    main_reporter->set_reporter_type_to_main();
    main_reporter->set_top_controls_moved(submit_info.top_controls_moved);
    submitted_compositor_frames_.emplace_back(submit_info.frame_token,
                                              std::move(main_reporter));
  }

  if (impl_reporter) {
    impl_reporter->EnableCompositorOnlyReporting();
    impl_reporter->StartStage(
        StageType::kSubmitCompositorFrameToPresentationCompositorFrame,
        submit_info.time);
    impl_reporter->AddEventsMetrics(
        std::move(submit_info.events_metrics.impl_event_metrics));
    impl_reporter->set_checkerboarded_needs_raster(
        submit_info.checkerboarded_needs_raster);
    impl_reporter->set_checkerboarded_needs_record(
        submit_info.checkerboarded_needs_record);
    impl_reporter->set_is_accompanied_by_main_thread_update(
        is_activated_frame_new);
    impl_reporter->set_reporter_type_to_impl();
    impl_reporter->set_top_controls_moved(submit_info.top_controls_moved);
    submitted_compositor_frames_.emplace_back(submit_info.frame_token,
                                              std::move(impl_reporter));
  }
}

void CompositorFrameReportingController::DidNotProduceFrame(
    const viz::BeginFrameId& id,
    FrameSkippedReason skip_reason) {
  for (auto& stage_reporter : reporters_) {
    if (stage_reporter && stage_reporter->frame_id() == id) {
      // The reporter will be flagged and terminated when replaced by another
      // reporter. The reporter is not terminated immediately here because it
      // can still end up producing a frame afterwards. For example, if the
      // compositor does not have any updates, and the main-thread takes too
      // long, then DidNotProduceFrame() is called for the reporter in the
      // BeginMain stage, but the main-thread can make updates, which can be
      // submitted with the next frame.
      stage_reporter->OnDidNotProduceFrame(skip_reason);
      if (skip_reason == FrameSkippedReason::kWaitingOnMain)
        SetPartialUpdateDeciderWhenWaitingOnMain(stage_reporter);

      break;
    }
  }
}

void CompositorFrameReportingController::
    SetPartialUpdateDeciderWhenWaitingOnMain(
        std::unique_ptr<CompositorFrameReporter>& stage_reporter) {
  // If the compositor has no updates, and the main-thread has not responded
  // to the begin-main-frame yet, then depending on main thread having
  // update or not this would be a NoFrameProduced or a DroppedFrame. To
  // handle this case , keep the reporter for the main-thread, but recreate
  // a reporter for the current frame and link it to the reporter it depends
  // on.
  auto reporter = RestoreReporterAtBeginImpl(stage_reporter->frame_id());
  if (reporter) {
    reporter->OnDidNotProduceFrame(FrameSkippedReason::kWaitingOnMain);
    reporter->TerminateFrame(FrameTerminationStatus::kDidNotProduceFrame,
                             Now());
    stage_reporter->AdoptReporter(std::move(reporter));
  } else {
    // The stage_reporter in this case was waiting for main, so needs to
    // be adopted by the reporter which is waiting on Main thread's work
    CompositorFrameReporter* partial_update_decider =
        GetOutstandingUpdatesFromMain(stage_reporter->frame_id());
    if (partial_update_decider) {
      stage_reporter->SetPartialUpdateDecider(partial_update_decider);
      stage_reporter->OnDidNotProduceFrame(FrameSkippedReason::kWaitingOnMain);
      stage_reporter->TerminateFrame(
          FrameTerminationStatus::kDidNotProduceFrame, Now());
      partial_update_decider->AdoptReporter(std::move(stage_reporter));
    }
  }
}

void CompositorFrameReportingController::OnFinishImplFrame(
    const viz::BeginFrameId& id) {
  for (auto& reporter : reporters_) {
    if (reporter && reporter->frame_id() == id) {
      reporter->OnFinishImplFrame(Now());
      return;
    }
  }
}

void CompositorFrameReportingController::MaybePassEventMetricsFromDroppedFrames(
    CompositorFrameReporter& reporter,
    uint32_t frame_token,
    bool next_reporter_from_same_frame) {
  // If there are outstanding metrics from dropped frames older than this
  // frame, this frame would be the first frame presented after those
  // dropped frames. So, this frame is the one presenting updates from those
  // frames to the user and should report metrics for them. Note that since
  // reporters for submitted but dropped frames are terminated before any
  // following frame being presented, all events metrics that should
  // potentially be included in this presented frame are already in
  // `events_metrics_from_dropped_frames_`.
  // The implicit assumption is that submitted_frame->frame_token will never
  // be equal to it->first
  if ((reporter.get_reporter_type() ==
           CompositorFrameReporter::ReporterType::kMain &&
       !next_reporter_from_same_frame) ||
      (reporter.get_reporter_type() ==
       CompositorFrameReporter::ReporterType::kImpl)) {
    // Just take all the events.
    for (auto it = events_metrics_from_dropped_frames_.begin();
         it != events_metrics_from_dropped_frames_.end() &&
         frame_token > it->first;
         it = events_metrics_from_dropped_frames_.erase(it)) {
      reporter.AddEventsMetrics(std::move(it->second.main_event_metrics));
      reporter.AddEventsMetrics(std::move(it->second.impl_event_metrics));
    }
  } else {
    // Main with accompanying impl - just take main events and don't remove them
    // from list, impl reporter will erase them.
    for (auto it = events_metrics_from_dropped_frames_.begin();
         it != events_metrics_from_dropped_frames_.end() &&
         frame_token > it->first;
         it++) {
      reporter.AddEventsMetrics(std::move(it->second.main_event_metrics));
    }
  }
}

void CompositorFrameReportingController::StoreEventMetricsFromDroppedFrames(
    CompositorFrameReporter& reporter,
    uint32_t frame_token) {
  // If the frame didn't end up being presented, keep its metrics around to
  // be reported with the first following presented frame.
  auto main_reporter_events_metrics = reporter.TakeMainBlockedEventsMetrics();
  auto remaining_reporter_events_metrics = reporter.TakeEventsMetrics();
  EventMetricsSet& frame_events_metrics =
      events_metrics_from_dropped_frames_[frame_token];

  switch (reporter.get_reporter_type()) {
    case CompositorFrameReporter::ReporterType::kImpl:
      // Impl events marked with `requires_main_thread` go to main list.
      frame_events_metrics.main_event_metrics.insert(
          frame_events_metrics.main_event_metrics.end(),
          std::make_move_iterator(main_reporter_events_metrics.begin()),
          std::make_move_iterator(main_reporter_events_metrics.end()));
      // Remaining events go to impl list.
      frame_events_metrics.impl_event_metrics.insert(
          frame_events_metrics.impl_event_metrics.end(),
          std::make_move_iterator(remaining_reporter_events_metrics.begin()),
          std::make_move_iterator(remaining_reporter_events_metrics.end()));
      break;
    case CompositorFrameReporter::ReporterType::kMain:
      // TODO(b/291088394): Handle events from a "main" reporter that is marked
      // as main because it had updates from the latest begin main frame. In
      // such a case even when scrolling on impl the events will go to the
      // "main" reporter and then if the frame corresponding to this gets
      // dropped then the events would actually be passed to a main reporter
      // instead of impl reporter if it was present. All events from main
      // reporter go to main events list.
      frame_events_metrics.main_event_metrics.insert(
          frame_events_metrics.main_event_metrics.end(),
          std::make_move_iterator(main_reporter_events_metrics.begin()),
          std::make_move_iterator(main_reporter_events_metrics.end()));
      frame_events_metrics.main_event_metrics.insert(
          frame_events_metrics.main_event_metrics.end(),
          std::make_move_iterator(remaining_reporter_events_metrics.begin()),
          std::make_move_iterator(remaining_reporter_events_metrics.end()));
      break;
  }
}

void CompositorFrameReportingController::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  bool feedback_failed = details.presentation_feedback.failed();

  for (auto submitted_frame = submitted_compositor_frames_.begin();
       submitted_frame != submitted_compositor_frames_.end() &&
       !viz::FrameTokenGT(submitted_frame->frame_token, frame_token);) {
    bool is_earlier_frame = submitted_frame->frame_token != frame_token;

    // If the presentation feedback is a failure, earlier frames should still be
    // left in the queue as they still might end up being presented
    // successfully. Skip to the next frame.
    if (feedback_failed && is_earlier_frame) {
      submitted_frame++;
      continue;
    }

    auto termination_status = feedback_failed
                                  ? FrameTerminationStatus::kDidNotPresentFrame
                                  : FrameTerminationStatus::kPresentedFrame;

    // If this is an earlier frame, presentation feedback has been successful
    // which means this earlier frame should be considered dropped.
    if (is_earlier_frame)
      termination_status = FrameTerminationStatus::kDidNotPresentFrame;

    auto& reporter = submitted_frame->reporter;
    reporter->SetVizBreakdown(details);
    reporter->TerminateFrame(termination_status,
                             details.presentation_feedback.timestamp);

    base::TimeDelta latency_prediction_deviation_threshold;
    if (EventLatencyTracingRecorder::IsEventLatencyTracingEnabled()) {
      latency_prediction_deviation_threshold =
          details.presentation_feedback.interval.is_zero()
              ? kDefaultLatencyPredictionDeviationThreshold
              : (details.presentation_feedback.interval) / 2;
      switch (reporter->get_reporter_type()) {
        case CompositorFrameReporter::ReporterType::kImpl:
          reporter->CalculateCompositorLatencyPrediction(
              previous_latency_predictions_impl_,
              latency_prediction_deviation_threshold);
          break;
        case CompositorFrameReporter::ReporterType::kMain:
          reporter->CalculateCompositorLatencyPrediction(
              previous_latency_predictions_main_,
              latency_prediction_deviation_threshold);
          break;
      }
    }

    // If the page was transitioned from invisible to visible, need to throw
    // away EventsMetrics from `events_metrics_from_dropped_frames_` because
    // these measurement would be invalid due to the duration of page being
    // invisible.
    if (waiting_for_did_present_after_visible_) {
      waiting_for_did_present_after_visible_ = false;
      // The implicit assumption is that submitted_frame->frame_token will never
      // be equal to it->first
      for (auto it = events_metrics_from_dropped_frames_.begin();
           it != events_metrics_from_dropped_frames_.end() &&
           submitted_frame->frame_token > it->first;
           it = events_metrics_from_dropped_frames_.erase(it)) {
      }
    }

    if (termination_status == FrameTerminationStatus::kPresentedFrame) {
      if (EventLatencyTracingRecorder::IsEventLatencyTracingEnabled()) {
        // TODO(crbug.com/40228308): Consider using a separate container to
        // differentiate event predictions with and without a main dispatch
        // stage.
        reporter->CalculateEventLatencyPrediction(
            event_latency_predictions_, latency_prediction_deviation_threshold);
      }

      // For presented frames, if `reporter` was cloned from another reporter,
      // and the original reporter is still alive, then check whether the cloned
      // reporter has a 'partial update decider'. It is still possible for the
      // original reporter to terminate with 'no damage', and if that happens,
      // then the cloned reporter's 'partial update' flag will need to be reset.
      // To allow this to happen, keep the cloned reporter alive, and hand over
      // its ownership to the original reporter, so that the cloned reporter
      // stays alive until the original reporter is terminated, and the cloned
      // reporter's 'partial update' flag can be unset if necessary. This is not
      // necessary for frames with failed presentation as we can say for sure
      // that they are dropped and nothing will change their fate.

      CompositorFrameReporter* reporter_ptr = reporter.get();
      if (CompositorFrameReporter* orig_reporter =
              reporter->partial_update_decider()) {
        orig_reporter->AdoptReporter(std::move(reporter));
      }

      // Pass dropped events only after the potential adoption has already taken
      // place, as we don't want to pass the events from previously dropped
      // frames to the adopter.
      auto next = submitted_frame + 1;
      bool next_reporter_from_same_frame =
          next != submitted_compositor_frames_.end() &&
          submitted_frame->frame_token == next->frame_token;
      MaybePassEventMetricsFromDroppedFrames(*reporter_ptr,
                                             submitted_frame->frame_token,
                                             next_reporter_from_same_frame);

      reporter_ptr->DidSuccessfullyPresentFrame();
    } else {
      StoreEventMetricsFromDroppedFrames(*reporter,
                                         submitted_frame->frame_token);
    }

    if (feedback_failed) {
      // When feedback is for a failed presentation, `submitted_frame` is not
      // necessarily in the front of the queue. We will reach here only once per
      // did-present; so, we will have 1 operation of O(n) complexity (n is the
      // number of previous frames).
      submitted_frame = submitted_compositor_frames_.erase(submitted_frame);
    } else {
      // When feedback is for a successful presentation, `submitted_frame` is in
      // the front of the queue; so, we will have n operations of O(1)
      // complexity for a did-present (n is the number of previous frames).
      // `pop_front()` function is used here to shrink the queue when necessary
      // to avoid unnecessary memory usage over time.
      DCHECK_EQ(submitted_frame->frame_token,
                submitted_compositor_frames_.front().frame_token);
      submitted_compositor_frames_.pop_front();
      submitted_frame = submitted_compositor_frames_.begin();
    }
  }
}

void CompositorFrameReportingController::OnStoppedRequestingBeginFrames() {
  // If the client stopped requesting begin-frames, that means the begin-frames
  // currently being handled are no longer expected to produce any
  // compositor-frames. So terminate the reporters.
  auto now = Now();
  for (int i = 0; i < PipelineStage::kNumPipelineStages; ++i) {
    if (reporters_[i]) {
      reporters_[i]->OnDidNotProduceFrame(FrameSkippedReason::kNoDamage);
      reporters_[i]->TerminateFrame(FrameTerminationStatus::kDidNotProduceFrame,
                                    now);
    }
  }
  last_started_compositor_frame_ = {};
}

void CompositorFrameReportingController::NotifyReadyToCommit(
    std::unique_ptr<BeginMainFrameMetrics> details) {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->SetBlinkBreakdown(
      std::move(details), begin_main_frame_start_time_);
  AdvanceReporterStage(PipelineStage::kBeginMainFrame,
                       PipelineStage::kReadyToCommit);
}

void CompositorFrameReportingController::AddActiveTracker(
    FrameSequenceTrackerType type) {
  active_trackers_.set(static_cast<size_t>(type));
}

void CompositorFrameReportingController::RemoveActiveTracker(
    FrameSequenceTrackerType type) {
  active_trackers_.reset(static_cast<size_t>(type));
  if (global_trackers_.dropped_frame_counter)
    global_trackers_.dropped_frame_counter->ReportFrames();
}

void CompositorFrameReportingController::SetScrollingThread(
    FrameInfo::SmoothEffectDrivingThread thread) {
  scrolling_thread_ = thread;
}

void CompositorFrameReportingController::SetThreadAffectsSmoothness(
    FrameInfo::SmoothEffectDrivingThread thread_type,
    bool affects_smoothness) {
  auto current_smooth_thread = GetSmoothThread();

  if (thread_type == FrameInfo::SmoothEffectDrivingThread::kCompositor) {
    is_compositor_thread_driving_smoothness_ = affects_smoothness;
  } else {
    DCHECK_EQ(thread_type, FrameInfo::SmoothEffectDrivingThread::kMain);
    is_main_thread_driving_smoothness_ = affects_smoothness;
  }

  // keep the history for the last 3 seconds.
  if (!smooth_thread_history_.empty()) {
    auto expired_smooth_thread =
        smooth_thread_history_.lower_bound(Now() - base::Seconds(3));
    smooth_thread_history_.erase(smooth_thread_history_.begin(),
                                 expired_smooth_thread);
  }

  // Only trackes the history if there is a change in smooth_thread_
  if (current_smooth_thread != GetSmoothThread()) {
    smooth_thread_history_.insert(std::make_pair(Now(), current_smooth_thread));
  }
}

void CompositorFrameReportingController::AdvanceReporterStage(
    PipelineStage start,
    PipelineStage target) {
  auto& reporter = reporters_[target];
  if (reporter) {
    auto termination_status = FrameTerminationStatus::kReplacedByNewReporter;
    base::TimeTicks termination_time;
    if (reporter->did_not_produce_frame()) {
      termination_time = reporter->did_not_produce_frame_time();
      termination_status = FrameTerminationStatus::kDidNotProduceFrame;
    } else if (target == PipelineStage::kBeginMainFrame &&
               reporter->did_abort_main_frame()) {
      termination_time = reporter->main_frame_abort_time();
    } else {
      termination_time = Now();
    }
    reporter->TerminateFrame(termination_status, termination_time);
  }
  reporters_[target] = std::move(reporters_[start]);
}

bool CompositorFrameReportingController::CanSubmitImplFrame(
    const viz::BeginFrameId& id) const {
#if DCHECK_IS_ON()
  auto& reporter = reporters_[PipelineStage::kBeginImplFrame];
  if (reporter) {
    DCHECK_EQ(reporter->frame_id(), id);
    DCHECK(reporter->did_finish_impl_frame());
  }
#endif
  return reporters_[PipelineStage::kBeginImplFrame].get() != nullptr;
}

bool CompositorFrameReportingController::CanSubmitMainFrame(
    const viz::BeginFrameId& id) const {
  if (!reporters_[PipelineStage::kBeginMainFrame])
    return false;
  auto& reporter = reporters_[PipelineStage::kBeginMainFrame];
  return (reporter->frame_id() == id && reporter->did_finish_impl_frame() &&
          reporter->did_abort_main_frame());
}

std::unique_ptr<CompositorFrameReporter>
CompositorFrameReportingController::RestoreReporterAtBeginImpl(
    const viz::BeginFrameId& id) {
  auto& main_reporter = reporters_[PipelineStage::kBeginMainFrame];
  auto& ready_to_commit_reporter = reporters_[PipelineStage::kReadyToCommit];
  auto& commit_reporter = reporters_[PipelineStage::kCommit];
  if (main_reporter && main_reporter->frame_id() == id) {
    DCHECK(!ready_to_commit_reporter ||
           ready_to_commit_reporter->frame_id() != id);
    DCHECK(!commit_reporter || commit_reporter->frame_id() != id);
    return main_reporter->CopyReporterAtBeginImplStage();
  }
  if (ready_to_commit_reporter && ready_to_commit_reporter->frame_id() == id) {
    DCHECK(!commit_reporter || commit_reporter->frame_id() != id);
    return ready_to_commit_reporter->CopyReporterAtBeginImplStage();
  }
  if (commit_reporter && commit_reporter->frame_id() == id)
    return commit_reporter->CopyReporterAtBeginImplStage();
  return nullptr;
}

void CompositorFrameReportingController::InitializeUkmManager(
    std::unique_ptr<ukm::UkmRecorder> recorder) {
  latency_ukm_reporter_->InitializeUkmManager(std::move(recorder));
  // TODO(crbug/334977830): the mix of `GlobalMetricsTrackers` and `raw_ptr` is
  // making ownership harder to follow. We should clean this all up.
  //
  // The order of reporters is strictly managed to guarantee their lifetimes.
  // `latency_ukm_reporter_` outlives `scroll_jank_ukm_reporter_`.
  scroll_jank_ukm_reporter_->set_ukm_manager(
      latency_ukm_reporter_->ukm_manager());
}

void CompositorFrameReportingController::SetSourceId(ukm::SourceId source_id) {
  latency_ukm_reporter_->SetSourceId(source_id);
}

CompositorFrameReporter::SmoothThread
CompositorFrameReportingController::GetSmoothThread() const {
  if (is_main_thread_driving_smoothness_) {
    return is_compositor_thread_driving_smoothness_ ? SmoothThread::kSmoothBoth
                                                    : SmoothThread::kSmoothMain;
  }

  return is_compositor_thread_driving_smoothness_
             ? SmoothThread::kSmoothCompositor
             : SmoothThread::kSmoothNone;
}

CompositorFrameReporter::SmoothThread
CompositorFrameReportingController::GetSmoothThreadAtTime(
    base::TimeTicks timestamp) const {
  if (smooth_thread_history_.lower_bound(timestamp) ==
      smooth_thread_history_.end())
    return GetSmoothThread();
  return smooth_thread_history_.lower_bound(timestamp)->second;
}

CompositorFrameReporter*
CompositorFrameReportingController::GetOutstandingUpdatesFromMain(
    const viz::BeginFrameId& id) const {
  // Any unterminated reporter in the 'main frame', or 'commit' stages, then
  // that indicates some pending updates from the main thread.
  {
    const auto& reporter = reporters_[PipelineStage::kBeginMainFrame];
    if (reporter && reporter->frame_id() < id &&
        !reporter->did_abort_main_frame()) {
      return reporter.get();
    }
  }
  {
    const auto& reporter = reporters_[PipelineStage::kReadyToCommit];
    if (reporter && reporter->frame_id() < id &&
        !reporter->did_abort_main_frame()) {
      return reporter.get();
    }
  }
  {
    const auto& reporter = reporters_[PipelineStage::kCommit];
    if (reporter && reporter->frame_id() < id) {
      DCHECK(!reporter->did_abort_main_frame());
      return reporter.get();
    }
  }
  return nullptr;
}

void CompositorFrameReportingController::CreateReportersForDroppedFrames(
    const viz::BeginFrameArgs& old_args,
    const viz::BeginFrameArgs& new_args) const {
  DCHECK_EQ(new_args.frame_id.source_id, old_args.frame_id.source_id);
  DCHECK_GE(
      new_args.frame_id.sequence_number - new_args.frames_throttled_since_last,
      old_args.frame_id.sequence_number);
  const uint32_t interval = new_args.frame_id.sequence_number -
                            old_args.frame_id.sequence_number -
                            new_args.frames_throttled_since_last;

  // Up to 100 frames will be reported (100 closest frames to new_args).
  const uint32_t kMaxFrameCount = 100;

  // If there are more than 100 frames skipped, ignore them
  if (interval > kMaxFrameCount)
    return;

  auto timestamp = old_args.frame_time + old_args.interval;
  for (uint32_t i = 1; i < interval; ++i, timestamp += old_args.interval) {
    auto args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, old_args.frame_id.source_id,
        old_args.frame_id.sequence_number + i, timestamp,
        timestamp + old_args.interval, old_args.interval,
        viz::BeginFrameArgs::NORMAL);
    devtools_instrumentation::DidBeginFrame(
        layer_tree_host_id_, args.frame_time, args.frame_id.sequence_number);
    // ThreadType::kUnknown is used here for scrolling thread, because the
    // frames reported here could have a scroll interaction active at their
    // start time, but they were skipped and history of scrolling thread might
    // change in the diff of start time and report time.
    auto reporter = std::make_unique<CompositorFrameReporter>(
        active_trackers_, args, should_report_histograms_,
        GetSmoothThreadAtTime(timestamp),
        FrameInfo::SmoothEffectDrivingThread::kUnknown, layer_tree_host_id_,
        global_trackers_);
    reporter->set_tick_clock(tick_clock_);
    reporter->StartStage(StageType::kBeginImplFrameToSendBeginMainFrame,
                         timestamp);
    reporter->TerminateFrame(FrameTerminationStatus::kDidNotPresentFrame,
                             args.deadline);
    reporter->set_is_backfill(true);
  }
}

void CompositorFrameReportingController::AddSortedFrame(
    const viz::BeginFrameArgs& args,
    const FrameInfo& frame_info) {
  if (global_trackers_.frame_sequence_trackers) {
    global_trackers_.frame_sequence_trackers->AddSortedFrame(args, frame_info);
  }
}

void CompositorFrameReportingController::SetDroppedFrameCounter(
    DroppedFrameCounter* counter) {
  global_trackers_.dropped_frame_counter = counter;
  if (counter) {
    counter->SetSortedFrameCallback(
        base::BindRepeating(&CompositorFrameReportingController::AddSortedFrame,
                            base::Unretained(this)));
  }
}

}  // namespace cc
