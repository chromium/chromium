// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/delay_policy.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/features.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/compositor_timing_history.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_compositor_scheduler_state.pbzero.h"

namespace cc {

namespace {
// This is a fudge factor we subtract from the deadline to account
// for message latency and kernel scheduling variability.
const base::TimeDelta kDeadlineFudgeFactor = base::Microseconds(1000);

// This adjustment is applied by multiplying with the previous, begin-main-frame
// to activate threshold. For example, if we want to consider a page fast if it
// it takes half the threshold, we would return 0.5. Naturally, this function
// will return values in the range [0, 1].
double FastMainThreadThresholdAdjustment() {
  if (base::FeatureList::IsEnabled(features::kAdjustFastMainThreadThreshold)) {
    double result = base::GetFieldTrialParamByFeatureAsDouble(
        features::kAdjustFastMainThreadThreshold, "Scalar", -1.0);
    if (result >= 0.0 && result <= 1.0) {
      return result;
    }
  }
  return 1.0;
}

}  // namespace

Scheduler::Scheduler(
    SchedulerClient* client,
    const SchedulerSettings& settings,
    int layer_tree_host_id,
    base::SingleThreadTaskRunner* task_runner,
    std::unique_ptr<CompositorTimingHistory> compositor_timing_history,
    CompositorFrameReportingController* compositor_frame_reporting_controller)
    : settings_(settings),
      client_(client),
      layer_tree_host_id_(layer_tree_host_id),
      task_runner_(task_runner),
      compositor_timing_history_(std::move(compositor_timing_history)),
      compositor_frame_reporting_controller_(
          compositor_frame_reporting_controller),
      begin_impl_frame_tracker_(FROM_HERE),
      state_machine_(settings) {
  TRACE_EVENT1("cc", "Scheduler::Scheduler", "settings", settings_.AsValue());
  DCHECK(client_);
  DCHECK(!state_machine_.BeginFrameNeeded());

  begin_impl_frame_deadline_timer_.SetTaskRunner(task_runner);

  // We want to handle animate_only BeginFrames.
  wants_animate_only_begin_frames_ = true;

  ProcessScheduledActions();
}

Scheduler::~Scheduler() {
  SetBeginFrameSource(nullptr);
}

void Scheduler::Stop() {
  stopped_ = true;
}

void Scheduler::SetNeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {
  {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "Scheduler::SetNeedsImplSideInvalidation",
                 "needs_first_draw_on_activation",
                 needs_first_draw_on_activation);
    state_machine_.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  }
  ProcessScheduledActions();
}

base::TimeTicks Scheduler::Now() const {
  base::TimeTicks now = base::TimeTicks::Now();
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.now"),
               "Scheduler::Now", "now", now);
  return now;
}

void Scheduler::SetVisible(bool visible) {
  state_machine_.SetVisible(visible);
  UpdateCompositorTimingHistoryRecordingEnabled();
  ProcessScheduledActions();
}

void Scheduler::SetShouldWarmUp() {
  CHECK(base::FeatureList::IsEnabled(features::kWarmUpCompositor));
  state_machine_.SetShouldWarmUp();
  ProcessScheduledActions();
}

