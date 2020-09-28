// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporting_controller.h"

#include <utility>

#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/dropped_frame_counter.h"
#include "cc/metrics/latency_ukm_reporter.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace cc {
namespace {
using SmoothThread = CompositorFrameReporter::SmoothThread;
using StageType = CompositorFrameReporter::StageType;
using FrameTerminationStatus = CompositorFrameReporter::FrameTerminationStatus;
}  // namespace

CompositorFrameReportingController::CompositorFrameReportingController(
    bool should_report_metrics,
    int layer_tree_host_id)
    : should_report_metrics_(should_report_metrics),
      layer_tree_host_id_(layer_tree_host_id),
      latency_ukm_reporter_(std::make_unique<LatencyUkmReporter>()) {}

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

void CompositorFrameReportingController::WillBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  base::TimeTicks begin_time = Now();
  if (reporters_[PipelineStage::kBeginImplFrame]) {
    auto& reporter = reporters_[PipelineStage::kBeginImplFrame];
    DCHECK(reporter->did_finish_impl_frame());
    DCHECK(reporter->did_not_produce_frame());
    reporter->TerminateFrame(FrameTerminationStatus::kDidNotProduceFrame,
                             reporter->did_not_produce_frame_time());
  }
  auto reporter = std::make_unique<CompositorFrameReporter>(
      active_trackers_, args, latency_ukm_reporter_.get(),
      should_report_metrics_, GetSmoothThread(), layer_tree_host_id_);
  reporter->set_tick_clock(tick_clock_);
  reporter->StartStage(StageType::kBeginImplFrameToSendBeginMainFrame,
                       begin_time);
  reporter->SetDroppedFrameCounter(dropped_frame_counter_);
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
    // deadline yet). So will start a new reporter at BeginMainFrame.
    auto reporter = std::make_unique<CompositorFrameReporter>(
        active_trackers_, args, latency_ukm_reporter_.get(),
        should_report_metrics_, GetSmoothThread(), layer_tree_host_id_);
    reporter->set_tick_clock(tick_clock_);
    reporter->StartStage(StageType::kSendBeginMainFrameToCommit, Now());
    reporter->SetDroppedFrameCounter(dropped_frame_counter_);
    reporters_[PipelineStage::kBeginMainFrame] = std::move(reporter);
  }
}

void CompositorFrameReportingController::BeginMainFrameAborted(
    const viz::BeginFrameId& id) {
  auto& reporter = reporters_[PipelineStage::kBeginMainFrame];
  DCHECK(reporter);
  DCHECK_EQ(reporter->frame_id(), id);
  reporter->OnAbortBeginMainFrame(Now());
}

void CompositorFrameReportingController::WillCommit() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->StartStage(StageType::kCommit,
                                                         Now());
}

void CompositorFrameReportingController::DidCommit() {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->StartStage(
      StageType::kEndCommitToActivation, Now());
  AdvanceReporterStage(PipelineStage::kBeginMainFrame, PipelineStage::kCommit);
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
    uint32_t frame_token,
    const viz::BeginFrameId& current_frame_id,
    const viz::BeginFrameId& last_activated_frame_id,
    EventMetricsSet events_metrics) {
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
      // If the frame does not include any new updates from the main thread,
      // then flag the frame as containing only partial updates.
      if (!is_activated_frame_new)
        reporter->set_has_partial_update(true);
      impl_reporter = std::move(reporter);
    }
  }

