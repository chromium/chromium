// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_state_machine.h"

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/features.h"

namespace cc {

namespace {
// Surfaces and CompositorTimingHistory don't support more than 1 pending swap.
const int kMaxPendingSubmitFrames = 1;

}  // namespace

SchedulerStateMachine::SchedulerStateMachine(const SchedulerSettings& settings)
    : settings_(settings) {}

SchedulerStateMachine::~SchedulerStateMachine() = default;

perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MajorStateV2::
    LayerTreeFrameSinkState
    SchedulerStateMachine::LayerTreeFrameSinkStateToProtozeroEnum(
        LayerTreeFrameSinkState state) {
  using pbzeroMajorStateV2 =
      perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MajorStateV2;
  switch (state) {
    case LayerTreeFrameSinkState::NONE:
      return pbzeroMajorStateV2::LAYER_TREE_FRAME_NONE;
    case LayerTreeFrameSinkState::ACTIVE:
      return pbzeroMajorStateV2::LAYER_TREE_FRAME_ACTIVE;
    case LayerTreeFrameSinkState::CREATING:
      return pbzeroMajorStateV2::LAYER_TREE_FRAME_CREATING;
    case LayerTreeFrameSinkState::WAITING_FOR_FIRST_COMMIT:
      return pbzeroMajorStateV2::LAYER_TREE_FRAME_WAITING_FOR_FIRST_COMMIT;
    case LayerTreeFrameSinkState::WAITING_FOR_FIRST_ACTIVATION:
      return pbzeroMajorStateV2::LAYER_TREE_FRAME_WAITING_FOR_FIRST_ACTIVATION;
  }
  NOTREACHED();
}

perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MajorStateV2::
    BeginImplFrameState
    SchedulerStateMachine::BeginImplFrameStateToProtozeroEnum(
        BeginImplFrameState state) {
  using pbzeroMajorStateV2 =
      perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MajorStateV2;
  switch (state) {
    case BeginImplFrameState::IDLE:
      return pbzeroMajorStateV2::BEGIN_IMPL_FRAME_IDLE;
    case BeginImplFrameState::INSIDE_BEGIN_FRAME:
      return pbzeroMajorStateV2::BEGIN_IMPL_FRAME_INSIDE_BEGIN_FRAME;
    case BeginImplFrameState::INSIDE_DEADLINE:
      return pbzeroMajorStateV2::BEGIN_IMPL_FRAME_INSIDE_DEADLINE;
  }
  NOTREACHED();
}

const char* SchedulerStateMachine::BeginImplFrameDeadlineModeToString(
    BeginImplFrameDeadlineMode mode) {
  switch (mode) {
    case BeginImplFrameDeadlineMode::NONE:
      return "BeginImplFrameDeadlineMode::NONE";
    case BeginImplFrameDeadlineMode::IMMEDIATE:
      return "BeginImplFrameDeadlineMode::IMMEDIATE";
    case BeginImplFrameDeadlineMode::WAIT_FOR_SCROLL:
      return "BeginImplFrameDeadlineMode::WAIT_FOR_SCROLL";
    case BeginImplFrameDeadlineMode::REGULAR:
      return "BeginImplFrameDeadlineMode::REGULAR";
    case BeginImplFrameDeadlineMode::LATE:
      return "BeginImplFrameDeadlineMode::LATE";
    case BeginImplFrameDeadlineMode::BLOCKED:
      return "BeginImplFrameDeadlineMode::BLOCKED";
  }
  NOTREACHED();
}

perfetto::protos::pbzero::ChromeCompositorSchedulerStateV2::
    BeginImplFrameDeadlineMode
    SchedulerStateMachine::BeginImplFrameDeadlineModeToProtozeroEnum(
        BeginImplFrameDeadlineMode mode) {
  using pbzeroSchedulerState =
      perfetto::protos::pbzero::ChromeCompositorSchedulerStateV2;
  switch (mode) {
    case BeginImplFrameDeadlineMode::NONE:
      return pbzeroSchedulerState::DEADLINE_MODE_NONE;
    case BeginImplFrameDeadlineMode::IMMEDIATE:
      return pbzeroSchedulerState::DEADLINE_MODE_IMMEDIATE;
    case BeginImplFrameDeadlineMode::WAIT_FOR_SCROLL:
      return pbzeroSchedulerState::DEADLINE_MODE_WAIT_FOR_SCROLL;
    case BeginImplFrameDeadlineMode::REGULAR:
      return pbzeroSchedulerState::DEADLINE_MODE_REGULAR;
    case BeginImplFrameDeadlineMode::LATE:
      return pbzeroSchedulerState::DEADLINE_MODE_LATE;
    case BeginImplFrameDeadlineMode::BLOCKED:
      return pbzeroSchedulerState::DEADLINE_MODE_BLOCKED;
  }
  NOTREACHED();
}

perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MajorStateV2::
    BeginMainFrameState
    SchedulerStateMachine::BeginMainFrameStateToProtozeroEnum(
        BeginMainFrameState state) {
  using pbzeroMajorStateV2 =
      perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MajorStateV2;
  switch (state) {
    case BeginMainFrameState::IDLE:
      return pbzeroMajorStateV2::BEGIN_MAIN_FRAME_IDLE;
    case BeginMainFrameState::SENT:
      return pbzeroMajorStateV2::BEGIN_MAIN_FRAME_SENT;
    case BeginMainFrameState::READY_TO_COMMIT:
      return pbzeroMajorStateV2::BEGIN_MAIN_FRAME_READY_TO_COMMIT;
  }
  NOTREACHED();
}

perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MajorStateV2::
    ForcedRedrawOnTimeoutState
    SchedulerStateMachine::ForcedRedrawOnTimeoutStateToProtozeroEnum(
        ForcedRedrawOnTimeoutState state) {
  using pbzeroMajorStateV2 =
      perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MajorStateV2;
  switch (state) {
    case ForcedRedrawOnTimeoutState::IDLE:
      return pbzeroMajorStateV2::FORCED_REDRAW_IDLE;
    case ForcedRedrawOnTimeoutState::WAITING_FOR_COMMIT:
      return pbzeroMajorStateV2::FORCED_REDRAW_WAITING_FOR_COMMIT;
    case ForcedRedrawOnTimeoutState::WAITING_FOR_ACTIVATION:
      return pbzeroMajorStateV2::FORCED_REDRAW_WAITING_FOR_ACTIVATION;
    case ForcedRedrawOnTimeoutState::WAITING_FOR_DRAW:
      return pbzeroMajorStateV2::FORCED_REDRAW_WAITING_FOR_DRAW;
  }
  NOTREACHED();
}

perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MinorStateV2::
    ScrollHandlerState
    ScrollHandlerStateToProtozeroEnum(ScrollHandlerState state) {
  using pbzeroMinorStateV2 =
      perfetto::protos::pbzero::ChromeCompositorStateMachineV2::MinorStateV2;
  switch (state) {
    case ScrollHandlerState::SCROLL_AFFECTS_SCROLL_HANDLER:
      return pbzeroMinorStateV2::SCROLL_AFFECTS_SCROLL_HANDLER;
    case ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER:
      return pbzeroMinorStateV2::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER;
  }
  NOTREACHED();
}

perfetto::protos::pbzero::ChromeCompositorSchedulerActionV2
SchedulerStateMachine::ActionToProtozeroEnum(Action action) {
  using pbzeroSchedulerAction =
      perfetto::protos::pbzero::ChromeCompositorSchedulerActionV2;
  switch (action) {
    case Action::NONE:
      return pbzeroSchedulerAction::CC_SCHEDULER_ACTION_V2_NONE;
    case Action::SEND_BEGIN_MAIN_FRAME:
      return pbzeroSchedulerAction::
          CC_SCHEDULER_ACTION_V2_SEND_BEGIN_MAIN_FRAME;
    case Action::COMMIT:
      return pbzeroSchedulerAction::CC_SCHEDULER_ACTION_V2_COMMIT;
    case Action::POST_COMMIT:
      return pbzeroSchedulerAction::CC_SCHEDULER_ACTION_V2_POST_COMMIT;
    case Action::ACTIVATE_SYNC_TREE:
      return pbzeroSchedulerAction::CC_SCHEDULER_ACTION_V2_ACTIVATE_SYNC_TREE;
    case Action::DRAW_IF_POSSIBLE:
      return pbzeroSchedulerAction::CC_SCHEDULER_ACTION_V2_DRAW_IF_POSSIBLE;
    case Action::DRAW_FORCED:
      return pbzeroSchedulerAction::CC_SCHEDULER_ACTION_V2_DRAW_FORCED;
    case Action::DRAW_ABORT:
      return pbzeroSchedulerAction::CC_SCHEDULER_ACTION_V2_DRAW_ABORT;
    case Action::UPDATE_DISPLAY_TREE:
      return pbzeroSchedulerAction::CC_SCHEDULER_ACTION_V2_UPDATE_DISPLAY_TREE;
    case Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION:
      return pbzeroSchedulerAction::
          CC_SCHEDULER_ACTION_V2_BEGIN_LAYER_TREE_FRAME_SINK_CREATION;
    case Action::PREPARE_TILES:
      return pbzeroSchedulerAction::CC_SCHEDULER_ACTION_V2_PREPARE_TILES;
    case Action::INVALIDATE_LAYER_TREE_FRAME_SINK:
      return pbzeroSchedulerAction::
          CC_SCHEDULER_ACTION_V2_INVALIDATE_LAYER_TREE_FRAME_SINK;
    case Action::PERFORM_IMPL_SIDE_INVALIDATION:
      return pbzeroSchedulerAction::
          CC_SCHEDULER_ACTION_V2_PERFORM_IMPL_SIDE_INVALIDATION;
    case Action::NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL:
      return pbzeroSchedulerAction::
          CC_SCHEDULER_ACTION_V2_NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL;
    case Action::NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_SOON:
      return pbzeroSchedulerAction::
          CC_SCHEDULER_ACTION_V2_NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_SOON;
  }
  NOTREACHED();
}

void SchedulerStateMachine::AsProtozeroInto(
    perfetto::protos::pbzero::ChromeCompositorStateMachineV2* state) const {
  auto* major_state = state->set_major_state();
  major_state->set_next_action(ActionToProtozeroEnum(NextAction()));
  major_state->set_begin_impl_frame_state(
      BeginImplFrameStateToProtozeroEnum(begin_impl_frame_state_));
  major_state->set_begin_main_frame_state(
      BeginMainFrameStateToProtozeroEnum(begin_main_frame_state_));
  major_state->set_layer_tree_frame_sink_state(
      LayerTreeFrameSinkStateToProtozeroEnum(layer_tree_frame_sink_state_));
  major_state->set_forced_redraw_state(
      ForcedRedrawOnTimeoutStateToProtozeroEnum(forced_redraw_state_));

  auto* minor_state = state->set_minor_state();
  minor_state->set_commit_count(commit_count_);
  minor_state->set_current_frame_number(current_frame_number_);
  minor_state->set_last_frame_number_submit_performed(
      last_frame_number_submit_performed_);
  minor_state->set_last_frame_number_draw_performed(
      last_frame_number_draw_performed_);
  minor_state->set_last_frame_number_begin_main_frame_sent(
      last_frame_number_begin_main_frame_sent_);
  minor_state->set_did_draw(did_draw_);
  minor_state->set_did_send_begin_main_frame_for_current_frame(
      did_send_begin_main_frame_for_current_frame_);
  minor_state->set_did_notify_begin_main_frame_not_expected_until(
      did_notify_begin_main_frame_not_expected_until_);
  minor_state->set_did_notify_begin_main_frame_not_expected_soon(
      did_notify_begin_main_frame_not_expected_soon_);
  minor_state->set_wants_begin_main_frame_not_expected(
      wants_begin_main_frame_not_expected_);
  minor_state->set_did_commit_during_frame(did_commit_during_frame_);
  minor_state->set_did_invalidate_layer_tree_frame_sink(
      did_invalidate_layer_tree_frame_sink_);
  minor_state->set_did_perform_impl_side_invalidaion(
      did_perform_impl_side_invalidation_);
  minor_state->set_did_prepare_tiles(did_prepare_tiles_);
  minor_state->set_consecutive_checkerboard_animations(
      consecutive_checkerboard_animations_);
  minor_state->set_pending_submit_frames(pending_submit_frames_);
  minor_state->set_submit_frames_with_current_layer_tree_frame_sink(
      submit_frames_with_current_layer_tree_frame_sink_);
  minor_state->set_needs_redraw(needs_redraw_);
  minor_state->set_needs_prepare_tiles(needs_prepare_tiles_);
  minor_state->set_needs_begin_main_frame(needs_begin_main_frame_);
  minor_state->set_needs_one_begin_impl_frame(needs_one_begin_impl_frame_);
  minor_state->set_visible(visible_);
  minor_state->set_begin_frame_source_paused(begin_frame_source_paused_);
  minor_state->set_can_draw(can_draw_);
  minor_state->set_resourceless_draw(resourceless_draw_);
  minor_state->set_has_pending_tree(has_pending_tree_);
  minor_state->set_pending_tree_is_ready_for_activation(
      pending_tree_is_ready_for_activation_);
  minor_state->set_active_tree_needs_first_draw(active_tree_needs_first_draw_);
  minor_state->set_active_tree_is_ready_to_draw(active_tree_is_ready_to_draw_);
  minor_state->set_did_create_and_initialize_first_layer_tree_frame_sink(
      did_create_and_initialize_first_layer_tree_frame_sink_);
  minor_state->set_tree_priority(TreePriorityToProtozeroEnum(tree_priority_));
  minor_state->set_scroll_handler_state(
      ScrollHandlerStateToProtozeroEnum(scroll_handler_state_));
  minor_state->set_critical_begin_main_frame_to_activate_is_fast(
      critical_begin_main_frame_to_activate_is_fast_);
  minor_state->set_main_thread_missed_last_deadline(
      main_thread_missed_last_deadline_);
  minor_state->set_video_needs_begin_frames(video_needs_begin_frames_);
  minor_state->set_defer_begin_main_frame(defer_begin_main_frame_);
  minor_state->set_last_commit_had_no_updates(last_commit_had_no_updates_);
  minor_state->set_did_draw_in_last_frame(did_attempt_draw_in_last_frame_);
  minor_state->set_did_submit_in_last_frame(did_submit_in_last_frame_);
  minor_state->set_needs_impl_side_invalidation(needs_impl_side_invalidation_);
  minor_state->set_current_pending_tree_is_impl_side(
      current_pending_tree_is_impl_side_);
  minor_state->set_previous_pending_tree_was_impl_side(
      previous_pending_tree_was_impl_side_);
  minor_state->set_processing_animation_worklets_for_active_tree(
      processing_animation_worklets_for_active_tree_);
  minor_state->set_processing_animation_worklets_for_pending_tree(
      processing_animation_worklets_for_pending_tree_);
  minor_state->set_processing_paint_worklets_for_pending_tree(
      processing_paint_worklets_for_pending_tree_);
  minor_state->set_processing_paint_worklets_for_pending_tree(should_warm_up_);
}

bool SchedulerStateMachine::PendingDrawsShouldBeAborted() const {
  // Normally when |visible_| is false or |begin_frame_source_paused_| is true,
  // pending activations will be forced and draws will be aborted. However,
  // when the embedder is Android WebView, software draws could be scheduled by
  // the Android OS at any time and draws should not be aborted in this case.
  bool is_layer_tree_frame_sink_lost =
      (layer_tree_frame_sink_state_ == LayerTreeFrameSinkState::NONE);
  if (resourceless_draw_)
    return is_layer_tree_frame_sink_lost || !can_draw_;

  // These are all the cases where we normally cannot or do not want
  // to draw but, if |needs_redraw_| is true and we do not draw to
  // make forward progress, we might deadlock with the main
  // thread. This should be a superset of ShouldAbortCurrentFrame()
  // since activation of the pending tree is blocked by drawing of the
  // active tree and the main thread might be blocked on activation of
  // the most recent commit.
  return is_layer_tree_frame_sink_lost || !can_draw_ || !visible_ ||
         begin_frame_source_paused_ ||
         waiting_for_activation_after_rendering_resumed_;
}

bool SchedulerStateMachine::ShouldAbortCurrentFrame() const {
  // Abort the frame if there is no output surface to trigger our
  // activations, avoiding deadlock with the main thread.
  if (layer_tree_frame_sink_state_ == LayerTreeFrameSinkState::NONE)
    return true;

  // If we're not visible, we should just abort the frame. Since we
  // set RequiresHighResToDraw when becoming visible, we ensure that
  // we don't checkerboard until all visible resources are
  // done. Furthermore, if we do keep the pending tree around, when
  // becoming visible we might activate prematurely causing
  // RequiresHighResToDraw flag to be reset. In all cases, we can
  // simply activate on becoming invisible since we don't need to draw
  // the active tree when we're in this state.
  if (!visible_)
    return true;

  // Abort the frame when viz::BeginFrameSource is paused to avoid
  // deadlocking the main thread.
  if (begin_frame_source_paused_)
    return true;

  return false;
}

bool SchedulerStateMachine::ShouldBeginLayerTreeFrameSinkCreation() const {
  if (!should_warm_up_ && !visible_) {
    return false;
  }

  // We only want to start output surface initialization after the
  // previous commit is complete.
  if (begin_main_frame_state_ != BeginMainFrameState::IDLE ||
      next_begin_main_frame_state_ != BeginMainFrameState::IDLE) {
    return false;
  }

  // Make sure the BeginImplFrame from any previous LayerTreeFrameSinks
  // are complete before creating the new LayerTreeFrameSink.
  if (begin_impl_frame_state_ != BeginImplFrameState::IDLE)
    return false;

  // We want to clear the pipeline of any pending draws and activations
  // before starting output surface initialization. This allows us to avoid
  // weird corner cases where we abort draws or force activation while we
  // are initializing the output surface.
  if (active_tree_needs_first_draw_ || has_pending_tree_)
    return false;

  // We need to create the output surface if we don't have one and we haven't
  // started creating one yet.
  return layer_tree_frame_sink_state_ == LayerTreeFrameSinkState::NONE;
}

bool SchedulerStateMachine::ShouldDraw() const {
  if (settings_.use_layer_context_for_display) {
    return false;
  }

  // If we need to abort draws, we should do so ASAP since the draw could
  // be blocking other important actions (like output surface initialization),
  // from occurring. If we are waiting for the first draw, then perform the
  // aborted draw to keep things moving. If we are not waiting for the first
  // draw however, we don't want to abort for no reason.
  if (PendingDrawsShouldBeAborted())
    return active_tree_needs_first_draw_;

  // Do not draw more than once in the deadline. Aborted draws are ok because
  // those are effectively nops.
  if (did_draw_)
    return false;

  // Don't draw if an early check determined the frame does not have damage.
  if (skip_draw_)
    return false;

  // Don't draw if we are waiting on the first commit after a surface.
  if (layer_tree_frame_sink_state_ != LayerTreeFrameSinkState::ACTIVE)
    return false;

  // Do not queue too many draws.
  if (IsDrawThrottled())
    return false;

  // Except for the cases above, do not draw outside of the BeginImplFrame
  // deadline.
  if (begin_impl_frame_state_ != BeginImplFrameState::INSIDE_DEADLINE)
    return false;

  // Wait for ready to draw in full-pipeline mode or the browser compositor's
  // commit-to-active-tree mode.
  if ((settings_.wait_for_all_pipeline_stages_before_draw ||
       settings_.commit_to_active_tree) &&
      !active_tree_is_ready_to_draw_) {
    return false;
  }

  // Browser compositor commit steals any resources submitted in draw. Therefore
  // drawing while a commit is pending is wasteful.
  if (settings_.commit_to_active_tree && CommitPending())
    return false;

  // Only handle forced redraws due to timeouts on the regular deadline.
  if (forced_redraw_state_ == ForcedRedrawOnTimeoutState::WAITING_FOR_DRAW)
    return true;

  return needs_redraw_;
}

bool SchedulerStateMachine::ShouldUpdateDisplayTree() const {
  if (!settings_.use_layer_context_for_display) {
    return false;
  }

  if (did_update_display_tree_) {
    return false;
  }

  if (layer_tree_frame_sink_state_ != LayerTreeFrameSinkState::ACTIVE) {
    return false;
  }

  return needs_update_display_tree_;
}

bool SchedulerStateMachine::ShouldActivateSyncTree() const {
  // There is nothing to activate.
  if (!has_pending_tree_) {
    TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "Not activating sync tree due to no pending tree",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // We should not activate a second tree before drawing the first one.
  // Even if we need to force activation of the pending tree, we should abort
  // drawing the active tree first. Relax this requirement for synchronous
  // compositor where scheduler does not control draw, and blocking commit
  // may lead to bad scheduling.
  if (!settings_.using_synchronous_renderer_compositor &&
      active_tree_needs_first_draw_) {
    TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "Not activating before drawing active first",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // Delay pending tree activation until paint worklets have completed painting
  // the pending tree. This must occur before the |ShouldAbortCurrentFrame|
  // check as we cannot have an unpainted active tree.
  //
  // Note that paint worklets continue to paint when the page is not visible, so
  // any abort will eventually happen when they complete.
  if (processing_paint_worklets_for_pending_tree_) {
    TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "Not activating due to processing paint worklets",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (ShouldAbortCurrentFrame())
    return true;

  // Delay pending tree activation until animation worklets have completed
  // their asynchronous updates to pick up initial values.
  if (processing_animation_worklets_for_pending_tree_) {
    TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "Not activating due to processing animation worklets",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // At this point, only activate if we are ready to activate.
  if (!pending_tree_is_ready_for_activation_) {
    TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "Not activating because pending tree not ready",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  return true;
}

bool SchedulerStateMachine::ShouldNotifyBeginMainFrameNotExpectedUntil() const {
  // This method returns true if most of the conditions for sending a
  // BeginMainFrame are met, but one is not actually requested. This gives the
  // main thread the chance to do something else.

  if (!wants_begin_main_frame_not_expected_)
    return false;

  // Don't notify if a BeginMainFrame has already been requested or is in
  // progress.
  if (needs_begin_main_frame_ ||
      begin_main_frame_state_ != BeginMainFrameState::IDLE ||
      next_begin_main_frame_state_ != BeginMainFrameState::IDLE) {
    return false;
  }

  // Only notify when we're visible.
  if (!visible_)
    return false;

  // There are no BeginImplFrames while viz::BeginFrameSource is paused, meaning
  // the scheduler should send SendBeginMainFrameNotExpectedSoon instead,
  // indicating a longer period of inactivity.
  if (begin_frame_source_paused_)
    return false;

  // If we've gone idle and have stopped getting BeginFrames, we should send
  // SendBeginMainFrameNotExpectedSoon instead.
  if (!BeginFrameNeeded() &&
      begin_impl_frame_state_ == BeginImplFrameState::IDLE) {
    return false;
  }

  // Do not notify that no BeginMainFrame was sent too many times in a single
  // frame.
  if (did_notify_begin_main_frame_not_expected_until_)
    return false;

  // Do not notify if a commit happened during this frame as the main thread
  // will already be active and does not need to be woken up to make further
  // actions. (This occurs if the main frame was scheduled but didn't complete
  // before the vsync deadline).
  if (did_commit_during_frame_)
    return false;

  return true;
}

bool SchedulerStateMachine::ShouldNotifyBeginMainFrameNotExpectedSoon() const {
  if (!wants_begin_main_frame_not_expected_)
    return false;

  // Don't notify if a BeginMainFrame has already been requested or is in
  // progress.
  if (needs_begin_main_frame_ ||
      begin_main_frame_state_ != BeginMainFrameState::IDLE ||
      next_begin_main_frame_state_ != BeginMainFrameState::IDLE) {
    return false;
  }

  // Only send this when we've stopped getting BeginFrames and have gone idle.
  if (BeginFrameNeeded() ||
      begin_impl_frame_state_ != BeginImplFrameState::IDLE) {
    return false;
  }

  // Do not notify that we're not expecting frames more than once per frame.
  if (did_notify_begin_main_frame_not_expected_soon_)
    return false;

  return true;
}

bool SchedulerStateMachine::CouldSendBeginMainFrame() const {
  if (!needs_begin_main_frame_)
    return false;

  // We can not perform commits if we are not visible.
  if (!visible_)
    return false;

  // There are no BeginImplFrames while viz::BeginFrameSource is paused,
  // so should also stop BeginMainFrames.
  if (begin_frame_source_paused_)
    return false;

  // Do not send begin main frame when it is deferred.
  if (defer_begin_main_frame_)
    return false;

  // Do not send begin main frames if we want to pause rendering.
  if (pause_rendering_)
    return false;

  return true;
}

bool SchedulerStateMachine::ShouldSendBeginMainFrame() const {
  if (!CouldSendBeginMainFrame())
    return false;

  // Do not send more than one begin main frame in a begin frame.
  if (did_send_begin_main_frame_for_current_frame_)
    return false;

  // Only send BeginMainFrame when there isn't another commit pending already.
  // Other parts of the state machine indirectly defer the BeginMainFrame
  // by transitioning to WAITING commit states rather than going
  // immediately to IDLE.
  switch (begin_main_frame_state_) {
    case BeginMainFrameState::IDLE:
      break;
    case BeginMainFrameState::SENT:
      return false;
    case BeginMainFrameState::READY_TO_COMMIT:
      if (!settings_.main_frame_before_commit_enabled ||
          next_begin_main_frame_state_ != BeginMainFrameState::IDLE) {
        return false;
      }
      break;
  }

  // MFBA is disabled and we are waiting for previous activation, or the current
  // pending tree is impl-side.
  bool can_send_main_frame_with_pending_tree =
      settings_.main_frame_before_activation_enabled ||
      current_pending_tree_is_impl_side_;
  if (has_pending_tree_ && !can_send_main_frame_with_pending_tree)
    return false;

  // We are waiting for previous frame to be drawn, submitted and acked.
  if (settings_.commit_to_active_tree &&
      (active_tree_needs_first_draw_ || IsDrawThrottled())) {
    return false;
  }

  // Don't send BeginMainFrame early if we are prioritizing a committed
  // active tree because of ImplLatencyTakesPriority.
  if (ImplLatencyTakesPriority() &&
      ((has_pending_tree_ && !current_pending_tree_is_impl_side_) ||
       (active_tree_needs_first_draw_ &&
        !previous_pending_tree_was_impl_side_))) {
    return false;
  }

  // We should not send BeginMainFrame while we are in the idle state since we
  // might have new user input arriving soon. It's okay to send BeginMainFrame
  // for the synchronous compositor because the main thread is always high
  // latency in that case.
  // TODO(brianderson): Allow sending BeginMainFrame while idle when the main
  // thread isn't consuming user input for non-synchronous compositor.
  if (!settings_.using_synchronous_renderer_compositor &&
      begin_impl_frame_state_ == BeginImplFrameState::IDLE) {
    return false;
  }

  // We need a new commit for the forced redraw. This honors the
  // single commit per interval because the result will be swapped to screen.
  if (forced_redraw_state_ == ForcedRedrawOnTimeoutState::WAITING_FOR_COMMIT)
    return true;

  // We shouldn't normally accept commits if there isn't a LayerTreeFrameSink.
  if (!HasInitializedLayerTreeFrameSink())
    return false;

  // Throttle the BeginMainFrames on CompositorFrameAck unless we just
  // submitted a frame to potentially improve impl-thread latency over
  // main-thread throughput.
  // TODO(brianderson): Remove this restriction to improve throughput or
  // make it conditional on ImplLatencyTakesPriority.
  bool just_submitted_in_deadline =
      begin_impl_frame_state_ == BeginImplFrameState::INSIDE_DEADLINE &&
      did_submit_in_last_frame_;
  if (IsDrawThrottled() && !just_submitted_in_deadline)
    return false;

  // We should wait for scroll events to arrive before sending the
  // BeginMainFrame. So that the most up to date scroll positions are available
  // for main-thread effects.
  if (ShouldWaitForScrollEvent()) {
    return false;
  }

  return true;
}

bool SchedulerStateMachine::ShouldCommit() const {
  if (begin_main_frame_state_ != BeginMainFrameState::READY_TO_COMMIT)
    return false;

  // We must not finish the commit until the pending tree is free.
  if (has_pending_tree_) {
    DCHECK(settings_.main_frame_before_activation_enabled ||
           settings_.main_frame_before_commit_enabled ||
           current_pending_tree_is_impl_side_);
    return false;
  }

  // If we only have an active tree, it is incorrect to replace it before we've
  // drawn it.
  DCHECK(!settings_.commit_to_active_tree || !active_tree_needs_first_draw_);

  // In browser compositor commit reclaims any resources submitted during draw.
  DCHECK(!settings_.commit_to_active_tree || !IsDrawThrottled());

  return true;
}

void SchedulerStateMachine::DidCommit() {
  DCHECK(!needs_post_commit_);
  needs_post_commit_ = true;
}

bool SchedulerStateMachine::ShouldRunPostCommit() const {
  return needs_post_commit_;
}

void SchedulerStateMachine::DidPostCommit() {
  DCHECK(needs_post_commit_);
  needs_post_commit_ = false;
}

bool SchedulerStateMachine::ShouldPrepareTiles() const {
  // In full-pipeline mode, we need to prepare tiles ASAP to ensure that we
  // don't get stuck.
  if (settings_.wait_for_all_pipeline_stages_before_draw)
    return needs_prepare_tiles_;

  // Do not prepare tiles if we've already done so in commit or impl side
  // invalidation.
  if (did_prepare_tiles_)
    return false;

  // Limiting to once per-frame is not enough, since we only want to prepare
  // tiles _after_ draws.
  if (begin_impl_frame_state_ != BeginImplFrameState::INSIDE_DEADLINE)
    return false;

  return needs_prepare_tiles_;
}

bool SchedulerStateMachine::ShouldInvalidateLayerTreeFrameSink() const {
  // Do not invalidate more than once per begin frame.
  if (did_invalidate_layer_tree_frame_sink_)
    return false;

  // Only the synchronous compositor requires invalidations.
  if (!settings_.using_synchronous_renderer_compositor)
    return false;

  // Invalidations are only performed inside a BeginFrame.
  if (begin_impl_frame_state_ != BeginImplFrameState::INSIDE_BEGIN_FRAME)
    return false;

  // Don't invalidate for draw if we cannnot draw.
  // TODO(sunnyps): needs_prepare_tiles_ is needed here because PrepareTiles is
  // called only inside the deadline / draw phase. We could remove this if we
  // allowed PrepareTiles to happen in OnBeginImplFrame.
  return (needs_redraw_ && !PendingDrawsShouldBeAborted()) ||
         needs_prepare_tiles_;
}

SchedulerStateMachine::Action SchedulerStateMachine::NextAction() const {
  if (ShouldSendBeginMainFrame())
    return Action::SEND_BEGIN_MAIN_FRAME;
  if (ShouldRunPostCommit())
    return Action::POST_COMMIT;
  if (ShouldActivateSyncTree())
    return Action::ACTIVATE_SYNC_TREE;
  if (ShouldCommit())
    return Action::COMMIT;
  if (ShouldDraw()) {
    if (PendingDrawsShouldBeAborted())
      return Action::DRAW_ABORT;
    else if (forced_redraw_state_ ==
             ForcedRedrawOnTimeoutState::WAITING_FOR_DRAW)
      return Action::DRAW_FORCED;
    else
      return Action::DRAW_IF_POSSIBLE;
  }
  if (ShouldUpdateDisplayTree()) {
    return Action::UPDATE_DISPLAY_TREE;
  }
  if (ShouldPerformImplSideInvalidation())
    return Action::PERFORM_IMPL_SIDE_INVALIDATION;
  if (ShouldPrepareTiles())
    return Action::PREPARE_TILES;
  if (ShouldInvalidateLayerTreeFrameSink())
    return Action::INVALIDATE_LAYER_TREE_FRAME_SINK;
  if (ShouldBeginLayerTreeFrameSinkCreation())
    return Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION;
  if (ShouldNotifyBeginMainFrameNotExpectedUntil())
    return Action::NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL;
  if (ShouldNotifyBeginMainFrameNotExpectedSoon())
    return Action::NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_SOON;
  return Action::NONE;
}

bool SchedulerStateMachine::ShouldPerformImplSideInvalidation() const {
  if (pause_rendering_)
    return false;

  if (begin_frame_is_animate_only_)
    return false;

  if (!needs_impl_side_invalidation_)
    return false;

  // Don't invalidate if we've already done so either from the scheduler or as
  // part of commit.
  if (did_perform_impl_side_invalidation_)
    return false;

  // No invalidations should be done outside the impl frame.
  if (begin_impl_frame_state_ == BeginImplFrameState::IDLE)
    return false;

  // We need to be able to create a pending tree to perform an invalidation.
  if (!CouldCreatePendingTree())
    return false;

  // Check if we should defer invalidating so we can merge these invalidations
  // with the main frame.
  if (ShouldDeferInvalidatingForMainFrame())
    return false;

  // If invalidations go to the active tree and we are waiting for the previous
  // frame to be drawn, submitted and acked.
  if (settings_.commit_to_active_tree &&
      (active_tree_needs_first_draw_ || IsDrawThrottled())) {
    return false;
  }

  return true;
}

bool SchedulerStateMachine::ShouldDeferInvalidatingForMainFrame() const {
  DCHECK_NE(begin_impl_frame_state_, BeginImplFrameState::IDLE);

  // If the main thread is ready to commit, the impl-side invalidations will be
  // merged with the incoming main frame.
  if (begin_main_frame_state_ == BeginMainFrameState::READY_TO_COMMIT)
    return true;

  // If commits are being aborted (which would be the common case for a
  // compositor scroll), don't defer the invalidation.
  if (last_frame_events_.commit_had_no_updates || last_commit_had_no_updates_)
    return false;

  // If we prefer to invalidate over waiting on the main frame, do the
  // invalidation now.
  if (!should_defer_invalidation_for_fast_main_frame_)
    return false;

  // If there is a request for a main frame, then this could either be a
  // request that we need to respond to in this impl frame or its possible the
  // request is for the next frame (a rAF issued at the beginning of the current
  // main frame). In either case, defer invalidating so we can merge it with the
  // main frame.
  if (needs_begin_main_frame_)
    return true;

  // If the main frame was already sent, wait for the main thread to respond.
  if (begin_main_frame_state_ == BeginMainFrameState::SENT)
    return true;

  // If the main thread committed during the last frame, i.e. it was not
  // aborted, then we might get another main frame request later in the impl
  // frame. This could be the case for a timer based animation running on the
  // main thread which doesn't align with our vsync. For such cases,
  // conservatively defer invalidating until the deadline.
  if (last_frame_events_.did_commit_during_frame)
    return true;

  // If the main thread is not requesting any frames, perform the invalidation
  // at the beginning of the impl frame.
  return false;
}

void SchedulerStateMachine::WillPerformImplSideInvalidation() {
  current_pending_tree_is_impl_side_ = true;
  WillPerformImplSideInvalidationInternal();
}

void SchedulerStateMachine::WillPerformImplSideInvalidationInternal() {
  DCHECK(needs_impl_side_invalidation_);
  DCHECK(!has_pending_tree_);

  needs_impl_side_invalidation_ = false;
  has_pending_tree_ = true;
  did_perform_impl_side_invalidation_ = true;
  pending_tree_needs_first_draw_on_activation_ =
      next_invalidation_needs_first_draw_on_activation_;
  next_invalidation_needs_first_draw_on_activation_ = false;
  // TODO(eseckler): Track impl-side invalidations for pending/active tree and
  // CompositorFrame freshness computation.
}

bool SchedulerStateMachine::CouldCreatePendingTree() const {
  // Can't create a new pending tree till the current one is activated.
  if (has_pending_tree_)
    return false;

  // Can't make frames while we're invisible.
  if (!visible_)
    return false;

  // If the viz::BeginFrameSource is paused, we will not be able to make any
  // impl frames.
  if (begin_frame_source_paused_)
    return false;

  // Don't create a pending tree till a frame sink is fully initialized.  Check
  // for the ACTIVE state explicitly instead of calling
  // HasInitializedLayerTreeFrameSink() because that only checks if frame sink
  // has been recreated, but doesn't check if we're waiting for first commit or
  // activation.
  if (layer_tree_frame_sink_state_ != LayerTreeFrameSinkState::ACTIVE)
    return false;

  return true;
}

void SchedulerStateMachine::WillSendBeginMainFrame() {
  DCHECK(!has_pending_tree_ || settings_.main_frame_before_activation_enabled ||
         current_pending_tree_is_impl_side_);
  DCHECK(visible_);
  DCHECK(!begin_frame_source_paused_);
  DCHECK(!did_send_begin_main_frame_for_current_frame_);
  if (begin_main_frame_state_ == BeginMainFrameState::IDLE) {
    begin_main_frame_state_ = BeginMainFrameState::SENT;
  } else {
    // We are sending BMF to the main thread while the previous BMF has not yet
    // finished commit on the impl thread.
    DCHECK(settings_.main_frame_before_commit_enabled);
    DCHECK_EQ(begin_main_frame_state_, BeginMainFrameState::READY_TO_COMMIT);
    DCHECK_EQ(next_begin_main_frame_state_, BeginMainFrameState::IDLE);
    next_begin_main_frame_state_ = BeginMainFrameState::SENT;
  }
  needs_begin_main_frame_ = false;
  did_send_begin_main_frame_for_current_frame_ = true;
  // TODO(szager): Make sure this doesn't break perfetto
  last_frame_number_begin_main_frame_sent_ = current_frame_number_;
}

void SchedulerStateMachine::WillNotifyBeginMainFrameNotExpectedUntil() {
  DCHECK(visible_);
  DCHECK(!begin_frame_source_paused_);
  DCHECK(BeginFrameNeeded() ||
         begin_impl_frame_state_ != BeginImplFrameState::IDLE);
  DCHECK(!did_notify_begin_main_frame_not_expected_until_);
  did_notify_begin_main_frame_not_expected_until_ = true;
}

void SchedulerStateMachine::WillNotifyBeginMainFrameNotExpectedSoon() {
  DCHECK(!BeginFrameNeeded());
  DCHECK(begin_impl_frame_state_ == BeginImplFrameState::IDLE);
  DCHECK(!did_notify_begin_main_frame_not_expected_soon_);
  did_notify_begin_main_frame_not_expected_soon_ = true;
}

void SchedulerStateMachine::WillCommit(bool commit_has_no_updates) {
  bool can_have_pending_tree =
      commit_has_no_updates &&
      (settings_.main_frame_before_activation_enabled ||
       settings_.main_frame_before_commit_enabled ||
       current_pending_tree_is_impl_side_);
  DCHECK(!has_pending_tree_ || can_have_pending_tree);
  DCHECK(settings_.main_frame_before_commit_enabled ||
         next_begin_main_frame_state_ == BeginMainFrameState::IDLE);
  DCHECK_LT(next_begin_main_frame_state_, BeginMainFrameState::READY_TO_COMMIT);
  commit_count_++;
  if (commit_has_no_updates) {
    // Primary BMF was aborted, cannot have a pipelined BMF
    DCHECK_EQ(next_begin_main_frame_state_, BeginMainFrameState::IDLE);
    begin_main_frame_state_ = BeginMainFrameState::IDLE;
  } else {
    // Move the pipelined BMF state into the primary slot being vacated.
    DCHECK(settings_.main_frame_before_commit_enabled ||
           next_begin_main_frame_state_ == BeginMainFrameState::IDLE);
    DCHECK_NE(next_begin_main_frame_state_,
              BeginMainFrameState::READY_TO_COMMIT);
    begin_main_frame_state_ = next_begin_main_frame_state_;
    next_begin_main_frame_state_ = BeginMainFrameState::IDLE;
  }
  last_commit_had_no_updates_ = commit_has_no_updates;
  did_commit_during_frame_ = true;

  if (!commit_has_no_updates) {
    // If there was a commit, the impl-side invalidations will be merged with
    // it. We always fill the impl-side invalidation funnel here, even if no
    // request was currently pending, to defer creating another pending tree and
    // performing PrepareTiles until the next frame, in case the invalidation
    // request is received after the commit.
    if (needs_impl_side_invalidation_)
      WillPerformImplSideInvalidationInternal();
    did_perform_impl_side_invalidation_ = true;

    // We have a new pending tree.
    has_pending_tree_ = true;
    pending_tree_needs_first_draw_on_activation_ = true;
    pending_tree_is_ready_for_activation_ = false;
    if (!active_tree_needs_first_draw_ ||
        !settings_.wait_for_all_pipeline_stages_before_draw) {
      // Wait for the new pending tree to become ready to draw, which may happen
      // before or after activation (unless we're in full-pipeline mode and
      // need first draw to come through).
      active_tree_is_ready_to_draw_ = false;
    }
  }

  // Update state related to forced draws.
  if (forced_redraw_state_ == ForcedRedrawOnTimeoutState::WAITING_FOR_COMMIT) {
    forced_redraw_state_ =
        has_pending_tree_ ? ForcedRedrawOnTimeoutState::WAITING_FOR_ACTIVATION
                          : ForcedRedrawOnTimeoutState::WAITING_FOR_DRAW;
  }

  // Update the output surface state.
  if (layer_tree_frame_sink_state_ ==
      LayerTreeFrameSinkState::WAITING_FOR_FIRST_COMMIT) {
    layer_tree_frame_sink_state_ =
        has_pending_tree_
            ? LayerTreeFrameSinkState::WAITING_FOR_FIRST_ACTIVATION
            : LayerTreeFrameSinkState::ACTIVE;
  }
}

void SchedulerStateMachine::WillActivate() {
  // We cannot activate the pending tree while paint worklets are still being
  // processed; the pending tree *must* be fully painted before it can ever be
  // activated because we cannot paint the active tree.
  DCHECK(!processing_paint_worklets_for_pending_tree_);

  if (layer_tree_frame_sink_state_ ==
      LayerTreeFrameSinkState::WAITING_FOR_FIRST_ACTIVATION)
    layer_tree_frame_sink_state_ = LayerTreeFrameSinkState::ACTIVE;

  if (forced_redraw_state_ ==
      ForcedRedrawOnTimeoutState::WAITING_FOR_ACTIVATION)
    forced_redraw_state_ = ForcedRedrawOnTimeoutState::WAITING_FOR_DRAW;

  has_pending_tree_ = false;
  pending_tree_is_ready_for_activation_ = false;
  if (settings_.use_layer_context_for_display) {
    needs_update_display_tree_ = true;
    did_update_display_tree_ = false;
  } else {
    needs_redraw_ = true;
    active_tree_needs_first_draw_ =
        pending_tree_needs_first_draw_on_activation_;
    pending_tree_needs_first_draw_on_activation_ = false;
  }
  waiting_for_activation_after_rendering_resumed_ = false;

  previous_pending_tree_was_impl_side_ = current_pending_tree_is_impl_side_;
  current_pending_tree_is_impl_side_ = false;
}

void SchedulerStateMachine::WillDrawInternal() {
  // If a new active tree is pending after the one we are about to draw,
  // the main thread is in a high latency mode.
  // main_thread_missed_last_deadline_ is here in addition to
  // OnBeginImplFrameIdle for cases where the scheduler aborts draws outside
  // of the deadline.
  main_thread_missed_last_deadline_ =
      CommitPending() ||
      (has_pending_tree_ && !current_pending_tree_is_impl_side_);

  // We need to reset needs_redraw_ before we draw since the
  // draw itself might request another draw.
  needs_redraw_ = false;

  did_draw_ = true;
  active_tree_needs_first_draw_ = false;
  last_frame_number_draw_performed_ = current_frame_number_;

  if (forced_redraw_state_ == ForcedRedrawOnTimeoutState::WAITING_FOR_DRAW)
    forced_redraw_state_ = ForcedRedrawOnTimeoutState::IDLE;
}

void SchedulerStateMachine::DidDrawInternal(DrawResult draw_result) {
  switch (draw_result) {
    case DrawResult::kInvalidResult:
      NOTREACHED() << "Invalid return DrawResult:"
                   << static_cast<int>(DrawResult::kInvalidResult);
    case DrawResult::kAbortedCantDraw:
      if (consecutive_cant_draw_count_++ < 3u) {
        needs_redraw_ = true;
      } else {
        DUMP_WILL_BE_NOTREACHED()
            << consecutive_cant_draw_count_ << " consecutve draws"
            << " with DrawResult::kAbortedCantDraw result";
      }
      break;
    case DrawResult::kAbortedDrainingPipeline:
    case DrawResult::kSuccess:
      consecutive_checkerboard_animations_ = 0;
      consecutive_cant_draw_count_ = 0;
      forced_redraw_state_ = ForcedRedrawOnTimeoutState::IDLE;
      break;
    case DrawResult::kAbortedCheckerboardAnimations:
      DCHECK(!did_submit_in_last_frame_);
      needs_begin_main_frame_ = true;
      needs_redraw_ = true;
      consecutive_checkerboard_animations_++;
      consecutive_cant_draw_count_ = 0;

      if (consecutive_checkerboard_animations_ >=
              settings_.maximum_number_of_failed_draws_before_draw_is_forced &&
          forced_redraw_state_ == ForcedRedrawOnTimeoutState::IDLE) {
        // We need to force a draw, but it doesn't make sense to do this until
        // we've committed and have new textures.
        forced_redraw_state_ = ForcedRedrawOnTimeoutState::WAITING_FOR_COMMIT;
      }
      break;
    case DrawResult::kAbortedMissingHighResContent:
      DCHECK(!did_submit_in_last_frame_);
      // It's not clear whether this missing content is because of missing
      // pictures (which requires a commit) or because of memory pressure
      // removing textures (which might not).  To be safe, request a commit
      // anyway.
      needs_begin_main_frame_ = true;
      consecutive_cant_draw_count_ = 0;
      break;
  }
}

void SchedulerStateMachine::WillDraw() {
  DCHECK(!did_draw_);
  WillDrawInternal();
  // Set this to true to proactively request a new BeginFrame. We can't set this
  // in WillDrawInternal because AbortDraw calls WillDrawInternal but shouldn't
  // request another frame.
  did_attempt_draw_in_last_frame_ = true;
}

void SchedulerStateMachine::WillUpdateDisplayTree() {
  needs_update_display_tree_ = false;
  did_update_display_tree_ = true;
}

void SchedulerStateMachine::DidDraw(DrawResult draw_result) {
  draw_succeeded_in_last_frame_ = draw_result == DrawResult::kSuccess;
  DidDrawInternal(draw_result);
}

void SchedulerStateMachine::SetNeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {
  needs_impl_side_invalidation_ = true;
  next_invalidation_needs_first_draw_on_activation_ |=
      needs_first_draw_on_activation;
}

void SchedulerStateMachine::SetMainThreadWantsBeginMainFrameNotExpectedMessages(
    bool new_state) {
  wants_begin_main_frame_not_expected_ = new_state;
}

void SchedulerStateMachine::AbortDraw() {
  if (begin_frame_source_paused_) {
    draw_aborted_for_paused_begin_frame_ = true;
  }
  // Pretend like the draw was successful.
  // Note: We may abort at any time and cannot DCHECK that
  // we haven't drawn in or swapped in the last frame here.
  WillDrawInternal();
  DidDrawInternal(DrawResult::kAbortedDrainingPipeline);
}

void SchedulerStateMachine::WillPrepareTiles() {
  needs_prepare_tiles_ = false;
}

void SchedulerStateMachine::WillBeginLayerTreeFrameSinkCreation() {
  DCHECK_EQ(layer_tree_frame_sink_state_, LayerTreeFrameSinkState::NONE);
  layer_tree_frame_sink_state_ = LayerTreeFrameSinkState::CREATING;

  // The following DCHECKs make sure we are in the proper quiescent state.
  // The pipeline should be flushed entirely before we start output
  // surface creation to avoid complicated corner cases.
  DCHECK(begin_main_frame_state_ == BeginMainFrameState::IDLE);
  DCHECK(next_begin_main_frame_state_ == BeginMainFrameState::IDLE);
  DCHECK(!has_pending_tree_);
  DCHECK(!active_tree_needs_first_draw_);

  should_warm_up_ = false;
}

void SchedulerStateMachine::WillInvalidateLayerTreeFrameSink() {
  DCHECK(!did_invalidate_layer_tree_frame_sink_);
  did_invalidate_layer_tree_frame_sink_ = true;
  last_frame_number_invalidate_layer_tree_frame_sink_performed_ =
      current_frame_number_;
}

bool SchedulerStateMachine::BeginFrameNeededForVideo() const {
  return video_needs_begin_frames_;
}

bool SchedulerStateMachine::BeginFrameNeeded() const {
  // If we shouldn't subscribe to BeginFrames it implies BeginFrames are not
  // needed.
  if (!ShouldSubscribeToBeginFrames())
    return false;

  if (!pause_rendering_)
    return true;

  // Drain any in-flight main frame updates before pausing impl frames.
  if (begin_main_frame_state_ != BeginMainFrameState::IDLE)
    return true;

  // If a pending tree exists, activate and draw before pausing impl frames.
  if (has_pending_tree_)
    return true;

  // If a newly activated tree hasn't been drawn yet, draw it before pausing
  // impl frames.
  if (active_tree_needs_first_draw_)
    return true;

  return false;
}

bool SchedulerStateMachine::ShouldSubscribeToBeginFrames() const {
  // We can't handle BeginFrames when output surface isn't initialized.
  // TODO(brianderson): Support output surface creation inside a BeginFrame.
  if (!HasInitializedLayerTreeFrameSink())
    return false;

  // The propagation of the needsBeginFrame signal to viz is inherently racy
  // with issuing the next BeginFrame. In full-pipe mode, it is important we
  // don't miss a BeginFrame because our needsBeginFrames signal propagated to
  // viz too slowly. To avoid the race, we simply always request BeginFrames
  // from viz.
  if (settings_.wait_for_all_pipeline_stages_before_draw)
    return true;

  // If we are not visible, we don't need BeginFrame messages.
  if (!visible_)
    return false;

  return BeginFrameRequiredForAction() || BeginFrameNeededForVideo() ||
         ProactiveBeginFrameWanted();
}

void SchedulerStateMachine::SetVideoNeedsBeginFrames(
    bool video_needs_begin_frames) {
  video_needs_begin_frames_ = video_needs_begin_frames;
}

void SchedulerStateMachine::SetDeferBeginMainFrame(
    bool defer_begin_main_frame) {
  defer_begin_main_frame_ = defer_begin_main_frame;
}

void SchedulerStateMachine::SetPauseRendering(bool pause_rendering) {
  if (pause_rendering_ == pause_rendering) {
    return;
  }

  pause_rendering_ = pause_rendering;

  // If we're resuming rendering, we shouldn't already have a pending tree from
  // the main thread.
  // Note: This is possible if the main thread does the following:
  // 1. Pause rendering followed by a commit for the ongoing BeginMainFrame.
  // 2. Resume rendering before the above commit activates.
  // The current users of PauseRendering wait on the commit in #1 to be flushed
  // so it can never happen.
  DCHECK(pause_rendering_ || !has_pending_tree_);

  // When resuming rendering, main thread always commits at least one frame.
  // Dont draw any impl frames until this commit is activated.
  waiting_for_activation_after_rendering_resumed_ = !pause_rendering_;
}

// These are the cases where we require a BeginFrame message to make progress
// on requested actions.
bool SchedulerStateMachine::BeginFrameRequiredForAction() const {
  // The forced draw respects our normal draw scheduling, so we need to
  // request a BeginImplFrame for it.
  if (forced_redraw_state_ == ForcedRedrawOnTimeoutState::WAITING_FOR_DRAW)
    return true;

  return needs_redraw_ || needs_one_begin_impl_frame_ ||
         (needs_begin_main_frame_ && !defer_begin_main_frame_) ||
         needs_impl_side_invalidation_;
}

// These are cases where we are very likely want a BeginFrame message in the
// near future. Proactively requesting the BeginImplFrame helps hide the round
// trip latency of the SetNeedsBeginFrame request that has to go to the
// Browser.
// This includes things like drawing soon, but might not actually have a new
// frame to draw when we receive the next BeginImplFrame.
bool SchedulerStateMachine::ProactiveBeginFrameWanted() const {
  // Do not be proactive when invisible.
  if (!visible_)
    return false;

  // We should proactively request a BeginImplFrame if a commit is pending
  // because we will want to draw if the commit completes quickly. Do not
  // request frames when commits are disabled, because the frame requests will
  // not provide the needed commit (and will wake up the process when it could
  // stay idle).
  if ((begin_main_frame_state_ != BeginMainFrameState::IDLE) &&
      !defer_begin_main_frame_)
    return true;

  // If the pending tree activates quickly, we'll want a BeginImplFrame soon
  // to draw the new active tree.
  if (has_pending_tree_)
    return true;

  // Changing priorities may allow us to activate (given the new priorities),
  // which may result in a new frame.
  if (needs_prepare_tiles_)
    return true;

  // If we just tried to draw, it's likely that we are going to produce another
  // frame soon. This helps avoid negative glitches in our SetNeedsBeginFrame
  // requests, which may propagate to the BeginImplFrame provider and get
  // sampled at an inopportune time, delaying the next BeginImplFrame.
  if (did_attempt_draw_in_last_frame_)
    return true;

  // If the last commit was aborted because of early out (no updates), we should
  // still want a begin frame in case there is a commit coming again.
  if (last_commit_had_no_updates_)
    return true;

  // If there is active interaction happening (e.g. scroll/pinch), then keep
  // reqeusting frames.
  if (tree_priority_ == SMOOTHNESS_TAKES_PRIORITY)
    return true;

  return false;
}

void SchedulerStateMachine::OnBeginImplFrame(const viz::BeginFrameId& frame_id,
                                             bool animate_only) {
  begin_impl_frame_state_ = BeginImplFrameState::INSIDE_BEGIN_FRAME;
  current_frame_number_++;
  begin_frame_is_animate_only_ = animate_only;

  // Cache the values from the previous impl frame before reseting them for this
  // frame.
  last_frame_events_.commit_had_no_updates = last_commit_had_no_updates_;
  last_frame_events_.did_commit_during_frame = did_commit_during_frame_;

  last_commit_had_no_updates_ = false;
  did_attempt_draw_in_last_frame_ = false;
  draw_succeeded_in_last_frame_ = false;
  did_submit_in_last_frame_ = false;
  needs_one_begin_impl_frame_ = false;

  did_notify_begin_main_frame_not_expected_until_ = false;
  did_notify_begin_main_frame_not_expected_soon_ = false;
  did_send_begin_main_frame_for_current_frame_ = false;
  did_commit_during_frame_ = false;
  did_invalidate_layer_tree_frame_sink_ = false;
  did_perform_impl_side_invalidation_ = false;
  waiting_for_scroll_event_ = false;
}

void SchedulerStateMachine::OnBeginImplFrameDeadline() {
  begin_impl_frame_state_ = BeginImplFrameState::INSIDE_DEADLINE;

  // Clear funnels for any actions we perform during the deadline.
  did_draw_ = false;
}

void SchedulerStateMachine::OnBeginImplFrameIdle() {
  begin_impl_frame_state_ = BeginImplFrameState::IDLE;

  // Count any prepare tiles that happens in commits in between frames. We want
  // to prevent a prepare tiles during the next frame's deadline in that case.
  // This also allows synchronous compositor to do one PrepareTiles per draw.
  // This is same as the old prepare tiles funnel behavior.
  did_prepare_tiles_ = false;

  // If a new or undrawn active tree is pending after the deadline,
  // then the main thread is in a high latency mode.
  main_thread_missed_last_deadline_ =
      CommitPending() || has_pending_tree_ || active_tree_needs_first_draw_;

  // If we're entering a state where we won't get BeginFrames set all the
  // funnels so that we don't perform any actions that we shouldn't.
  if (!BeginFrameNeeded())
    did_send_begin_main_frame_for_current_frame_ = true;
}

SchedulerStateMachine::BeginImplFrameDeadlineMode
SchedulerStateMachine::CurrentBeginImplFrameDeadlineMode() const {
  const bool outside_begin_frame =
      begin_impl_frame_state_ != BeginImplFrameState::INSIDE_BEGIN_FRAME;
  if (settings_.using_synchronous_renderer_compositor || outside_begin_frame) {
    // No deadline for synchronous compositor, or when outside the begin frame.
    return BeginImplFrameDeadlineMode::NONE;
  } else if (ShouldBlockDeadlineIndefinitely()) {
    // We do not want to wait for a deadline because we're waiting for full
    // pipeline to be flushed for headless.
    return BeginImplFrameDeadlineMode::BLOCKED;
  } else if (ShouldTriggerBeginImplFrameDeadlineImmediately()) {
    if (ShouldWaitForScrollEvent()) {
      // We are scrolling but have not received a scroll event for this begin
      // frame. We want to wait before attempting to draw.
      return BeginImplFrameDeadlineMode::WAIT_FOR_SCROLL;
    }
    // We are ready to draw a new active tree immediately because there's no
    // commit expected or we're prioritizing active tree latency.
    return BeginImplFrameDeadlineMode::IMMEDIATE;
  } else if (needs_redraw_) {
    // We have an animation or fast input path on the impl thread that wants
    // to draw, so don't wait too long for a new active tree.
    return BeginImplFrameDeadlineMode::REGULAR;
  } else {
    // The impl thread doesn't have anything it wants to draw and we are just
    // waiting for a new active tree.
    return BeginImplFrameDeadlineMode::LATE;
  }
}

bool SchedulerStateMachine::ShouldWaitForScrollEvent() const {
  // We only apply this mode during frame production
  if (begin_impl_frame_state_ != BeginImplFrameState::INSIDE_BEGIN_FRAME) {
    return false;
  }
  // Once the deadline has been triggered, we should stop waiting.
  if (begin_impl_frame_state_ == BeginImplFrameState::INSIDE_DEADLINE) {
    return false;
  }
  // We are scrolling but have not received a scroll event for this begin frame.
  if (settings_.scroll_deadline_mode_enabled && is_scrolling_ &&
      waiting_for_scroll_event_) {
    return true;
  }
  return false;
}

bool SchedulerStateMachine::ShouldTriggerBeginImplFrameDeadlineImmediately()
    const {
  // If we aborted the current frame we should end the deadline right now.
  if (ShouldAbortCurrentFrame() && !has_pending_tree_)
    return true;

  // Throttle the deadline on CompositorFrameAck since we wont draw and submit
  // anyway.
  if (IsDrawThrottled())
    return false;

  // Delay immediate draws when we have pending animation worklet updates to
  // give them time to produce output before we draw.
  if (processing_animation_worklets_for_active_tree_)
    return false;

  // In full-pipe mode, we just gave all pipeline stages a chance to contribute.
  // We shouldn't wait any longer in any case - even if there are no updates.
  if (settings_.wait_for_all_pipeline_stages_before_draw)
    return true;

  if (active_tree_needs_first_draw_)
    return true;

  if (!needs_redraw_)
    return false;

  // This is used to prioritize impl-thread draws when the main thread isn't
  // producing anything, e.g., after an aborted commit. We also check that we
  // don't have a pending tree -- otherwise we should give it a chance to
  // activate.
  // TODO(skyostil): Revisit this when we have more accurate deadline estimates.
  if (!CommitPending() && !has_pending_tree_)
    return true;

  // Prioritize impl-thread draws in ImplLatencyTakesPriority mode.
  if (ImplLatencyTakesPriority())
    return true;

  return false;
}

bool SchedulerStateMachine::ShouldBlockDeadlineIndefinitely() const {
  if (!settings_.wait_for_all_pipeline_stages_before_draw &&
      !settings_.commit_to_active_tree) {
    return false;
  }

  // Avoid blocking for any reason if we don't have a layer tree frame sink or
  // are invisible.
  if (layer_tree_frame_sink_state_ == LayerTreeFrameSinkState::NONE)
    return false;

  if (!visible_)
    return false;

  // Do not wait for main frame to be ready for commits if in full-pipe mode,
  // if we're deferring commits, as the main thread may be blocked on paused
  // virtual time, causing deadlock against external frame control.
  if (defer_begin_main_frame_ &&
      settings_.wait_for_all_pipeline_stages_before_draw) {
    return false;
  }

  // Wait for main frame if one is in progress or about to be started.
  if (ShouldSendBeginMainFrame())
    return true;

  if (begin_main_frame_state_ != BeginMainFrameState::IDLE)
    return true;

  // Wait for tiles and activation.
  if (has_pending_tree_)
    return true;

  // Avoid blocking for draw when we can't draw. We block in the above cases
  // even if we cannot draw, because we may still be waiting for the first
  // active tree.
  if (!can_draw_)
    return false;

  // Wait for remaining tiles and draw.
  if (!active_tree_is_ready_to_draw_)
    return true;

  return false;
}

bool SchedulerStateMachine::IsDrawThrottled() const {
  return pending_submit_frames_ >= kMaxPendingSubmitFrames &&
         !settings_.disable_frame_rate_limit;
}

void SchedulerStateMachine::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;

  if (visible) {
    main_thread_missed_last_deadline_ = false;
    should_warm_up_ = false;
  }

  did_prepare_tiles_ = false;
}

void SchedulerStateMachine::SetShouldWarmUp() {
  CHECK(base::FeatureList::IsEnabled(features::kWarmUpCompositor));
  should_warm_up_ = true;
}

void SchedulerStateMachine::SetBeginFrameSourcePaused(bool paused) {
  begin_frame_source_paused_ = paused;
  if (!paused) {
    needs_redraw_ = draw_aborted_for_paused_begin_frame_;
    draw_aborted_for_paused_begin_frame_ = false;
  }
}

void SchedulerStateMachine::SetResourcelessSoftwareDraw(
    bool resourceless_draw) {
  resourceless_draw_ = resourceless_draw;
}

void SchedulerStateMachine::SetCanDraw(bool can_draw) {
  can_draw_ = can_draw;
}

void SchedulerStateMachine::SetSkipDraw(bool skip_draw) {
  skip_draw_ = skip_draw;
}

void SchedulerStateMachine::SetNeedsRedraw() {
  needs_redraw_ = true;
}

void SchedulerStateMachine::SetNeedsUpdateDisplayTree() {
  needs_update_display_tree_ = true;
  did_update_display_tree_ = false;
}

void SchedulerStateMachine::SetNeedsPrepareTiles() {
  if (!needs_prepare_tiles_) {
    TRACE_EVENT0("cc", "SchedulerStateMachine::SetNeedsPrepareTiles");
    needs_prepare_tiles_ = true;
  }
}
void SchedulerStateMachine::DidSubmitCompositorFrame() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("cc", "Scheduler:pending_submit_frames",
                                    TRACE_ID_LOCAL(this), "pending_frames",
                                    pending_submit_frames_);

  // If we are running with no frame rate limits, the GPU process can submit
  // a new BeginFrame request if the deadline for the pending BeginFrame
  // request expires. It will basically cause this DCHECK to fire as we may
  // not have received acks for previously submitted requests.
  // Please see SchedulerStateMachine::IsDrawThrottled() where throttling
  // is disabled when the disable_frame_rate_limit setting is enabled.
  // TODO(ananta/jonross/sunnyps)
  // http://crbug.com/346931323
  // We should remove or change this once VRR support is implemented for
  // Windows and other platforms potentially.
  if (!settings_.disable_frame_rate_limit) {
    DCHECK_LT(pending_submit_frames_, kMaxPendingSubmitFrames);
  }

  pending_submit_frames_++;
  submit_frames_with_current_layer_tree_frame_sink_++;

  did_submit_in_last_frame_ = true;
  last_frame_number_submit_performed_ = current_frame_number_;
}

void SchedulerStateMachine::DidReceiveCompositorFrameAck() {
  TRACE_EVENT_NESTABLE_ASYNC_END1("cc", "Scheduler:pending_submit_frames",
                                  TRACE_ID_LOCAL(this), "pending_frames",
                                  pending_submit_frames_);
  pending_submit_frames_--;
}

void SchedulerStateMachine::SetTreePrioritiesAndScrollState(
    TreePriority tree_priority,
    ScrollHandlerState scroll_handler_state) {
  tree_priority_ = tree_priority;
  scroll_handler_state_ = scroll_handler_state;
}

void SchedulerStateMachine::SetCriticalBeginMainFrameToActivateIsFast(
    bool is_fast) {
  critical_begin_main_frame_to_activate_is_fast_ = is_fast;
}

bool SchedulerStateMachine::ImplLatencyTakesPriority() const {
  // Attempt to synchronize with the main thread if it has a scroll listener
  // and is fast.
  if (ScrollHandlerState::SCROLL_AFFECTS_SCROLL_HANDLER ==
          scroll_handler_state_ &&
      critical_begin_main_frame_to_activate_is_fast_)
    return false;

  // Don't wait for the main thread if we are prioritizing smoothness.
  if (SMOOTHNESS_TAKES_PRIORITY == tree_priority_)
    return true;

  return false;
}

void SchedulerStateMachine::SetNeedsBeginMainFrame() {
  needs_begin_main_frame_ = true;
}

void SchedulerStateMachine::SetNeedsOneBeginImplFrame() {
  needs_one_begin_impl_frame_ = true;
}

void SchedulerStateMachine::NotifyReadyToCommit() {
  DCHECK_EQ(begin_main_frame_state_, BeginMainFrameState::SENT);
  begin_main_frame_state_ = BeginMainFrameState::READY_TO_COMMIT;
  // In commit_to_active_tree mode, commit should happen right after BeginFrame,
  // meaning when this function is called, next action should be commit.
  DCHECK(!settings_.commit_to_active_tree || ShouldCommit());
}

void SchedulerStateMachine::BeginMainFrameAborted(CommitEarlyOutReason reason) {
  if (begin_main_frame_state_ == BeginMainFrameState::SENT) {
    DCHECK_EQ(next_begin_main_frame_state_, BeginMainFrameState::IDLE);

    // If the main thread aborted, it doesn't matter if the  main thread missed
    // the last deadline since it didn't have an update anyway.
    main_thread_missed_last_deadline_ = false;

    switch (reason) {
      case CommitEarlyOutReason::kAbortedNotVisible:
      case CommitEarlyOutReason::kAbortedDeferredMainFrameUpdate:
      case CommitEarlyOutReason::kAbortedDeferredCommit:
        // TODO(rendering-core) For kAbortedDeferredCommit we may wish to do
        // something different because we have updated the main frame, but we
        // have not committed it. So we do not necessarily need a begin main
        // frame but we do need a commit for the frame we deferred. In practice
        // the next BeginMainFrame after the deferred commit timeout will cause
        // a commit, but it might come later than optimal.
        begin_main_frame_state_ = BeginMainFrameState::IDLE;
        SetNeedsBeginMainFrame();
        break;
      case CommitEarlyOutReason::kFinishedNoUpdates:
        WillCommit(/*commit_had_no_updates=*/true);
        break;
    }
  } else {
    DCHECK(settings_.main_frame_before_commit_enabled);
    DCHECK_EQ(next_begin_main_frame_state_, BeginMainFrameState::SENT);
    DCHECK_EQ(begin_main_frame_state_, BeginMainFrameState::READY_TO_COMMIT);
    next_begin_main_frame_state_ = BeginMainFrameState::IDLE;
    switch (reason) {
      case CommitEarlyOutReason::kAbortedNotVisible:
      case CommitEarlyOutReason::kAbortedDeferredMainFrameUpdate:
      case CommitEarlyOutReason::kAbortedDeferredCommit:
        SetNeedsBeginMainFrame();
        break;
      case CommitEarlyOutReason::kFinishedNoUpdates:
        commit_count_++;
        break;
    }
  }
}

void SchedulerStateMachine::DidPrepareTiles() {
  needs_prepare_tiles_ = false;
  did_prepare_tiles_ = true;
}

void SchedulerStateMachine::DidLoseLayerTreeFrameSink() {
  if (layer_tree_frame_sink_state_ == LayerTreeFrameSinkState::NONE ||
      layer_tree_frame_sink_state_ == LayerTreeFrameSinkState::CREATING)
    return;
  layer_tree_frame_sink_state_ = LayerTreeFrameSinkState::NONE;
  needs_redraw_ = false;
  needs_update_display_tree_ = false;
}

bool SchedulerStateMachine::NotifyReadyToActivate() {
  // It is not valid for clients to try and activate the pending tree whilst
  // paint worklets are still being processed; the pending tree *must* be fully
  // painted before it can ever be activated (even if e.g. it is not visible),
  // because we cannot paint the active tree.
  DCHECK(!processing_paint_worklets_for_pending_tree_);

  if (!has_pending_tree_ || pending_tree_is_ready_for_activation_)
    return false;

  pending_tree_is_ready_for_activation_ = true;
  return true;
}

bool SchedulerStateMachine::IsReadyToActivate() {
  return pending_tree_is_ready_for_activation_;
}

void SchedulerStateMachine::NotifyReadyToDraw() {
  active_tree_is_ready_to_draw_ = true;
}

void SchedulerStateMachine::NotifyAnimationWorkletStateChange(
    AnimationWorkletState state,
    TreeType tree) {
  if (tree == TreeType::ACTIVE) {
    switch (state) {
      case AnimationWorkletState::PROCESSING:
        DCHECK_GE(processing_animation_worklets_for_active_tree_, 0);
        DCHECK_LE(processing_animation_worklets_for_active_tree_, 1);
        processing_animation_worklets_for_active_tree_++;
        break;

      case AnimationWorkletState::IDLE:
        DCHECK_LE(processing_animation_worklets_for_active_tree_, 2);
        DCHECK_GE(processing_animation_worklets_for_active_tree_, 1);
        processing_animation_worklets_for_active_tree_--;
    }
  } else {
    processing_animation_worklets_for_pending_tree_ =
        (state == AnimationWorkletState::PROCESSING);
  }
}

void SchedulerStateMachine::NotifyPaintWorkletStateChange(
    PaintWorkletState state) {
  bool processing_paint_worklets_for_pending_tree =
      (state == PaintWorkletState::PROCESSING);
  DCHECK_NE(processing_paint_worklets_for_pending_tree,
            processing_paint_worklets_for_pending_tree_);
  processing_paint_worklets_for_pending_tree_ =
      processing_paint_worklets_for_pending_tree;
}

void SchedulerStateMachine::DidCreateAndInitializeLayerTreeFrameSink() {
  DCHECK_EQ(layer_tree_frame_sink_state_, LayerTreeFrameSinkState::CREATING);
  layer_tree_frame_sink_state_ =
      LayerTreeFrameSinkState::WAITING_FOR_FIRST_COMMIT;

  if (did_create_and_initialize_first_layer_tree_frame_sink_) {
    // TODO(boliu): See if we can remove this when impl-side painting is always
    // on. Does anything on the main thread need to update after recreate?
    needs_begin_main_frame_ = true;
  }
  did_create_and_initialize_first_layer_tree_frame_sink_ = true;
  pending_submit_frames_ = 0;
  submit_frames_with_current_layer_tree_frame_sink_ = 0;
  main_thread_missed_last_deadline_ = false;
}

bool SchedulerStateMachine::HasInitializedLayerTreeFrameSink() const {
  switch (layer_tree_frame_sink_state_) {
    case LayerTreeFrameSinkState::NONE:
    case LayerTreeFrameSinkState::CREATING:
      return false;

    case LayerTreeFrameSinkState::ACTIVE:
    case LayerTreeFrameSinkState::WAITING_FOR_FIRST_COMMIT:
    case LayerTreeFrameSinkState::WAITING_FOR_FIRST_ACTIVATION:
      return true;
  }
  NOTREACHED();
}

}  // namespace cc