void Scheduler::SetCanDraw(bool can_draw) {
  state_machine_.SetCanDraw(can_draw);
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToActivate() {
  if (state_machine_.NotifyReadyToActivate())
    compositor_timing_history_->ReadyToActivate();

  ProcessScheduledActions();
}

bool Scheduler::IsReadyToActivate() {
  return state_machine_.IsReadyToActivate();
}

void Scheduler::NotifyReadyToDraw() {
  // Future work might still needed for crbug.com/352894.
  state_machine_.NotifyReadyToDraw();
  ProcessScheduledActions();
}

void Scheduler::SetBeginFrameSource(viz::BeginFrameSource* source) {
  if (source == begin_frame_source_)
    return;
  if (begin_frame_source_ && observing_begin_frame_source_)
    begin_frame_source_->RemoveObserver(this);
  begin_frame_source_ = source;
  if (!begin_frame_source_)
    return;
  if (observing_begin_frame_source_)
    begin_frame_source_->AddObserver(this);
}

void Scheduler::NotifyAnimationWorkletStateChange(AnimationWorkletState state,
                                                  TreeType tree) {
  state_machine_.NotifyAnimationWorkletStateChange(state, tree);
  ProcessScheduledActions();
}

void Scheduler::NotifyPaintWorkletStateChange(PaintWorkletState state) {
  state_machine_.NotifyPaintWorkletStateChange(state);
  ProcessScheduledActions();
}

void Scheduler::SetNeedsBeginMainFrame() {
  state_machine_.SetNeedsBeginMainFrame();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsOneBeginImplFrame() {
  state_machine_.SetNeedsOneBeginImplFrame();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsRedraw() {
  state_machine_.SetNeedsRedraw();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsUpdateDisplayTree() {
  state_machine_.SetNeedsUpdateDisplayTree();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsPrepareTiles() {
  DCHECK(!IsInsideAction(SchedulerStateMachine::Action::PREPARE_TILES));
  state_machine_.SetNeedsPrepareTiles();
  ProcessScheduledActions();
}

void Scheduler::DidSubmitCompositorFrame(SubmitInfo& submit_info) {
  // Hardware and software draw may occur at the same frame simultaneously for
  // Android WebView. There is no need to call DidSubmitCompositorFrame here for
  // software draw.
  if (!settings_.using_synchronous_renderer_compositor ||
      !state_machine_.resourceless_draw()) {
    compositor_frame_reporting_controller_->DidSubmitCompositorFrame(
        submit_info, begin_main_frame_args_.frame_id,
        last_activate_origin_frame_args_.frame_id);
  }
  state_machine_.DidSubmitCompositorFrame();

  // There is no need to call ProcessScheduledActions here because
  // submitting a CompositorFrame should not trigger any new actions.
  if (!inside_process_scheduled_actions_) {
    DCHECK_EQ(state_machine_.NextAction(), SchedulerStateMachine::Action::NONE);
  }
}

void Scheduler::DidReceiveCompositorFrameAck() {
  DCHECK_GT(state_machine_.pending_submit_frames(), 0);
  state_machine_.DidReceiveCompositorFrameAck();
  ProcessScheduledActions();
}

void Scheduler::SetTreePrioritiesAndScrollState(
    TreePriority tree_priority,
    ScrollHandlerState scroll_handler_state) {
  state_machine_.SetTreePrioritiesAndScrollState(tree_priority,
                                                 scroll_handler_state);
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToCommit(
    std::unique_ptr<BeginMainFrameMetrics> details) {
  {
    TRACE_EVENT0("cc", "Scheduler::NotifyReadyToCommit");
    compositor_timing_history_->NotifyReadyToCommit();
    compositor_frame_reporting_controller_->NotifyReadyToCommit(
        std::move(details));
    state_machine_.NotifyReadyToCommit();
    next_commit_origin_frame_args_ = last_dispatched_begin_main_frame_args_;
  }
  ProcessScheduledActions();
}

void Scheduler::BeginMainFrameAborted(CommitEarlyOutReason reason) {
  {
    TRACE_EVENT1("cc", "Scheduler::BeginMainFrameAborted", "reason",
                 CommitEarlyOutReasonToString(reason));
    compositor_timing_history_->BeginMainFrameAborted();
    auto frame_id = last_dispatched_begin_main_frame_args_.frame_id;
    compositor_frame_reporting_controller_->BeginMainFrameAborted(frame_id,
                                                                  reason);

    state_machine_.BeginMainFrameAborted(reason);
  }
  ProcessScheduledActions();
}

void Scheduler::DidPrepareTiles() {
  state_machine_.DidPrepareTiles();
}

void Scheduler::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  compositor_frame_reporting_controller_->DidPresentCompositorFrame(frame_token,
                                                                    details);
}

void Scheduler::DidLoseLayerTreeFrameSink() {
  {
    TRACE_EVENT0("cc", "Scheduler::DidLoseLayerTreeFrameSink");
    state_machine_.DidLoseLayerTreeFrameSink();
    UpdateCompositorTimingHistoryRecordingEnabled();
  }
  ProcessScheduledActions();
}

void Scheduler::DidCreateAndInitializeLayerTreeFrameSink() {
  {
    TRACE_EVENT0("cc", "Scheduler::DidCreateAndInitializeLayerTreeFrameSink");
    DCHECK(!observing_begin_frame_source_);
    DCHECK(!begin_impl_frame_deadline_timer_.IsRunning());
    state_machine_.DidCreateAndInitializeLayerTreeFrameSink();
    UpdateCompositorTimingHistoryRecordingEnabled();
  }
  ProcessScheduledActions();
}

void Scheduler::NotifyBeginMainFrameStarted(
    base::TimeTicks main_thread_start_time) {
  TRACE_EVENT0("cc", "Scheduler::NotifyBeginMainFrameStarted");
  compositor_timing_history_->BeginMainFrameStarted(main_thread_start_time);
  compositor_frame_reporting_controller_->BeginMainFrameStarted(
      main_thread_start_time);
}

base::TimeTicks Scheduler::LastBeginImplFrameTime() {
  return begin_impl_frame_tracker_.Current().frame_time;
}

void Scheduler::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  TRACE_EVENT1("cc", "Scheduler::BeginMainFrameNotExpectedUntil",
               "remaining_time", (time - Now()).InMillisecondsF());

  DCHECK(!inside_scheduled_action_);
  base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);
  client_->ScheduledActionBeginMainFrameNotExpectedUntil(time);
}

void Scheduler::BeginMainFrameNotExpectedSoon() {
  TRACE_EVENT0("cc", "Scheduler::BeginMainFrameNotExpectedSoon");

  DCHECK(!inside_scheduled_action_);
  base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);
  client_->SendBeginMainFrameNotExpectedSoon();
}

void Scheduler::StartOrStopBeginFrames() {
  if (state_machine_.begin_impl_frame_state() !=
      SchedulerStateMachine::BeginImplFrameState::IDLE) {
    return;
  }

  bool needs_begin_frames = state_machine_.ShouldSubscribeToBeginFrames();
  if (needs_begin_frames == observing_begin_frame_source_) {
    return;
  }

  if (needs_begin_frames) {
    observing_begin_frame_source_ = true;
    if (begin_frame_source_)
      begin_frame_source_->AddObserver(this);
    devtools_instrumentation::NeedsBeginFrameChanged(layer_tree_host_id_, true);
  } else {
    observing_begin_frame_source_ = false;
    if (begin_frame_source_)
      begin_frame_source_->RemoveObserver(this);
    // We're going idle so drop pending begin frame.
    if (settings_.using_synchronous_renderer_compositor)
      FinishImplFrameSynchronous();
    CancelPendingBeginFrameTask();

    compositor_timing_history_->BeginImplFrameNotExpectedSoon();
    compositor_frame_reporting_controller_->OnStoppedRequestingBeginFrames();
    devtools_instrumentation::NeedsBeginFrameChanged(layer_tree_host_id_,
                                                     false);
    client_->WillNotReceiveBeginFrame();
  }
}

void Scheduler::CancelPendingBeginFrameTask() {
  if (pending_begin_frame_args_.IsValid()) {
    TRACE_EVENT_INSTANT0("cc", "Scheduler::BeginFrameDropped",
                         TRACE_EVENT_SCOPE_THREAD);
    SendDidNotProduceFrame(pending_begin_frame_args_,
                           FrameSkippedReason::kNoDamage);
    // Make pending begin frame invalid so that we don't accidentally use it.
    pending_begin_frame_args_ = viz::BeginFrameArgs();
  }
  pending_begin_frame_task_.Cancel();
}

void Scheduler::PostPendingBeginFrameTask() {
  bool is_idle = state_machine_.begin_impl_frame_state() ==
                 SchedulerStateMachine::BeginImplFrameState::IDLE;
  bool needs_begin_frames = state_machine_.BeginFrameNeeded();
  // We only post one pending begin frame task at a time, but we update the args
  // whenever we get a new begin frame.
  bool has_pending_begin_frame_args = pending_begin_frame_args_.IsValid();
  bool has_no_pending_begin_frame_task =
      pending_begin_frame_task_.IsCancelled();

  if (is_idle && needs_begin_frames && has_pending_begin_frame_args &&
      has_no_pending_begin_frame_task) {
    pending_begin_frame_task_.Reset(base::BindOnce(
        &Scheduler::HandlePendingBeginFrame, base::Unretained(this)));
    task_runner_->PostTask(FROM_HERE, pending_begin_frame_task_.callback());
  }
}

void Scheduler::OnBeginFrameSourcePausedChanged(bool paused) {
  if (state_machine_.begin_frame_source_paused() == paused)
    return;
  {
    TRACE_EVENT_INSTANT1("cc", "Scheduler::SetBeginFrameSourcePaused",
                         TRACE_EVENT_SCOPE_THREAD, "paused", paused);
    state_machine_.SetBeginFrameSourcePaused(paused);
  }
  ProcessScheduledActions();
}

// BeginFrame is the mechanism that tells us that now is a good time to start
// making a frame. Usually this means that user input for the frame is complete.
// If the scheduler is busy, we queue the BeginFrame to be handled later as
// a BeginRetroFrame.
bool Scheduler::OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) {
  TRACE_EVENT1("cc,benchmark", "Scheduler::BeginFrame", "args", args.AsValue());

  // If the begin frame interval is different than last frame and bigger than
  // zero then let |client_| know about the new interval for animations. In
  // theory the interval should always be bigger than zero but the value is
  // provided by APIs outside our control.
  if (args.interval != last_frame_interval_ && args.interval.is_positive()) {
    last_frame_interval_ = args.interval;
    client_->FrameIntervalUpdated(last_frame_interval_);
  }

  // Drop the BeginFrame if we don't need one.
  if (!state_machine_.BeginFrameNeeded()) {
    TRACE_EVENT_INSTANT0("cc", "Scheduler::BeginFrameDropped",
                         TRACE_EVENT_SCOPE_THREAD);
    // Since we don't use the BeginFrame, we may later receive the same
    // BeginFrame again. Thus, we can't confirm it at this point, even though we
    // don't have any updates right now.
    SendDidNotProduceFrame(args, FrameSkippedReason::kNoDamage);
    return false;
  }

  // Trace this begin frame time through the Chrome stack
  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"),
                         "viz::BeginFrameArgs",
                         args.frame_time.since_origin().InMicroseconds(),
                         TRACE_EVENT_FLAG_FLOW_OUT);

  if (settings_.using_synchronous_renderer_compositor) {
    BeginImplFrameSynchronous(args);
    return true;
  }

  bool inside_previous_begin_frame =
      state_machine_.begin_impl_frame_state() ==
      SchedulerStateMachine::BeginImplFrameState::INSIDE_BEGIN_FRAME;

  if (inside_process_scheduled_actions_ || inside_previous_begin_frame ||
      pending_begin_frame_args_.IsValid()) {
    // The BFS can send a begin frame while scheduler is processing previous
    // frame, or a MISSED begin frame inside the ProcessScheduledActions loop
    // when AddObserver is called. The BFS (e.g. mojo) may queue up many begin
    // frame calls, but we only want to process the last one. Saving the args,
    // and posting a task achieves that.
    if (pending_begin_frame_args_.IsValid()) {
      TRACE_EVENT_INSTANT0("cc", "Scheduler::BeginFrameDropped",
                           TRACE_EVENT_SCOPE_THREAD);
      SendDidNotProduceFrame(pending_begin_frame_args_,
                             FrameSkippedReason::kRecoverLatency);
    }
    pending_begin_frame_args_ = args;
    // ProcessScheduledActions() will post the previous frame's deadline if it
    // hasn't run yet, or post the begin frame task if the previous frame's
    // deadline has already run. If we're already inside
    // ProcessScheduledActions() this call will be a nop and the above will
    // happen at end of the top most call to ProcessScheduledActions().
    ProcessScheduledActions();
  } else {
    // This starts the begin frame immediately, and puts us in the
    // INSIDE_BEGIN_FRAME state, so if the message loop calls a bunch of
    // BeginFrames immediately after this call, they will be posted as a single
    // task, and all but the last BeginFrame will be dropped.
    BeginImplFrameWithDeadline(args);
  }
  return true;
}