#if DCHECK_IS_ON()
  if (!events_metrics.main_event_metrics.empty()) {
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
  if (!impl_reporter && !events_metrics.impl_event_metrics.empty()) {
    DCHECK(main_reporter);
    // If there are impl events, there must be a reporter with
    // |current_frame_id|.
    DCHECK_EQ(main_reporter->frame_id(), current_frame_id);
    events_metrics.main_event_metrics.reserve(
        events_metrics.main_event_metrics.size() +
        events_metrics.impl_event_metrics.size());
    events_metrics.main_event_metrics.insert(
        events_metrics.main_event_metrics.end(),
        events_metrics.impl_event_metrics.begin(),
        events_metrics.impl_event_metrics.end());
  }

  if (main_reporter) {
    main_reporter->StartStage(
        StageType::kSubmitCompositorFrameToPresentationCompositorFrame, Now());
    main_reporter->SetEventsMetrics(
        std::move(events_metrics.main_event_metrics));
    submitted_compositor_frames_.emplace_back(frame_token,
                                              std::move(main_reporter));
  }

  if (impl_reporter) {
    impl_reporter->EnableCompositorOnlyReporting();
    impl_reporter->StartStage(
        StageType::kSubmitCompositorFrameToPresentationCompositorFrame, Now());
    impl_reporter->SetEventsMetrics(
        std::move(events_metrics.impl_event_metrics));
    submitted_compositor_frames_.emplace_back(frame_token,
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
      break;
    }
  }

  // If the compositor has no updates, and the main-thread has not responded to
  // the begin-main-frame yet, then it is essentially a dropped frame. To handle
  // this case, keep the reporter for the main-thread, but recreate a reporter
  // for the dropped-frame.
  if (skip_reason == FrameSkippedReason::kWaitingOnMain) {
    auto reporter = RestoreReporterAtBeginImpl(id);
    if (reporter) {
      reporter->OnDidNotProduceFrame(skip_reason);
      reporter->TerminateFrame(FrameTerminationStatus::kDidNotProduceFrame,
                               Now());
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

void CompositorFrameReportingController::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  while (!submitted_compositor_frames_.empty()) {
    auto submitted_frame = submitted_compositor_frames_.begin();
    if (viz::FrameTokenGT(submitted_frame->frame_token, frame_token))
      break;

    auto termination_status = FrameTerminationStatus::kPresentedFrame;
    if (submitted_frame->frame_token != frame_token ||
        details.presentation_feedback.failed()) {
      termination_status = FrameTerminationStatus::kDidNotPresentFrame;
    }

    auto& reporter = submitted_frame->reporter;
    reporter->SetVizBreakdown(details);
    reporter->TerminateFrame(termination_status,
                             details.presentation_feedback.timestamp);

    // If |reporter| was cloned from a reporter, and the original reporter is
    // still alive, then check whether the cloned reporter has the 'partial
    // update' flag set. It is still possible for the original reporter to
    // terminate with 'no damage', and if that happens, then the cloned
    // reporter's 'partial update' flag will need to be reset. To allow this to
    // happen, keep the cloned reporter alive, and hand over its ownership to
    // the original reporter, so that the cloned reporter stays alive until the
    // original reporter is terminated, and the cloned reporter's 'partial
    // update' flag can be unset if necessary.
    if (reporter->has_partial_update()) {
      auto orig_reporter = reporter->cloned_from();
      if (orig_reporter)
        orig_reporter->AdoptReporter(std::move(reporter));
    }
    submitted_compositor_frames_.erase(submitted_frame);
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
}

void CompositorFrameReportingController::SetBlinkBreakdown(
    std::unique_ptr<BeginMainFrameMetrics> details,
    base::TimeTicks main_thread_start_time) {
  DCHECK(reporters_[PipelineStage::kBeginMainFrame]);
  reporters_[PipelineStage::kBeginMainFrame]->SetBlinkBreakdown(
      std::move(details), main_thread_start_time);
}

void CompositorFrameReportingController::AddActiveTracker(
    FrameSequenceTrackerType type) {
  active_trackers_.set(static_cast<size_t>(type));
}

void CompositorFrameReportingController::RemoveActiveTracker(
    FrameSequenceTrackerType type) {
  active_trackers_.reset(static_cast<size_t>(type));
  if (dropped_frame_counter_)
    dropped_frame_counter_->ReportFrames();
}

void CompositorFrameReportingController::SetThreadAffectsSmoothness(
    FrameSequenceMetrics::ThreadType thread_type,
    bool affects_smoothness) {
  if (thread_type == FrameSequenceMetrics::ThreadType::kCompositor) {
    is_compositor_thread_driving_smoothness_ = affects_smoothness;
  } else {
    DCHECK_EQ(thread_type, FrameSequenceMetrics::ThreadType::kMain);
    is_main_thread_driving_smoothness_ = affects_smoothness;
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
  auto& commit_reporter = reporters_[PipelineStage::kCommit];
  if (main_reporter && main_reporter->frame_id() == id)
    return main_reporter->CopyReporterAtBeginImplStage();
  if (commit_reporter && commit_reporter->frame_id() == id)
    return commit_reporter->CopyReporterAtBeginImplStage();
  return nullptr;
}

void CompositorFrameReportingController::SetUkmManager(UkmManager* manager) {
  latency_ukm_reporter_->set_ukm_manager(manager);
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

}  // namespace cc