void Scheduler::SetVideoNeedsBeginFrames(bool video_needs_begin_frames) {
  state_machine_.SetVideoNeedsBeginFrames(video_needs_begin_frames);
  ProcessScheduledActions();
}

void Scheduler::SetIsScrolling(bool is_scrolling) {
  state_machine_.set_is_scrolling(is_scrolling);
}

void Scheduler::SetWaitingForScrollEvent(bool waiting_for_scroll_event) {
  state_machine_.set_waiting_for_scroll_event(waiting_for_scroll_event);
}

void Scheduler::OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                            bool skip_draw) {
  DCHECK(settings_.using_synchronous_renderer_compositor);
  if (state_machine_.begin_impl_frame_state() ==
      SchedulerStateMachine::BeginImplFrameState::INSIDE_BEGIN_FRAME) {
    DCHECK(needs_finish_frame_for_synchronous_compositor_);
  } else {
    DCHECK_EQ(state_machine_.begin_impl_frame_state(),
              SchedulerStateMachine::BeginImplFrameState::IDLE);
    DCHECK(!needs_finish_frame_for_synchronous_compositor_);
  }
  DCHECK(!begin_impl_frame_deadline_timer_.IsRunning());

  state_machine_.SetResourcelessSoftwareDraw(resourceless_software_draw);
  state_machine_.SetSkipDraw(skip_draw);
  OnBeginImplFrameDeadline();

  state_machine_.OnBeginImplFrameIdle();
  ProcessScheduledActions();
  state_machine_.SetResourcelessSoftwareDraw(false);
}

// This is separate from BeginImplFrameWithDeadline() because we only want at
// most one outstanding task even if |pending_begin_frame_args_| changes.
void Scheduler::HandlePendingBeginFrame() {
  DCHECK(pending_begin_frame_args_.IsValid());
  viz::BeginFrameArgs args = pending_begin_frame_args_;
  pending_begin_frame_args_ = viz::BeginFrameArgs();
  pending_begin_frame_task_.Cancel();

  BeginImplFrameWithDeadline(args);
}

void Scheduler::BeginImplFrameWithDeadline(const viz::BeginFrameArgs& args) {
  DCHECK(pending_begin_frame_task_.IsCancelled());
  DCHECK(!pending_begin_frame_args_.IsValid());

  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BeginImplFrameState::IDLE);

  bool main_thread_is_in_high_latency_mode =
      state_machine_.main_thread_missed_last_deadline();
  TRACE_EVENT2("cc,benchmark", "Scheduler::BeginImplFrame", "args",
               args.AsValue(), "main_thread_missed_last_deadline",
               main_thread_is_in_high_latency_mode);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "MainThreadLatency", main_thread_is_in_high_latency_mode);

  base::TimeTicks now = Now();
  // Discard missed begin frames if they are too late. In full-pipe mode, we
  // ignore BeginFrame deadlines.
  if (!settings_.wait_for_all_pipeline_stages_before_draw &&
      args.type == viz::BeginFrameArgs::MISSED && args.deadline < now) {
    TRACE_EVENT_INSTANT0("cc", "Scheduler::MissedBeginFrameDropped",
                         TRACE_EVENT_SCOPE_THREAD);
    skipped_last_frame_missed_exceeded_deadline_ = true;
    SendDidNotProduceFrame(args, FrameSkippedReason::kRecoverLatency);
    return;
  }
  skipped_last_frame_missed_exceeded_deadline_ = false;

  viz::BeginFrameArgs adjusted_args = args;
  adjusted_args.deadline -= compositor_timing_history_->DrawDurationEstimate();
  adjusted_args.deadline -= kDeadlineFudgeFactor;

  // TODO(khushalsagar): We need to consider the deadline fudge factor here to
  // match the deadline used in BeginImplFrameDeadlineMode::REGULAR mode
  // (used in the case where the impl thread needs to redraw). In the case where
  // main_frame_to_active is fast, we should consider using
  // BeginImplFrameDeadlineMode::LATE instead to avoid putting the main
  // thread in high latency mode. See crbug.com/753146.
  base::TimeDelta bmf_to_activate_threshold =
      adjusted_args.interval -
      compositor_timing_history_->DrawDurationEstimate() - kDeadlineFudgeFactor;

  base::TimeDelta bmf_to_activate_estimate_critical =
      compositor_timing_history_
          ->BeginMainFrameQueueToActivateCriticalEstimate();
  base::TimeDelta fast_main_thread_threshold =
      bmf_to_activate_threshold * FastMainThreadThresholdAdjustment();
  state_machine_.SetCriticalBeginMainFrameToActivateIsFast(
      bmf_to_activate_estimate_critical < fast_main_thread_threshold);

  // Update the BeginMainFrame args now that we know whether the main
  // thread will be on the critical path or not.
  begin_main_frame_args_ = adjusted_args;
  begin_main_frame_args_.on_critical_path = !ImplLatencyTakesPriority();

  // If we expect the main thread to respond within this frame, defer the
  // invalidation to merge it with the incoming main frame. Even if the response
  // is delayed such that the raster can not be completed within this frame's
  // draw, its better to delay the invalidation than blocking the pipeline with
  // an extra pending tree update to be flushed.
  base::TimeDelta time_since_main_frame_sent;
  if (compositor_timing_history_->begin_main_frame_sent_time() !=
      base::TimeTicks()) {
    time_since_main_frame_sent =
        now - compositor_timing_history_->begin_main_frame_sent_time();
  }
  base::TimeDelta bmf_sent_to_ready_to_commit_estimate;
  if (begin_main_frame_args_.on_critical_path) {
    bmf_sent_to_ready_to_commit_estimate =
        compositor_timing_history_
            ->BeginMainFrameStartToReadyToCommitCriticalEstimate();
  } else {
    bmf_sent_to_ready_to_commit_estimate =
        compositor_timing_history_
            ->BeginMainFrameStartToReadyToCommitNotCriticalEstimate();
  }

  bool main_thread_response_expected_soon;
  // Allow the main thread to delay N impl frame before we decide to give up
  // and create a pending tree instead.
  time_since_main_frame_sent -=
      args.interval * settings_.delay_impl_invalidation_frames;
  if (time_since_main_frame_sent > bmf_to_activate_threshold) {
    // If the response to a main frame is pending past the desired duration
    // then proactively assume that the main thread is slow instead of late
    // correction through the frame history.
    main_thread_response_expected_soon = false;
  } else {
    main_thread_response_expected_soon =
        bmf_sent_to_ready_to_commit_estimate - time_since_main_frame_sent <
        bmf_to_activate_threshold;
  }
  state_machine_.set_should_defer_invalidation_for_fast_main_frame(
      main_thread_response_expected_soon);

  BeginImplFrame(adjusted_args, now);
}

void Scheduler::BeginImplFrameSynchronous(const viz::BeginFrameArgs& args) {
  // Finish the previous frame (if needed) before starting a new one.
  FinishImplFrameSynchronous();

  TRACE_EVENT1("cc,benchmark", "Scheduler::BeginImplFrame", "args",
               args.AsValue());
  // The main thread currently can't commit before we draw with the
  // synchronous compositor, so never consider the BeginMainFrame fast.
  state_machine_.SetCriticalBeginMainFrameToActivateIsFast(false);
  begin_main_frame_args_ = args;
  begin_main_frame_args_.on_critical_path = !ImplLatencyTakesPriority();

  viz::BeginFrameArgs adjusted_args = args;
  adjusted_args.deadline -= compositor_timing_history_->DrawDurationEstimate();
  adjusted_args.deadline -= kDeadlineFudgeFactor;

  BeginImplFrame(adjusted_args, Now());
  compositor_timing_history_->WillFinishImplFrame(
      state_machine_.needs_redraw());
  compositor_frame_reporting_controller_->OnFinishImplFrame(
      adjusted_args.frame_id);
  // Delay the call to |FinishFrame()| if a draw is anticipated, so that it is
  // called after the draw happens (in |OnDrawForLayerTreeFrameSink()|).
  needs_finish_frame_for_synchronous_compositor_ = true;
  if (!state_machine_.did_invalidate_layer_tree_frame_sink()) {
    // If there was no invalidation, then finish the frame immediately.
    FinishImplFrameSynchronous();
  }
}

void Scheduler::FinishImplFrame() {
  DCHECK(!needs_finish_frame_for_synchronous_compositor_);
  state_machine_.OnBeginImplFrameIdle();

  // Send ack before calling ProcessScheduledActions() because it might send an
  // ack for any pending begin frame if we are going idle after this. This
  // ensures that the acks are sent in order.
  if (!state_machine_.did_submit_in_last_frame()) {
    bool has_pending_tree = state_machine_.has_pending_tree();
    bool is_waiting_on_main = state_machine_.begin_main_frame_state() !=
                              SchedulerStateMachine::BeginMainFrameState::IDLE;
    bool is_draw_throttled =
        state_machine_.needs_redraw() && state_machine_.IsDrawThrottled();

    FrameSkippedReason reason = FrameSkippedReason::kNoDamage;

    if (is_waiting_on_main || has_pending_tree)
      reason = FrameSkippedReason::kWaitingOnMain;
    else if (is_draw_throttled)
      reason = FrameSkippedReason::kDrawThrottled;

    SendDidNotProduceFrame(begin_impl_frame_tracker_.Current(), reason);

    // If the current finished impl frame is not the last activated frame, but
    // the last activated frame has succeeded draw, it means that the drawn
    // frame would not be submitted and is causing no visible damage.
    if (begin_impl_frame_tracker_.Current().frame_id !=
            last_activate_origin_frame_args_.frame_id &&
        state_machine_.draw_succeeded_in_last_frame()) {
      compositor_frame_reporting_controller_->DidNotProduceFrame(
          last_activate_origin_frame_args_.frame_id,
          FrameSkippedReason::kNoDamage);
    }
  }

  begin_impl_frame_tracker_.Finish();

  ProcessScheduledActions();
  DCHECK(!inside_scheduled_action_);
  {
    base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);
    client_->DidFinishImplFrame(last_activate_origin_frame_args());
  }

  if (begin_frame_source_)
    begin_frame_source_->DidFinishFrame(this);
}

void Scheduler::SendDidNotProduceFrame(const viz::BeginFrameArgs& args,
                                       FrameSkippedReason reason) {
  if (last_begin_frame_ack_.frame_id == args.frame_id)
    return;
  last_begin_frame_ack_ = viz::BeginFrameAck(args, false /* has_damage */);
  client_->DidNotProduceFrame(last_begin_frame_ack_, reason);
  compositor_frame_reporting_controller_->DidNotProduceFrame(args.frame_id,
                                                             reason);
}

// BeginImplFrame starts a compositor frame that will wait up until a deadline
// for a BeginMainFrame+activation to complete before it times out and draws
// any asynchronous animation and scroll/pinch updates.
void Scheduler::BeginImplFrame(const viz::BeginFrameArgs& args,
                               base::TimeTicks now) {
  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BeginImplFrameState::IDLE);
  DCHECK(!begin_impl_frame_deadline_timer_.IsRunning());
  DCHECK(state_machine_.HasInitializedLayerTreeFrameSink());

  {
    DCHECK(!inside_scheduled_action_);
    base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);

    begin_impl_frame_tracker_.Start(args);
    state_machine_.OnBeginImplFrame(args.frame_id, args.animate_only);
    compositor_frame_reporting_controller_->WillBeginImplFrame(args);
    bool has_damage =
        client_->WillBeginImplFrame(begin_impl_frame_tracker_.Current());

    if (!has_damage)
      state_machine_.AbortDraw();
  }

  ProcessScheduledActions();
}

void Scheduler::ScheduleBeginImplFrameDeadline() {
  using DeadlineMode = SchedulerStateMachine::BeginImplFrameDeadlineMode;
  deadline_mode_ = state_machine_.CurrentBeginImplFrameDeadlineMode();

  base::TimeTicks new_deadline;
  switch (deadline_mode_) {
    case DeadlineMode::NONE:
      // NONE is returned when deadlines aren't used (synchronous compositor),
      // or when outside a begin frame. In either case deadline task shouldn't
      // be posted or should be cancelled already.
      DCHECK(!begin_impl_frame_deadline_timer_.IsRunning());
      return;
    case DeadlineMode::BLOCKED: {
      // TODO(sunnyps): Posting the deadline for pending begin frame is required
      // for browser compositor (commit_to_active_tree) to make progress in some
      // cases. Change browser compositor deadline to LATE in state machine to
      // fix this.
      //
      // TODO(sunnyps): Full pipeline mode should always go from blocking
      // deadline to triggering deadline immediately, but DCHECKing for this
      // causes layout test failures.
      bool has_pending_begin_frame = pending_begin_frame_args_.IsValid();
      if (has_pending_begin_frame) {
        new_deadline = base::TimeTicks();
        break;
      } else {
        begin_impl_frame_deadline_timer_.Stop();
        return;
      }
    }
    case DeadlineMode::LATE: {
      // We are waiting for a commit without needing active tree draw or we
      // have nothing to do.
      new_deadline = begin_impl_frame_tracker_.Current().frame_time +
                     begin_impl_frame_tracker_.Current().interval;
      // Send early DidNotProduceFrame if we don't expect to produce a frame
      // soon so that display scheduler doesn't wait unnecessarily.
      // Note: This will only send one DidNotProduceFrame ack per begin frame.
      if (!state_machine_.NewActiveTreeLikely()) {
        SendDidNotProduceFrame(begin_impl_frame_tracker_.Current(),
                               FrameSkippedReason::kNoDamage);
      }
      break;
    }
    case DeadlineMode::REGULAR:
      // We are animating the active tree but we're also waiting for commit.
      new_deadline = begin_impl_frame_tracker_.Current().deadline;
      break;
    case DeadlineMode::IMMEDIATE:
      // Avoid using Now() for immediate deadlines because it's expensive, and
      // this method is called in every ProcessScheduledActions() call. Using
      // base::TimeTicks() achieves the same result.
      new_deadline = base::TimeTicks();
      break;
    case DeadlineMode::WAIT_FOR_SCROLL:
      new_deadline = begin_impl_frame_tracker_.Current().frame_time +
                     begin_impl_frame_tracker_.Current().interval *
                         settings_.scroll_deadline_ratio;
      break;
  }

  // Post deadline task only if we didn't have one already or something caused
  // us to change the deadline.
  bool has_no_deadline_task = !begin_impl_frame_deadline_timer_.IsRunning();
  if (has_no_deadline_task || new_deadline != deadline_) {
    TRACE_EVENT2("cc", "Scheduler::ScheduleBeginImplFrameDeadline",
                 "new deadline", new_deadline, "deadline mode",
                 SchedulerStateMachine::BeginImplFrameDeadlineModeToString(
                     deadline_mode_));
    deadline_ = new_deadline;
    static const unsigned char* debug_tracing_enabled =
        TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
            TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"));
    if (debug_tracing_enabled)
      deadline_scheduled_at_ = Now();
    begin_impl_frame_deadline_timer_.Stop();
    begin_impl_frame_deadline_timer_.Start(
        FROM_HERE, deadline_,
        base::BindOnce(&Scheduler::OnBeginImplFrameDeadline,
                       base::Unretained(this)),
        base::subtle::DelayPolicy::kPrecise);
  }
}

void Scheduler::OnBeginImplFrameDeadline() {
  {
    TRACE_EVENT0("cc,benchmark", "Scheduler::OnBeginImplFrameDeadline");
    begin_impl_frame_deadline_timer_.Stop();
    // We split the deadline actions up into two phases so the state machine
    // has a chance to trigger actions that should occur during and after
    // the deadline separately. For example:
    // * Sending the BeginMainFrame will not occur after the deadline in
    //     order to wait for more user-input before starting the next commit.
    // * Creating a new OutputSurface will not occur during the deadline in
    //     order to allow the state machine to "settle" first.
    if (!settings_.using_synchronous_renderer_compositor) {
      compositor_timing_history_->WillFinishImplFrame(
          state_machine_.needs_redraw());
      compositor_frame_reporting_controller_->OnFinishImplFrame(
          begin_main_frame_args_.frame_id);
    }

    state_machine_.OnBeginImplFrameDeadline();
    client_->OnBeginImplFrameDeadline();
  }
  ProcessScheduledActions();

  if (settings_.using_synchronous_renderer_compositor)
    FinishImplFrameSynchronous();
  else
    FinishImplFrame();
}

void Scheduler::FinishImplFrameSynchronous() {
  DCHECK(settings_.using_synchronous_renderer_compositor);
  if (needs_finish_frame_for_synchronous_compositor_) {
    needs_finish_frame_for_synchronous_compositor_ = false;
    FinishImplFrame();
  }
}

void Scheduler::DrawIfPossible() {
  DCHECK(!inside_scheduled_action_);
  base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);
  compositor_timing_history_->WillDraw();
  state_machine_.WillDraw();
  DrawResult result = client_->ScheduledActionDrawIfPossible();
  state_machine_.DidDraw(result);
  compositor_timing_history_->DidDraw();
}

void Scheduler::DrawForced() {
  DCHECK(!inside_scheduled_action_);
  base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);
  bool drawing_with_new_active_tree =
      state_machine_.active_tree_needs_first_draw() &&
      !state_machine_.previous_pending_tree_was_impl_side();
  if (drawing_with_new_active_tree) {
    TRACE_EVENT_WITH_FLOW1(
        "viz,benchmark", "Graphics.Pipeline.DrawForced",
        TRACE_ID_GLOBAL(last_activate_origin_frame_args().trace_id),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "trace_id",
        last_activate_origin_frame_args().trace_id);
  }
  compositor_timing_history_->WillDraw();
  state_machine_.WillDraw();
  DrawResult result = client_->ScheduledActionDrawForced();
  state_machine_.DidDraw(result);
  compositor_timing_history_->DidDraw();
}

void Scheduler::UpdateDisplayTree() {
  DCHECK(!inside_scheduled_action_);
  base::AutoReset<bool> mark_inside(&inside_scheduled_action_, true);

  // TODO(rockot): Update CompositorTimingHistory.
  state_machine_.WillUpdateDisplayTree();
  client_->ScheduledActionUpdateDisplayTree();
}

void Scheduler::SetDeferBeginMainFrame(bool defer_begin_main_frame) {
  {
    TRACE_EVENT1("cc", "Scheduler::SetDeferBeginMainFrame",
                 "defer_begin_main_frame", defer_begin_main_frame);
    state_machine_.SetDeferBeginMainFrame(defer_begin_main_frame);
  }
  ProcessScheduledActions();
}

void Scheduler::SetPauseRendering(bool pause_rendering) {
  {
    TRACE_EVENT1("cc", "Scheduler::SetPauseRendering", "pause_rendering",
                 pause_rendering);
    state_machine_.SetPauseRendering(pause_rendering);
  }
  ProcessScheduledActions();
}

void Scheduler::SetMainThreadWantsBeginMainFrameNotExpected(bool new_state) {
  state_machine_.SetMainThreadWantsBeginMainFrameNotExpectedMessages(new_state);
  ProcessScheduledActions();
}

void Scheduler::ProcessScheduledActions() {
  // Do not perform actions during compositor shutdown.
  if (stopped_)
    return;

  // We do not allow ProcessScheduledActions to be recursive.
  // The top-level call will iteratively execute the next action for us anyway.
  if (inside_process_scheduled_actions_ || inside_scheduled_action_)
    return;

  base::AutoReset<bool> mark_inside(&inside_process_scheduled_actions_, true);

  SchedulerStateMachine::Action action;
  do {
    action = state_machine_.NextAction();
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                "SchedulerStateMachine", [this](perfetto::EventContext ctx) {
                  this->AsProtozeroInto(
                      ctx,
                      ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                          ->set_cc_scheduler_state());
                });
    base::AutoReset<SchedulerStateMachine::Action> mark_inside_action(
        &inside_action_, action);
    switch (action) {
      case SchedulerStateMachine::Action::NONE:
        break;
      case SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME:
        compositor_timing_history_->WillBeginMainFrame(begin_main_frame_args_);
        compositor_frame_reporting_controller_->WillBeginMainFrame(
            begin_main_frame_args_);
        state_machine_.WillSendBeginMainFrame();
        client_->ScheduledActionSendBeginMainFrame(begin_main_frame_args_);
        last_dispatched_begin_main_frame_args_ = begin_main_frame_args_;
        break;
      case SchedulerStateMachine::Action::
          NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL:
        state_machine_.WillNotifyBeginMainFrameNotExpectedUntil();
        BeginMainFrameNotExpectedUntil(begin_main_frame_args_.frame_time +
                                       begin_main_frame_args_.interval);
        break;
      case SchedulerStateMachine::Action::
          NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_SOON:
        state_machine_.WillNotifyBeginMainFrameNotExpectedSoon();
        BeginMainFrameNotExpectedSoon();
        break;
      case SchedulerStateMachine::Action::COMMIT:
        state_machine_.WillCommit(/*commit_had_no_updates=*/false);
        compositor_timing_history_->WillCommit();
        compositor_frame_reporting_controller_->WillCommit();
        client_->ScheduledActionCommit();
        compositor_timing_history_->DidCommit();
        compositor_frame_reporting_controller_->DidCommit();
        state_machine_.DidCommit();
        last_commit_origin_frame_args_ = next_commit_origin_frame_args_;
        break;
      case SchedulerStateMachine::Action::POST_COMMIT:
        client_->ScheduledActionPostCommit();
        state_machine_.DidPostCommit();
        break;
      case SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE:
        compositor_timing_history_->WillActivate();
        compositor_frame_reporting_controller_->WillActivate();
        state_machine_.WillActivate();
        client_->ScheduledActionActivateSyncTree();
        compositor_timing_history_->DidActivate();
        compositor_frame_reporting_controller_->DidActivate();
        last_activate_origin_frame_args_ = last_commit_origin_frame_args_;
        break;
      case SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION:
        state_machine_.WillPerformImplSideInvalidation();
        compositor_timing_history_->WillInvalidateOnImplSide();
        compositor_frame_reporting_controller_->WillInvalidateOnImplSide();
        client_->ScheduledActionPerformImplSideInvalidation();
        break;
      case SchedulerStateMachine::Action::DRAW_IF_POSSIBLE:
        DrawIfPossible();
        break;
      case SchedulerStateMachine::Action::DRAW_FORCED:
        DrawForced();
        break;
      case SchedulerStateMachine::Action::DRAW_ABORT:
        // No action is actually performed, but this allows the state machine to
        // drain the pipeline without actually drawing.
        state_machine_.AbortDraw();
        break;
      case SchedulerStateMachine::Action::UPDATE_DISPLAY_TREE:
        UpdateDisplayTree();
        break;
      case SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION:
        state_machine_.WillBeginLayerTreeFrameSinkCreation();
        client_->ScheduledActionBeginLayerTreeFrameSinkCreation();
        break;
      case SchedulerStateMachine::Action::PREPARE_TILES:
        state_machine_.WillPrepareTiles();
        client_->ScheduledActionPrepareTiles();
        break;
      case SchedulerStateMachine::Action::INVALIDATE_LAYER_TREE_FRAME_SINK:
        state_machine_.WillInvalidateLayerTreeFrameSink();
        client_->ScheduledActionInvalidateLayerTreeFrameSink(
            state_machine_.RedrawPending());
        break;
    }
  } while (action != SchedulerStateMachine::Action::NONE);

  ScheduleBeginImplFrameDeadline();

  PostPendingBeginFrameTask();
  StartOrStopBeginFrames();
}

void Scheduler::AsProtozeroInto(
    perfetto::EventContext& ctx,
    perfetto::protos::pbzero::ChromeCompositorSchedulerStateV2* state) const {
  base::TimeTicks now = Now();

  state_machine_.AsProtozeroInto(state->set_state_machine());

  state->set_observing_begin_frame_source(observing_begin_frame_source_);
  state->set_begin_impl_frame_deadline_task(
      begin_impl_frame_deadline_timer_.IsRunning());
  state->set_pending_begin_frame_task(!pending_begin_frame_task_.IsCancelled());
  state->set_skipped_last_frame_missed_exceeded_deadline(
      skipped_last_frame_missed_exceeded_deadline_);
  state->set_inside_action(
      SchedulerStateMachine::ActionToProtozeroEnum(inside_action_));
  state->set_deadline_mode(
      SchedulerStateMachine::BeginImplFrameDeadlineModeToProtozeroEnum(
          deadline_mode_));

  state->set_deadline_us(deadline_.since_origin().InMicroseconds());
  state->set_deadline_scheduled_at_us(
      deadline_scheduled_at_.since_origin().InMicroseconds());

  state->set_now_us(Now().since_origin().InMicroseconds());
  state->set_now_to_deadline_delta_us((deadline_ - Now()).InMicroseconds());
  state->set_now_to_deadline_scheduled_at_delta_us(
      (deadline_scheduled_at_ - Now()).InMicroseconds());

  begin_impl_frame_tracker_.AsProtozeroInto(ctx, now,
                                            state->set_begin_impl_frame_args());

  BeginFrameObserverBase::AsProtozeroInto(
      ctx, state->set_begin_frame_observer_state());

  if (begin_frame_source_) {
    begin_frame_source_->AsProtozeroInto(ctx,
                                         state->set_begin_frame_source_state());
  }
}

void Scheduler::UpdateCompositorTimingHistoryRecordingEnabled() {
  compositor_timing_history_->SetRecordingEnabled(
      state_machine_.HasInitializedLayerTreeFrameSink() &&
      state_machine_.visible());
}

size_t Scheduler::CommitDurationSampleCountForTesting() const {
  return compositor_timing_history_
      ->CommitDurationSampleCountForTesting();  // IN-TEST
}

viz::BeginFrameAck Scheduler::CurrentBeginFrameAckForActiveTree() const {
  return viz::BeginFrameAck(begin_main_frame_args_, true);
}

void Scheduler::ClearHistory() {
  // Ensure we reset decisions based on history from the previous navigation.
  compositor_timing_history_->ClearHistory();
  ProcessScheduledActions();
}

}  // namespace cc
