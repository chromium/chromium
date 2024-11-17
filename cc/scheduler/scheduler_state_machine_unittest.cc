// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/scheduler/scheduler_state_machine.h"

#include <stddef.h>

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/features.h"
#include "cc/scheduler/scheduler.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Macro to compare two enum values and get nice output.
// Without:
//   Value of: actual()      Actual: 7
//   Expected: expected()  Which is: 0
// With:
//   Value of: actual()      Actual: "ACTION_DRAW"
//   Expected: expected()  Which is: "ACTION_NONE"
#define EXPECT_ENUM_EQ(enum_tostring, expected, actual) \
  EXPECT_THAT(enum_tostring(actual), testing::Eq(enum_tostring(expected)))

#define EXPECT_IMPL_FRAME_STATE(expected)               \
  EXPECT_ENUM_EQ(BeginImplFrameStateToString, expected, \
                 state.begin_impl_frame_state())

#define EXPECT_MAIN_FRAME_STATE(expected)               \
  EXPECT_ENUM_EQ(BeginMainFrameStateToString, expected, \
                 state.GetBeginMainFrameState())

#define EXPECT_NEXT_MAIN_FRAME_STATE(expected)          \
  EXPECT_ENUM_EQ(BeginMainFrameStateToString, expected, \
                 state.GetNextBeginMainFrameState())

#define EXPECT_ACTION(expected) \
  EXPECT_ENUM_EQ(ActionToString, expected, state.NextAction())

#define EXPECT_ACTION_UPDATE_STATE(action)                            \
  EXPECT_ACTION(action);                                              \
  if (action == SchedulerStateMachine::Action::DRAW_IF_POSSIBLE ||    \
      action == SchedulerStateMachine::Action::DRAW_FORCED) {         \
    EXPECT_IMPL_FRAME_STATE(                                          \
        SchedulerStateMachine::BeginImplFrameState::INSIDE_DEADLINE); \
  }                                                                   \
  PerformAction(&state, action);                                      \
  if (action == SchedulerStateMachine::Action::NONE) {                \
    if (state.begin_impl_frame_state() ==                             \
        SchedulerStateMachine::BeginImplFrameState::INSIDE_DEADLINE)  \
      state.OnBeginImplFrameIdle();                                   \
  }

#define SET_UP_STATE(state)                                                 \
  state.SetVisible(true);                                                   \
  EXPECT_ACTION_UPDATE_STATE(                                               \
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION); \
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);          \
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();         \
  state.SetCanDraw(true)

namespace cc {

namespace {

const char* BeginImplFrameStateToString(
    SchedulerStateMachine::BeginImplFrameState state) {
  using BeginImplFrameState = SchedulerStateMachine::BeginImplFrameState;
  switch (state) {
    case BeginImplFrameState::IDLE:
      return "BeginImplFrameState::IDLE";
    case BeginImplFrameState::INSIDE_BEGIN_FRAME:
      return "BeginImplFrameState::INSIDE_BEGIN_FRAME";
    case BeginImplFrameState::INSIDE_DEADLINE:
      return "BeginImplFrameState::INSIDE_DEADLINE";
  }
  NOTREACHED();
}
const char* BeginMainFrameStateToString(
    SchedulerStateMachine::BeginMainFrameState state) {
  using BeginMainFrameState = SchedulerStateMachine::BeginMainFrameState;
  switch (state) {
    case BeginMainFrameState::IDLE:
      return "BeginMainFrameState::IDLE";
    case BeginMainFrameState::SENT:
      return "BeginMainFrameState::SENT";
    case BeginMainFrameState::READY_TO_COMMIT:
      return "BeginMainFrameState::READY_TO_COMMIT";
  }
  NOTREACHED();
}

const char* ActionToString(SchedulerStateMachine::Action action) {
  using Action = SchedulerStateMachine::Action;
  switch (action) {
    case Action::NONE:
      return "Action::NONE";
    case Action::SEND_BEGIN_MAIN_FRAME:
      return "Action::SEND_BEGIN_MAIN_FRAME";
    case Action::COMMIT:
      return "Action::COMMIT";
    case Action::POST_COMMIT:
      return "Action::POST_COMMIT";
    case Action::ACTIVATE_SYNC_TREE:
      return "Action::ACTIVATE_SYNC_TREE";
    case Action::DRAW_IF_POSSIBLE:
      return "Action::DRAW_IF_POSSIBLE";
    case Action::DRAW_FORCED:
      return "Action::DRAW_FORCED";
    case Action::DRAW_ABORT:
      return "Action::DRAW_ABORT";
    case Action::UPDATE_DISPLAY_TREE:
      return "Action::UPDATE_DISPLAY_TREE";
    case Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION:
      return "Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION";
    case Action::PREPARE_TILES:
      return "Action::PREPARE_TILES";
    case Action::INVALIDATE_LAYER_TREE_FRAME_SINK:
      return "Action::INVALIDATE_LAYER_TREE_FRAME_SINK";
    case Action::PERFORM_IMPL_SIDE_INVALIDATION:
      return "Action::PERFORM_IMPL_SIDE_INVALIDATION";
    case Action::NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL:
      return "Action::NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL";
    case Action::NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_SOON:
      return "Action::NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_SOON";
  }
  NOTREACHED();
}

const bool kAnimateOnly = false;

const SchedulerStateMachine::BeginImplFrameState all_begin_impl_frame_states[] =
    {
        SchedulerStateMachine::BeginImplFrameState::IDLE,
        SchedulerStateMachine::BeginImplFrameState::INSIDE_BEGIN_FRAME,
        SchedulerStateMachine::BeginImplFrameState::INSIDE_DEADLINE,
};

const SchedulerStateMachine::BeginMainFrameState begin_main_frame_states[] = {
    SchedulerStateMachine::BeginMainFrameState::IDLE,
    SchedulerStateMachine::BeginMainFrameState::SENT,
    SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT};

// Exposes the protected state fields of the SchedulerStateMachine for testing
class StateMachine : public SchedulerStateMachine {
 public:
  explicit StateMachine(const SchedulerSettings& scheduler_settings)
      : SchedulerStateMachine(scheduler_settings),
        draw_result_for_test_(DrawResult::kSuccess) {}

  void CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit() {
    DidCreateAndInitializeLayerTreeFrameSink();
    layer_tree_frame_sink_state_ = LayerTreeFrameSinkState::ACTIVE;
  }

  void IssueNextBeginImplFrame() {
    OnBeginImplFrame(viz::BeginFrameId(0, next_begin_frame_number_++),
                     kAnimateOnly);
  }

  void SetBeginMainFrameState(BeginMainFrameState cs) {
    begin_main_frame_state_ = cs;
  }
  BeginMainFrameState GetBeginMainFrameState() const {
    return begin_main_frame_state_;
  }
  BeginMainFrameState GetNextBeginMainFrameState() const {
    return next_begin_main_frame_state_;
  }

  ForcedRedrawOnTimeoutState ForcedRedrawState() const {
    return forced_redraw_state_;
  }

  void SetBeginImplFrameState(BeginImplFrameState bifs) {
    begin_impl_frame_state_ = bifs;
  }

  BeginImplFrameState begin_impl_frame_state() const {
    return begin_impl_frame_state_;
  }

  LayerTreeFrameSinkState layer_tree_frame_sink_state() const {
    return layer_tree_frame_sink_state_;
  }

  void SetNeedsBeginMainFrameForTest(bool needs_begin_main_frame) {
    needs_begin_main_frame_ = needs_begin_main_frame;
  }

  bool NeedsCommit() const { return needs_begin_main_frame_; }

  void SetNeedsOneBeginImplFrame(bool needs_frame) {
    needs_one_begin_impl_frame_ = needs_frame;
  }

  void SetNeedsRedraw(bool needs_redraw) { needs_redraw_ = needs_redraw; }

  void SetDrawResultForTest(DrawResult draw_result) {
    draw_result_for_test_ = draw_result;
  }
  DrawResult draw_result_for_test() { return draw_result_for_test_; }

  void SetNeedsForcedRedrawForTimeout(bool b) {
    forced_redraw_state_ = ForcedRedrawOnTimeoutState::WAITING_FOR_COMMIT;
    active_tree_needs_first_draw_ = true;
  }
  bool NeedsForcedRedrawForTimeout() const {
    return forced_redraw_state_ != ForcedRedrawOnTimeoutState::IDLE;
  }

  void SetActiveTreeNeedsFirstDraw(bool needs_first_draw) {
    active_tree_needs_first_draw_ = needs_first_draw;
  }

  bool CanDraw() const { return can_draw_; }
  bool Visible() const { return visible_; }

  bool ShouldAbortCurrentFrame() const {
    return SchedulerStateMachine::ShouldAbortCurrentFrame();
  }

  bool has_pending_tree() const { return has_pending_tree_; }
  void SetHasPendingTree(bool has_pending_tree) {
    has_pending_tree_ = has_pending_tree;
  }

  bool needs_impl_side_invalidation() const {
    return needs_impl_side_invalidation_;
  }

  using SchedulerStateMachine::ProactiveBeginFrameWanted;
  using SchedulerStateMachine::ShouldDraw;
  using SchedulerStateMachine::ShouldPrepareTiles;
  using SchedulerStateMachine::ShouldSendBeginMainFrame;
  using SchedulerStateMachine::ShouldTriggerBeginImplFrameDeadlineImmediately;
  using SchedulerStateMachine::ShouldWaitForScrollEvent;
  using SchedulerStateMachine::WillCommit;

 protected:
  DrawResult draw_result_for_test_;
  uint64_t next_begin_frame_number_ = viz::BeginFrameArgs::kStartingFrameNumber;
};

void PerformAction(StateMachine* sm, SchedulerStateMachine::Action action) {
  switch (action) {
    case SchedulerStateMachine::Action::NONE:
      return;

    case SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE:
      sm->WillActivate();
      return;

    case SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME:
      sm->WillSendBeginMainFrame();
      return;

    case SchedulerStateMachine::Action::
        NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL:
      sm->WillNotifyBeginMainFrameNotExpectedUntil();
      return;

    case SchedulerStateMachine::Action::
        NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_SOON:
      sm->WillNotifyBeginMainFrameNotExpectedSoon();
      return;

    case SchedulerStateMachine::Action::COMMIT: {
      bool commit_has_no_updates = false;
      sm->WillCommit(commit_has_no_updates);
      sm->DidCommit();
      return;
    }

    case SchedulerStateMachine::Action::POST_COMMIT:
      sm->DidPostCommit();
      return;

    case SchedulerStateMachine::Action::DRAW_FORCED:
    case SchedulerStateMachine::Action::DRAW_IF_POSSIBLE: {
      sm->WillDraw();
      sm->DidDraw(sm->draw_result_for_test());
      return;
    }

    case SchedulerStateMachine::Action::DRAW_ABORT: {
      sm->AbortDraw();
      return;
    }

    case SchedulerStateMachine::Action::UPDATE_DISPLAY_TREE:
      sm->WillUpdateDisplayTree();
      return;

    case SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION:
      sm->WillBeginLayerTreeFrameSinkCreation();
      return;

    case SchedulerStateMachine::Action::PREPARE_TILES:
      sm->WillPrepareTiles();
      return;

    case SchedulerStateMachine::Action::INVALIDATE_LAYER_TREE_FRAME_SINK:
      sm->WillInvalidateLayerTreeFrameSink();
      return;

    case SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION:
      sm->WillPerformImplSideInvalidation();
      return;
  }
}

TEST(SchedulerStateMachineTest, BeginFrameNeeded) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  state.SetBeginMainFrameState(
      SchedulerStateMachine::BeginMainFrameState::IDLE);

  // Don't request BeginFrames if we are idle.
  state.SetNeedsRedraw(false);
  state.SetNeedsOneBeginImplFrame(false);
  EXPECT_FALSE(state.BeginFrameNeeded());

  // Request BeginFrames if we one is needed.
  state.SetNeedsRedraw(false);
  state.SetNeedsOneBeginImplFrame(true);
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Request BeginFrames if we are ready to draw.
  state.SetVisible(true);
  state.SetNeedsRedraw(true);
  state.SetNeedsOneBeginImplFrame(false);
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Don't background tick for needs_redraw.
  state.SetVisible(false);
  state.SetNeedsRedraw(true);
  state.SetNeedsOneBeginImplFrame(false);
  EXPECT_FALSE(state.BeginFrameNeeded());

  // Proactively request BeginFrames when commit is pending.
  state.SetVisible(true);
  state.SetNeedsRedraw(false);
  state.SetNeedsOneBeginImplFrame(false);
  state.SetNeedsBeginMainFrameForTest(true);
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Don't request BeginFrames when commit is pending if
  // we are currently deferring commits.
  state.SetVisible(true);
  state.SetNeedsRedraw(false);
  state.SetNeedsOneBeginImplFrame(false);
  state.SetNeedsBeginMainFrameForTest(true);
  state.SetDeferBeginMainFrame(true);
  EXPECT_FALSE(state.BeginFrameNeeded());
}

TEST(SchedulerStateMachineTest, BeginMainFrameIsHighestPriorityAction) {
  SchedulerSettings default_scheduler_settings;
  default_scheduler_settings.main_frame_before_activation_enabled = true;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);

  // Still need to active, but sending BMF takes priority.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);

  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);

  // Still need to draw, but sending BMF takes priority.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION(SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest,
     TestNextActionNotifyBeginMainFrameNotExpectedUntil) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetMainThreadWantsBeginMainFrameNotExpectedMessages(true);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  state.IssueNextBeginImplFrame();
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  state.SetNeedsOneBeginImplFrame(true);
  EXPECT_TRUE(state.BeginFrameNeeded());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::
                                 NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest,
     TestNextActionNotifyBeginMainFrameNotExpectedSoon) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetMainThreadWantsBeginMainFrameNotExpectedMessages(true);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  state.IssueNextBeginImplFrame();
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  state.SetNeedsOneBeginImplFrame(true);
  EXPECT_TRUE(state.BeginFrameNeeded());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::
                                 NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.SetNeedsOneBeginImplFrame(false);
  EXPECT_FALSE(state.BeginFrameNeeded());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.SetBeginImplFrameState(
      SchedulerStateMachine::BeginImplFrameState::IDLE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_SOON);

  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, TestNextActionBeginsMainFrameIfNeeded) {
  SchedulerSettings default_scheduler_settings;

  // If no commit needed, do nothing.
  {
    StateMachine state(default_scheduler_settings);
    state.SetVisible(true);
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
    state.SetBeginMainFrameState(
        SchedulerStateMachine::BeginMainFrameState::IDLE);
    state.SetNeedsRedraw(false);

    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    EXPECT_FALSE(state.NeedsCommit());

    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

    state.OnBeginImplFrameDeadline();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    EXPECT_FALSE(state.NeedsCommit());
  }

  // If commit requested but not visible yet, do nothing.
  {
    StateMachine state(default_scheduler_settings);
    state.SetBeginMainFrameState(
        SchedulerStateMachine::BeginMainFrameState::IDLE);
    state.SetNeedsRedraw(false);
    state.SetNeedsBeginMainFrame();

    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    EXPECT_TRUE(state.NeedsCommit());

    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

    state.OnBeginImplFrameDeadline();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    EXPECT_TRUE(state.NeedsCommit());
  }

  // If commit requested, begin a main frame.
  {
    StateMachine state(default_scheduler_settings);
    state.SetBeginMainFrameState(
        SchedulerStateMachine::BeginMainFrameState::IDLE);
    state.SetVisible(true);
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
    state.SetNeedsRedraw(false);
    state.SetNeedsBeginMainFrame();

    // Expect nothing to happen until after OnBeginImplFrame.
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);
    EXPECT_IMPL_FRAME_STATE(SchedulerStateMachine::BeginImplFrameState::IDLE);
    EXPECT_TRUE(state.NeedsCommit());
    EXPECT_TRUE(state.BeginFrameNeeded());

    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
    EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
    EXPECT_FALSE(state.NeedsCommit());
  }

  // If commit requested and can't draw, still begin a main frame.
  {
    StateMachine state(default_scheduler_settings);
    state.SetBeginMainFrameState(
        SchedulerStateMachine::BeginMainFrameState::IDLE);
    state.SetVisible(true);
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
    state.SetNeedsRedraw(false);
    state.SetNeedsBeginMainFrame();
    state.SetCanDraw(false);

    // Expect nothing to happen until after OnBeginImplFrame.
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);
    EXPECT_IMPL_FRAME_STATE(SchedulerStateMachine::BeginImplFrameState::IDLE);
    EXPECT_TRUE(state.BeginFrameNeeded());

    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
    EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
    EXPECT_FALSE(state.NeedsCommit());
  }
}

// Explicitly test main_frame_before_activation_enabled = true
TEST(SchedulerStateMachineTest, MainFrameBeforeActivationEnabled) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.main_frame_before_activation_enabled = true;
  StateMachine state(scheduler_settings);
  state.SetBeginMainFrameState(
      SchedulerStateMachine::BeginMainFrameState::IDLE);
  SET_UP_STATE(state);
  state.SetNeedsRedraw(false);
  state.SetNeedsBeginMainFrame();

  EXPECT_TRUE(state.BeginFrameNeeded());

  // Commit to the pending tree.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Verify that the next commit starts while there is still a pending tree.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Verify the pending commit doesn't overwrite the pending
  // tree until the pending tree has been activated.
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Verify NotifyReadyToActivate unblocks activation, commit, and
  // draw in that order.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);
}

TEST(SchedulerStateMachineTest,
     FailedDrawForAnimationCheckerboardSetsNeedsCommitAndRetriesDraw) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Start a frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_FALSE(state.CommitPending());

  // Failing a draw triggers request for a new BeginMainFrame.
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DrawResult::kAbortedCheckerboardAnimations);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // It's okay to attempt more draws just in case additional raster
  // finishes and the requested commit wasn't actually necessary.
  EXPECT_TRUE(state.CommitPending());
  EXPECT_TRUE(state.RedrawPending());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DrawResult::kAbortedCheckerboardAnimations);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, FailedDrawForMissingHighResNeedsCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Start a frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_FALSE(state.CommitPending());

  // Failing a draw triggers because of high res tiles missing
  // request for a new BeginMainFrame.
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DrawResult::kAbortedMissingHighResContent);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // It doesn't request a draw until we get a new commit though.
  EXPECT_TRUE(state.CommitPending());
  EXPECT_FALSE(state.RedrawPending());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Finish the commit and activation.
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.RedrawPending());

  // Verify we draw with the new frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DrawResult::kSuccess);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest,
     TestFailedDrawsEventuallyForceDrawAfterNextCommit) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.maximum_number_of_failed_draws_before_draw_is_forced = 1;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state);

  // Start a commit.
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.CommitPending());

  // Then initiate a draw that fails.
  state.SetNeedsRedraw(true);
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DrawResult::kAbortedCheckerboardAnimations);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.BeginFrameNeeded());
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.CommitPending());

  // Finish the commit. Note, we should not yet be forcing a draw, but should
  // continue the commit as usual.
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.RedrawPending());

  // Activate so we're ready for a new main frame.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.RedrawPending());

  // The redraw should be forced at the end of the next BeginImplFrame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DrawResult::kSuccess);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_FORCED);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, TestFailedDrawsDoNotRestartForcedDraw) {
  SchedulerSettings scheduler_settings;
  int draw_limit = 2;
  scheduler_settings.maximum_number_of_failed_draws_before_draw_is_forced =
      draw_limit;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state);

  // Start a commit.
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.CommitPending());

  // Then initiate a draw.
  state.SetNeedsRedraw(true);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);

  // Fail the draw enough times to force a redraw.
  for (int i = 0; i < draw_limit; ++i) {
    state.SetNeedsRedraw(true);
    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    state.OnBeginImplFrameDeadline();
    state.SetDrawResultForTest(DrawResult::kAbortedCheckerboardAnimations);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    state.OnBeginImplFrameIdle();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  }

  EXPECT_TRUE(state.BeginFrameNeeded());
  EXPECT_TRUE(state.RedrawPending());
  // But the commit is ongoing.
  EXPECT_TRUE(state.CommitPending());
  EXPECT_TRUE(
      state.ForcedRedrawState() ==
      SchedulerStateMachine::ForcedRedrawOnTimeoutState::WAITING_FOR_COMMIT);

  // After failing additional draws, we should still be in a forced
  // redraw, but not back in IDLE.
  for (int i = 0; i < draw_limit; ++i) {
    state.SetNeedsRedraw(true);
    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    state.OnBeginImplFrameDeadline();
    state.SetDrawResultForTest(DrawResult::kAbortedCheckerboardAnimations);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    state.OnBeginImplFrameIdle();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  }
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(
      state.ForcedRedrawState() ==
      SchedulerStateMachine::ForcedRedrawOnTimeoutState::WAITING_FOR_COMMIT);
}

TEST(SchedulerStateMachineTest, TestFailedDrawIsRetriedInNextBeginImplFrame) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  // Start a draw.
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_TRUE(state.RedrawPending());
  state.SetDrawResultForTest(DrawResult::kAbortedCheckerboardAnimations);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);

  // Failing the draw for animation checkerboards makes us require a commit.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.RedrawPending());

  // We should not be trying to draw again now, but we have a commit pending.
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // We should try to draw again at the end of the next BeginImplFrame on
  // the impl thread.
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DrawResult::kSuccess);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, TestDoestDrawTwiceInSameFrame) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);
  state.SetNeedsRedraw(true);

  // Draw the first frame.
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Before the next BeginImplFrame, set needs redraw again.
  // This should not redraw until the next BeginImplFrame.
  state.SetNeedsRedraw(true);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Move to another frame. This should now draw.
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // We just submitted, so we should proactively request another BeginImplFrame.
  EXPECT_TRUE(state.BeginFrameNeeded());
}

TEST(SchedulerStateMachineTest, TestNextActionDrawsOnBeginImplFrame) {
  SchedulerSettings default_scheduler_settings;

  // When not in BeginImplFrame deadline, or in BeginImplFrame deadline
  // but not visible, don't draw.
  size_t num_begin_main_frame_states =
      sizeof(begin_main_frame_states) /
      sizeof(SchedulerStateMachine::BeginMainFrameState);
  size_t num_begin_impl_frame_states =
      sizeof(all_begin_impl_frame_states) /
      sizeof(SchedulerStateMachine::BeginImplFrameState);
  for (size_t i = 0; i < num_begin_main_frame_states; ++i) {
    for (size_t j = 0; j < num_begin_impl_frame_states; ++j) {
      StateMachine state(default_scheduler_settings);
      state.SetVisible(true);
      EXPECT_ACTION_UPDATE_STATE(
          SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
      EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
      state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
      state.SetBeginMainFrameState(begin_main_frame_states[i]);
      state.SetBeginImplFrameState(all_begin_impl_frame_states[j]);
      bool visible =
          (all_begin_impl_frame_states[j] !=
           SchedulerStateMachine::BeginImplFrameState::INSIDE_DEADLINE);
      state.SetVisible(visible);

      // Case 1: needs_begin_main_frame=false
      EXPECT_NE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE,
                state.NextAction());

      // Case 2: needs_begin_main_frame=true
      state.SetNeedsBeginMainFrame();
      EXPECT_NE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE,
                state.NextAction());
    }
  }

  // When in BeginImplFrame deadline we should always draw for SetNeedsRedraw
  // except if we're ready to commit, in which case we expect a commit first.
  for (size_t i = 0; i < num_begin_main_frame_states; ++i) {
    StateMachine state(default_scheduler_settings);
    state.SetVisible(true);
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
    state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
    state.SetCanDraw(true);
    state.SetBeginMainFrameState(begin_main_frame_states[i]);
    state.SetBeginImplFrameState(
        SchedulerStateMachine::BeginImplFrameState::INSIDE_DEADLINE);

    state.SetNeedsRedraw(true);

    SchedulerStateMachine::Action expected_action;
    if (begin_main_frame_states[i] ==
        SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT) {
      expected_action = SchedulerStateMachine::Action::COMMIT;
    } else {
      expected_action = SchedulerStateMachine::Action::DRAW_IF_POSSIBLE;
    }

    // Case 1: needs_begin_main_frame=false.
    EXPECT_ACTION(expected_action);

    // Case 2: needs_begin_main_frame=true.
    state.SetNeedsBeginMainFrame();
    EXPECT_ACTION(expected_action);
  }
}

TEST(SchedulerStateMachineTest, TestNoBeginMainFrameStatesRedrawWhenInvisible) {
  SchedulerSettings default_scheduler_settings;

  size_t num_begin_main_frame_states =
      sizeof(begin_main_frame_states) /
      sizeof(SchedulerStateMachine::BeginMainFrameState);
  for (size_t i = 0; i < num_begin_main_frame_states; ++i) {
    // There shouldn't be any drawing regardless of BeginImplFrame.
    for (size_t j = 0; j < 2; ++j) {
      StateMachine state(default_scheduler_settings);
      state.SetVisible(true);
      EXPECT_ACTION_UPDATE_STATE(
          SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
      EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
      state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
      state.SetBeginMainFrameState(begin_main_frame_states[i]);
      state.SetVisible(false);
      state.SetNeedsRedraw(true);
      if (j == 1) {
        state.SetBeginImplFrameState(
            SchedulerStateMachine::BeginImplFrameState::INSIDE_DEADLINE);
      }

      // Case 1: needs_begin_main_frame=false.
      EXPECT_NE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE,
                state.NextAction());

      // Case 2: needs_begin_main_frame=true.
      state.SetNeedsBeginMainFrame();
      EXPECT_NE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE,
                state.NextAction());
    }
  }
}

TEST(SchedulerStateMachineTest, TestCanRedraw_StopsDraw) {
  SchedulerSettings default_scheduler_settings;

  size_t num_begin_main_frame_states =
      sizeof(begin_main_frame_states) /
      sizeof(SchedulerStateMachine::BeginMainFrameState);
  for (size_t i = 0; i < num_begin_main_frame_states; ++i) {
    // There shouldn't be any drawing regardless of BeginImplFrame.
    for (size_t j = 0; j < 2; ++j) {
      StateMachine state(default_scheduler_settings);
      state.SetVisible(true);
      EXPECT_ACTION_UPDATE_STATE(
          SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
      EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
      state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
      state.SetBeginMainFrameState(begin_main_frame_states[i]);
      state.SetVisible(false);
      state.SetNeedsRedraw(true);
      if (j == 1)
        state.IssueNextBeginImplFrame();

      state.SetCanDraw(false);
      EXPECT_NE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE,
                state.NextAction());
    }
  }
}

TEST(SchedulerStateMachineTest,
     TestCanRedrawWithWaitingForFirstDrawMakesProgress) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();

  state.SetActiveTreeNeedsFirstDraw(true);
  state.SetNeedsBeginMainFrame();
  state.SetNeedsRedraw(true);
  state.SetCanDraw(false);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_ABORT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_ABORT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, TestSetNeedsBeginMainFrameIsNotLost) {
  SchedulerSettings scheduler_settings;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state);
  state.SetNeedsBeginMainFrame();

  EXPECT_TRUE(state.BeginFrameNeeded());

  // Begin the frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);

  // Now, while the frame is in progress, set another commit.
  state.SetNeedsBeginMainFrame();
  EXPECT_TRUE(state.NeedsCommit());

  // Let the frame finish.
  state.NotifyReadyToCommit();
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT);

  // Expect to commit regardless of BeginImplFrame state.
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BeginImplFrameState::INSIDE_BEGIN_FRAME);
  EXPECT_ACTION(SchedulerStateMachine::Action::COMMIT);

  state.OnBeginImplFrameDeadline();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BeginImplFrameState::INSIDE_DEADLINE);
  EXPECT_ACTION(SchedulerStateMachine::Action::COMMIT);

  state.OnBeginImplFrameIdle();
  EXPECT_IMPL_FRAME_STATE(SchedulerStateMachine::BeginImplFrameState::IDLE);
  EXPECT_ACTION(SchedulerStateMachine::Action::COMMIT);

  state.IssueNextBeginImplFrame();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BeginImplFrameState::INSIDE_BEGIN_FRAME);
  EXPECT_ACTION(SchedulerStateMachine::Action::COMMIT);

  // Finish the commit and activate, then make sure we start the next commit
  // immediately and draw on the next BeginImplFrame.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.OnBeginImplFrameDeadline();

  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, TestFullCycle) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Begin the frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Tell the scheduler the frame finished.
  state.NotifyReadyToCommit();
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT);

  // Commit.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);

  // Activate.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());

  // Expect to do nothing until BeginImplFrame deadline
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // At BeginImplFrame deadline, draw.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();

  // Should be synchronized, no draw needed, no action needed.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);
  EXPECT_FALSE(state.needs_redraw());
}

TEST(SchedulerStateMachineTest, CommitWithoutDrawWithPendingTree) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Make a main frame, commit and activate it. But don't draw it.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);

  // Try to make a new main frame before drawing. Since we will commit it to a
  // pending tree and not clobber the active tree, we're able to start a new
  // begin frame and commit it.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
}

TEST(SchedulerStateMachineTest, DontCommitWithoutDrawWithoutPendingTree) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.commit_to_active_tree = true;
  scheduler_settings.main_frame_before_activation_enabled = false;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state);

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();
  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 10);
  // Make a main frame, commit and activate it. But don't draw it.
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);

  // Try to make a new main frame before drawing, but since we would clobber the
  // active tree, we will not do so.
  state.SetNeedsBeginMainFrame();
  frame_id.sequence_number++;
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, AbortedMainFrameDoesNotResetPendingTree) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.main_frame_before_activation_enabled = true;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state);

  // Perform a commit so that we have an active tree.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.has_pending_tree());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Ask for another commit but abort it. Verify that we didn't reset pending
  // tree state.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.has_pending_tree());
  state.BeginMainFrameAborted(CommitEarlyOutReason::kFinishedNoUpdates);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.has_pending_tree());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Ask for another commit that doesn't abort.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.has_pending_tree());

  // Verify that commit is delayed until the pending tree is activated.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_FALSE(state.has_pending_tree());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.has_pending_tree());
}

TEST(SchedulerStateMachineTest, TestFullCycleWithCommitToActive) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.commit_to_active_tree = true;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state);

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Begin the frame.
  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 10);
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Tell the scheduler the frame finished.
  state.NotifyReadyToCommit();
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT);
  // Commit.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  // Commit always calls NotifyReadyToActivate in this mode.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // No draw because we haven't received NotifyReadyToDraw yet.
  state.OnBeginImplFrameDeadline();
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Can't BeginMainFrame yet since last commit hasn't been drawn yet.
  state.SetNeedsBeginMainFrame();
  frame_id.sequence_number++;
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Now call ready to draw which will allow the draw to happen and
  // BeginMainFrame to be sent.
  state.NotifyReadyToDraw();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  // Submit throttled from this point.
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Can't BeginMainFrame yet since we're submit-frame throttled.
  frame_id.sequence_number++;
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // CompositorFrameAck unblocks BeginMainFrame.
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Draw the newly activated tree.
  state.NotifyReadyToDraw();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // When commits are deferred, we don't block the deadline.
  state.SetDeferBeginMainFrame(true);
  frame_id.sequence_number++;
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_NE(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());
}

TEST(SchedulerStateMachineTest, TestFullCycleWithCommitRequestInbetween) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Begin the frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Request another commit while the commit is in flight.
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Tell the scheduler the frame finished.
  state.NotifyReadyToCommit();
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT);

  // First commit and activate.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());

  // Expect to do nothing until BeginImplFrame deadline.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // At BeginImplFrame deadline, draw.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();

  // Should be synchronized, no draw needed, no action needed.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);
  EXPECT_FALSE(state.needs_redraw());

  // Next BeginImplFrame should initiate second commit.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, TestNoRequestCommitWhenInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  state.SetVisible(false);
  state.SetNeedsBeginMainFrame();
  EXPECT_FALSE(state.CouldSendBeginMainFrame());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, TestNoRequestCommitWhenBeginFrameSourcePaused) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  state.SetBeginFrameSourcePaused(true);
  state.SetNeedsBeginMainFrame();
  EXPECT_FALSE(state.CouldSendBeginMainFrame());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, TestNoRequestLayerTreeFrameSinkWhenInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  // We should not request a LayerTreeFrameSink when we are still invisible.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  state.SetVisible(false);
  state.DidLoseLayerTreeFrameSink();
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
}

// See ProxyMain::BeginMainFrame "EarlyOut_NotVisible" /
// "EarlyOut_LayerTreeFrameSinkLost" cases.
TEST(SchedulerStateMachineTest, TestAbortBeginMainFrameBecauseInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Begin the frame while visible.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Become invisible and abort BeginMainFrame.
  state.SetVisible(false);
  state.BeginMainFrameAborted(CommitEarlyOutReason::kAbortedNotVisible);

  // NeedsCommit should now be true again because we never actually did a
  // commit.
  EXPECT_TRUE(state.NeedsCommit());

  // We should now be back in the idle state as if we never started the frame.
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // We shouldn't do anything on the BeginImplFrame deadline.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Become visible again.
  state.SetVisible(true);

  // Although we have aborted on this frame and haven't cancelled the commit
  // (i.e. need another), don't send another BeginMainFrame yet.
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.NeedsCommit());

  // Start a new frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);

  // We should be starting the commit now.
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

// See ProxyMain::BeginMainFrame "EarlyOut_NoUpdates" case.
TEST(SchedulerStateMachineTest, TestAbortBeginMainFrameBecauseCommitNotNeeded) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.DidCreateAndInitializeLayerTreeFrameSink();
  state.SetCanDraw(true);

  // Get into a begin frame / commit state.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);

  // Abort the commit, true means that the BeginMainFrame was sent but there
  // was no work to do on the main thread.
  state.BeginMainFrameAborted(CommitEarlyOutReason::kFinishedNoUpdates);

  // NeedsCommit should now be false because the commit was actually handled.
  EXPECT_FALSE(state.NeedsCommit());

  // Since the commit was aborted, we don't need to try and draw.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Verify another commit doesn't start on another frame either.
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Verify another commit can start if requested, though.
  state.SetNeedsBeginMainFrame();
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION(SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, TestMainFrameBeforeCommit) {
  SchedulerSettings default_scheduler_settings;
  default_scheduler_settings.main_frame_before_commit_enabled = true;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.DidCreateAndInitializeLayerTreeFrameSink();
  state.SetCanDraw(true);

  // Get into a begin frame / commit state.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);
  EXPECT_NEXT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::IDLE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
  EXPECT_NEXT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::IDLE);
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);

  // The secondary BeginMainFrame can't be sent until the first one completes
  state.IssueNextBeginImplFrame();
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);

  // Abort the primary BeginMainFrame; the next BeginMainFrame should also be
  // primary.
  state.BeginMainFrameAborted(CommitEarlyOutReason::kFinishedNoUpdates);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
  EXPECT_NEXT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::IDLE);

  // Complete the primary BeginMainFrame; the next BeginMainFrame should be
  // secondary, and it should be sent *before* commit of the primary.
  state.NotifyReadyToCommit();
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT);
  EXPECT_NEXT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::IDLE);

  // Secondary BeginMainFrame can be sent while primary is READY_TO_COMMIT
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT);
  EXPECT_NEXT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::SENT);

  // No more than two main frames in flight at a time; we should not send
  // this main frame until the next NotifyReadyToCommit().
  state.SetNeedsBeginMainFrame();

  // Complete the commit; the secondary BeginMainFrame gets promoted to primary.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
  EXPECT_NEXT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::IDLE);

  // Activate to vacate pending tree for another commit
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  // Cannot send the next main frame until the previous one is ready to commit
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);

  // Create a secondary BeginMainFrame and abort it; should not affect the
  // primary.
  state.NotifyReadyToCommit();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.BeginMainFrameAborted(CommitEarlyOutReason::kFinishedNoUpdates);
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT);
  EXPECT_NEXT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::IDLE);
  EXPECT_ACTION(SchedulerStateMachine::Action::COMMIT);
}

TEST(SchedulerStateMachineTest, TestFirstContextCreation) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  state.SetCanDraw(true);

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Check that the first init does not SetNeedsBeginMainFrame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Check that a needs commit initiates a BeginMainFrame.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, TestContextLostWhenCompletelyIdle) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  EXPECT_NE(SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION,
            state.NextAction());
  state.DidLoseLayerTreeFrameSink();

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Once context recreation begins, nothing should happen.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Recreate the context.
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();

  // When the context is recreated, we should begin a commit.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest,
     TestContextLostWhenIdleAndCommitRequestedWhileRecreating) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  EXPECT_NE(SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION,
            state.NextAction());
  state.DidLoseLayerTreeFrameSink();
  EXPECT_EQ(state.layer_tree_frame_sink_state(),
            SchedulerStateMachine::LayerTreeFrameSinkState::NONE);

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Once context recreation begins, nothing should happen.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // While context is recreating, commits shouldn't begin.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Recreate the context
  state.DidCreateAndInitializeLayerTreeFrameSink();
  EXPECT_EQ(
      state.layer_tree_frame_sink_state(),
      SchedulerStateMachine::LayerTreeFrameSinkState::WAITING_FOR_FIRST_COMMIT);
  EXPECT_FALSE(state.RedrawPending());

  // When the context is recreated, we wait until the next BeginImplFrame
  // before starting.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // When the BeginFrame comes in we should begin a commit
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);

  // Until that commit finishes, we shouldn't be drawing.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Finish the commit, which should make the surface active.
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_EQ(state.layer_tree_frame_sink_state(),
            SchedulerStateMachine::LayerTreeFrameSinkState::
                WAITING_FOR_FIRST_ACTIVATION);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_EQ(state.layer_tree_frame_sink_state(),
            SchedulerStateMachine::LayerTreeFrameSinkState::ACTIVE);

  // Finishing the first commit after initializing a LayerTreeFrameSink should
  // automatically cause a redraw.
  EXPECT_TRUE(state.RedrawPending());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_FALSE(state.RedrawPending());

  // Next frame as no work to do.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Once the context is recreated, whether we draw should be based on
  // SetCanDraw if waiting on first draw after activate.
  state.SetNeedsRedraw(true);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.SetCanDraw(false);
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);
  state.SetCanDraw(true);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Once the context is recreated, whether we draw should be based on
  // SetCanDraw if waiting on first draw after activate.
  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  // Activate so we need the first draw
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.SetCanDraw(false);
  EXPECT_ACTION(SchedulerStateMachine::Action::DRAW_ABORT);
  state.SetCanDraw(true);
  EXPECT_ACTION(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
}

TEST(SchedulerStateMachineTest, TestContextLostWhileCommitInProgress) {
  SchedulerSettings scheduler_settings;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state);

  // Get a commit in flight.
  state.SetNeedsBeginMainFrame();

  // Set damage and expect a draw.
  state.SetNeedsRedraw(true);
  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 10);
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Cause a lost context while the BeginMainFrame is in flight.
  state.DidLoseLayerTreeFrameSink();
  EXPECT_FALSE(state.BeginFrameNeeded());

  // Ask for another draw. Expect nothing happens.
  state.SetNeedsRedraw(true);
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);

  // Finish the frame, commit and activate.
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);

  // We will abort the draw when the LayerTreeFrameSink is lost if we are
  // waiting for the first draw to unblock the main thread.
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_ABORT);

  // Expect to begin context recreation only in BeginImplFrameState::IDLE
  EXPECT_IMPL_FRAME_STATE(SchedulerStateMachine::BeginImplFrameState::IDLE);
  EXPECT_ACTION(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);

  frame_id.sequence_number++;
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BeginImplFrameState::INSIDE_BEGIN_FRAME);
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);

  state.OnBeginImplFrameDeadline();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BeginImplFrameState::INSIDE_DEADLINE);
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest,
     TestContextLostWhileCommitInProgressAndAnotherCommitRequested) {
  SchedulerSettings scheduler_settings;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state);

  // Get a commit in flight.
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Set damage and expect a draw.
  state.SetNeedsRedraw(true);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Cause a lost context while the BeginMainFrame is in flight.
  state.DidLoseLayerTreeFrameSink();

  // Ask for another draw and also set needs commit. Expect nothing happens.
  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Finish the frame, and commit and activate.
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());

  // Because the LayerTreeFrameSink is missing, we expect the draw to abort.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_ABORT);

  // Expect to begin context recreation only in BeginImplFrameState::IDLE
  EXPECT_IMPL_FRAME_STATE(SchedulerStateMachine::BeginImplFrameState::IDLE);
  EXPECT_ACTION(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);

  state.IssueNextBeginImplFrame();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BeginImplFrameState::INSIDE_BEGIN_FRAME);
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);

  state.OnBeginImplFrameDeadline();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BeginImplFrameState::INSIDE_DEADLINE);
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);

  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);

  // After we get a new LayerTreeFrameSink, the commit flow should start.
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest,
     DontDrawBeforeCommitAfterLostLayerTreeFrameSink) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  state.SetNeedsRedraw(true);

  // Cause a lost LayerTreeFrameSink, and restore it.
  state.DidLoseLayerTreeFrameSink();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.DidCreateAndInitializeLayerTreeFrameSink();

  EXPECT_FALSE(state.RedrawPending());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION(SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest,
     TestShouldAbortCurrentFrameAfterLostLayerTreeFrameSink) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  state.SetBeginMainFrameState(
      SchedulerStateMachine::BeginMainFrameState::SENT);

  // Cause a lost context.
  state.DidLoseLayerTreeFrameSink();

  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);

  EXPECT_TRUE(state.ShouldAbortCurrentFrame());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);

  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_ABORT);
}

TEST(SchedulerStateMachineTest, TestNoBeginFrameNeededWhenInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();

  EXPECT_FALSE(state.BeginFrameNeeded());
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.BeginFrameNeeded());

  state.SetVisible(false);
  EXPECT_FALSE(state.BeginFrameNeeded());

  state.SetVisible(true);
  EXPECT_TRUE(state.BeginFrameNeeded());
}

TEST(SchedulerStateMachineTest, TestNoBeginMainFrameWhenInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  state.SetVisible(false);
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);
  EXPECT_FALSE(state.BeginFrameNeeded());

  // When become visible again, the needs commit should still be pending.
  state.SetVisible(true);
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, TestFinishCommitWhenCommitInProgress) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  state.SetVisible(false);
  state.SetBeginMainFrameState(
      SchedulerStateMachine::BeginMainFrameState::SENT);
  state.SetNeedsBeginMainFrame();

  // After the commit completes, activation and draw happen immediately
  // because we are not visible.
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_ABORT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest,
     TestFinishCommitWhenCommitInProgressAndBeginFrameSourcePaused) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  state.SetCanDraw(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.CreateAndInitializeLayerTreeFrameSinkWithActivatedCommit();
  state.SetBeginFrameSourcePaused(true);
  state.SetBeginMainFrameState(
      SchedulerStateMachine::BeginMainFrameState::SENT);
  state.SetNeedsBeginMainFrame();

  // After the commit completes, activation and draw happen immediately
  // because we are not visible.
  state.NotifyReadyToCommit();
  EXPECT_TRUE(state.ShouldAbortCurrentFrame());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_ABORT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_FALSE(state.active_tree_needs_first_draw());
  EXPECT_FALSE(state.needs_redraw());

  // Unpausing should draw again to ensure a frame is submitted for the commit.
  state.SetBeginFrameSourcePaused(false);
  EXPECT_TRUE(state.needs_redraw());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.DidReceiveCompositorFrameAck();
}

TEST(SchedulerStateMachineTest, TestInitialActionsWhenContextLost) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);
  state.SetNeedsBeginMainFrame();
  state.DidLoseLayerTreeFrameSink();

  // When we are visible, we normally want to begin LayerTreeFrameSink creation
  // as soon as possible.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);

  state.DidCreateAndInitializeLayerTreeFrameSink();
  EXPECT_EQ(
      state.layer_tree_frame_sink_state(),
      SchedulerStateMachine::LayerTreeFrameSinkState::WAITING_FOR_FIRST_COMMIT);

  // We should not send a BeginMainFrame when we are invisible, even if we've
  // lost the LayerTreeFrameSink and are trying to get the first commit, since
  // the
  // main thread will just abort anyway.
  state.SetVisible(false);
  EXPECT_ACTION(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, ReportIfNotDrawing) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(false);
  state.SetVisible(true);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(true);
  state.SetVisible(false);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(false);
  state.SetVisible(false);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(true);
  state.SetVisible(true);
  state.SetBeginFrameSourcePaused(true);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetBeginFrameSourcePaused(false);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());
}

TEST(SchedulerStateMachineTest, ForceDrawForResourcelessSoftwareDraw) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);
  state.SetResourcelessSoftwareDraw(true);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());

  state.SetVisible(false);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());

  state.SetResourcelessSoftwareDraw(false);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetResourcelessSoftwareDraw(true);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());
  state.SetVisible(true);

  state.SetBeginFrameSourcePaused(true);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());

  state.SetResourcelessSoftwareDraw(false);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetResourcelessSoftwareDraw(true);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());
  state.SetBeginFrameSourcePaused(false);

  state.SetVisible(false);
  state.DidLoseLayerTreeFrameSink();

  state.SetCanDraw(false);
  state.WillBeginLayerTreeFrameSinkCreation();
  state.DidCreateAndInitializeLayerTreeFrameSink();
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(true);
  state.DidLoseLayerTreeFrameSink();
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(true);
  state.WillBeginLayerTreeFrameSinkCreation();
  state.DidCreateAndInitializeLayerTreeFrameSink();
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(false);
  state.DidLoseLayerTreeFrameSink();
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());
}

TEST(SchedulerStateMachineTest,
     TestTriggerDeadlineImmediatelyAfterAbortedCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  // This test mirrors what happens during the first frame of a scroll gesture.
  // First we get the input event and a BeginFrame.
  state.IssueNextBeginImplFrame();

  // As a response the compositor requests a redraw and a commit to tell the
  // main thread about the new scroll offset.
  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();

  // We should start the commit normally.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Since only the scroll offset changed, the main thread will abort the
  // commit.
  state.BeginMainFrameAborted(CommitEarlyOutReason::kFinishedNoUpdates);

  // Since the commit was aborted, we should draw right away instead of waiting
  // for the deadline.
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
}

void FinishPreviousCommitAndDrawWithoutExitingDeadline(
    StateMachine* state_ptr) {
  // Gross, but allows us to use macros below.
  StateMachine& state = *state_ptr;

  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
}

TEST(SchedulerStateMachineTest, TestImplLatencyTakesPriorityImplInvalidations) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  // Set smoothness priority (used while scrolling).
  state.SetTreePrioritiesAndScrollState(
      SMOOTHNESS_TAKES_PRIORITY,
      ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER);

  // Impl-side invalidation creates a pending tree which is not yet activated.
  bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 1);
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  state.OnBeginImplFrameIdle();

  // Now we need a main frame.
  state.SetNeedsBeginMainFrame();
  frame_id.sequence_number++;
  state.OnBeginImplFrame(frame_id, kAnimateOnly);

  // We should send a BeginMainFrame even though we haven't drawn the impl
  // tree from last frame yet.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
}

TEST(SchedulerStateMachineTest,
     TestTriggerDeadlineImmediatelyOnLostLayerTreeFrameSink) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  state.SetNeedsBeginMainFrame();

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());

  state.DidLoseLayerTreeFrameSink();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  // The deadline should be triggered immediately when LayerTreeFrameSink is
  // lost.
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
}

TEST(SchedulerStateMachineTest, TestTriggerDeadlineImmediatelyWhenInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  state.SetNeedsBeginMainFrame();

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());

  state.SetVisible(false);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.ShouldAbortCurrentFrame());
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
}

TEST(SchedulerStateMachineTest,
     TestTriggerDeadlineImmediatelyWhenBeginFrameSourcePaused) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  state.SetNeedsBeginMainFrame();

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());

  state.SetBeginFrameSourcePaused(true);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_TRUE(state.ShouldAbortCurrentFrame());
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
}

TEST(SchedulerStateMachineTest, TestDeferBeginMainFrame) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  state.SetDeferBeginMainFrame(true);

  state.SetNeedsBeginMainFrame();
  EXPECT_FALSE(state.BeginFrameNeeded());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.SetDeferBeginMainFrame(false);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, EarlyOutCommitWantsProactiveBeginFrame) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  EXPECT_FALSE(state.ProactiveBeginFrameWanted());
  bool commit_has_no_updates = true;
  state.WillCommit(commit_has_no_updates);
  EXPECT_TRUE(state.ProactiveBeginFrameWanted());
  state.IssueNextBeginImplFrame();
  EXPECT_FALSE(state.ProactiveBeginFrameWanted());
}

TEST(SchedulerStateMachineTest,
     NoLayerTreeFrameSinkCreationWhileCommitPending) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Set up the request for a commit and start a frame.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  PerformAction(&state, SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);

  // Lose the LayerTreeFrameSink.
  state.DidLoseLayerTreeFrameSink();

  // The scheduler shouldn't trigger the LayerTreeFrameSink creation till the
  // previous commit has been cleared.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Trigger the deadline and ensure that the scheduler does not trigger any
  // actions until we receive a response for the pending commit.
  state.OnBeginImplFrameDeadline();
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Abort the commit, since that is what we expect the main thread to do if the
  // LayerTreeFrameSink was lost due to a synchronous call from the main thread
  // to release the LayerTreeFrameSink.
  state.BeginMainFrameAborted(CommitEarlyOutReason::kAbortedDeferredCommit);

  // The scheduler should begin the LayerTreeFrameSink creation now.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
}

TEST(SchedulerStateMachineTest, NoImplSideInvalidationsWhileInvisible) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // No impl-side invalidations should be performed while we are not visible.
  state.SetVisible(false);
  bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest,
     NoImplSideInvalidationsWhenBeginFrameSourcePaused) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // No impl-side invalidations should be performed while we can not make impl
  // frames.
  state.SetBeginFrameSourcePaused(true);
  bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest,
     ImplSideInvalidationAndMainFrame_NoMainFrameRequest) {
  // No main frame request or any update in the last frame, invalidation runs
  // immediately.
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest,
     ImplSideInvalidationAndMainFrame_MainFrameRequest_FastMainThread) {
  // Main frame request, no abort history and the main thread is fast,
  // invalidation waits for main frame.
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  bool needs_first_draw_on_activation = true;
  state.set_should_defer_invalidation_for_fast_main_frame(true);
  state.SetNeedsBeginMainFrame();
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest,
     ImplSideInvalidationAndMainFrame_LastFrameCommit) {
  // Main frame committed in the last impl frame and is fast, invalidation waits
  // for main frame request.
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  bool needs_first_draw_on_activation = true;
  state.set_should_defer_invalidation_for_fast_main_frame(true);
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest,
     ImplSideInvalidationAndMainFrame_LastFrameAborted) {
  // Last main frame was aborted. An invalidation is performed even if a main
  // frame request is pending.
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.BeginMainFrameAborted(CommitEarlyOutReason::kFinishedNoUpdates);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  bool needs_first_draw_on_activation = true;
  state.set_should_defer_invalidation_for_fast_main_frame(true);
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest,
     ImplSideInvalidationAndMainFrame_MainFrameRequest_SlowMainThread) {
  // Main frame request but the main thread is slow, invalidation runs
  // immediately.
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  bool needs_first_draw_on_activation = true;
  state.set_should_defer_invalidation_for_fast_main_frame(false);
  state.SetNeedsBeginMainFrame();
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest, NoImplSideInvalidationUntilFrameSinkActive) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Prefer impl side invalidation over begin main frame.
  state.set_should_defer_invalidation_for_fast_main_frame(false);

  state.DidLoseLayerTreeFrameSink();

  // Create new frame sink but don't commit or activate yet.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);

  state.DidCreateAndInitializeLayerTreeFrameSink();
  state.SetNeedsBeginMainFrame();

  bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  // No impl side invalidation because we're still waiting for first commit.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);

  state.OnBeginImplFrameDeadline();
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);

  state.IssueNextBeginImplFrame();
  // No impl side invalidation because we're still waiting for first activation.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.OnBeginImplFrameIdle();

  state.SetNeedsBeginMainFrame();
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  // Impl side invalidation only after receiving first commit and activation for
  // new frame sink.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest, ImplSideInvalidationWhenPendingTreeExists) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Set up request for the main frame, commit and create the pending tree.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);

  // Request an impl-side invalidation after the commit. The request should wait
  // till the current pending tree is activated.
  bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Activate the pending tree. Since the commit fills the impl-side
  // invalidation funnel as well, the request should wait until the next
  // BeginFrame.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Since there is no main frame request, this should perform impl-side
  // invalidations.
  state.set_should_defer_invalidation_for_fast_main_frame(false);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest, ImplSideInvalidationWhileReadyToCommit) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Set up request for the main frame with a slow main thread.
  state.set_should_defer_invalidation_for_fast_main_frame(false);
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyReadyToCommit();

  // Request an impl-side invalidation after we are ready to commit. The
  // invalidations are merged.
  bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  EXPECT_TRUE(state.needs_impl_side_invalidation());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_FALSE(state.needs_impl_side_invalidation());
}

TEST(SchedulerStateMachineTest,
     ConsecutiveImplSideInvalidationsWaitForBeginFrame) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Set up a request for impl-side invalidation.
  bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Request another invalidation, which should wait until the pending tree is
  // activated *and* we start the next BeginFrame.
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Now start the next frame, which will first draw the active tree and then
  // perform the pending impl-side invalidation request.
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest, ImplSideInvalidationsThrottledOnDraw) {
  // In commit_to_active_tree mode, performing the next invalidation should be
  // throttled on the active tree being drawn.
  SchedulerSettings settings;
  settings.commit_to_active_tree = true;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Commit to the sync tree, activate and draw.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  state.NotifyReadyToDraw();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Request impl-side invalidation and start a new frame, which should be
  // blocked on the ack for the previous frame.
  bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Ack the previous frame and begin impl frame, which should perform the
  // invalidation now.
  state.DidReceiveCompositorFrameAck();
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest, PrepareTilesWaitForImplSideInvalidation) {
  // PrepareTiles
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Request a PrepareTiles and impl-side invalidation. The impl-side
  // invalidation should run first, since it will perform PrepareTiles as well.
  bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.SetNeedsPrepareTiles();
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
  state.DidPrepareTiles();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

TEST(SchedulerStateMachineTest, TestFullPipelineMode) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.wait_for_all_pipeline_stages_before_draw = true;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state);

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // While we are waiting for an main frame or pending tree activation, we
  // should even block while we can't draw.
  state.SetCanDraw(false);

  // Begin the frame.
  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 10);
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());

  // If main thread defers commits, don't wait for it.
  state.SetDeferBeginMainFrame(true);
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());
  state.SetDeferBeginMainFrame(false);

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  // We are blocking on the main frame.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());

  // Tell the scheduler the frame finished.
  state.NotifyReadyToCommit();
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT);
  // We are blocking on commit.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());
  // Commit.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  // We are blocking on activation.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // We should prepare tiles even though we are not in the deadline, otherwise
  // we would get stuck here.
  EXPECT_FALSE(state.ShouldPrepareTiles());
  state.SetNeedsPrepareTiles();
  EXPECT_TRUE(state.ShouldPrepareTiles());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::PREPARE_TILES);

  // Ready to activate, but not draw.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  // We should no longer block, because can_draw is still false, and we are no
  // longer waiting for activation.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());

  // However, we should continue to block on ready to draw if we can draw.
  state.SetCanDraw(true);
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Ready to draw triggers immediate deadline.
  state.NotifyReadyToDraw();
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  // In full-pipe mode, CompositorFrameAck should always arrive before any
  // subsequent BeginFrame.
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Request a redraw without main frame.
  state.SetNeedsRedraw(true);

  // Redraw should happen immediately since there is no pending tree and active
  // tree is ready to draw.
  frame_id.sequence_number++;
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());

  // Redraw on impl-side only.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  // In full-pipe mode, CompositorFrameAck should always arrive before any
  // subsequent BeginFrame.
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Request a redraw on active frame and a main frame.
  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();

  frame_id.sequence_number++;
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  // Blocked on main frame.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());

  // Even with SMOOTHNESS_TAKES_PRIORITY, we don't prioritize impl thread and we
  // should wait for main frame.
  state.SetTreePrioritiesAndScrollState(
      SMOOTHNESS_TAKES_PRIORITY,
      ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER);
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());

  // Abort commit and ensure that we don't block anymore.
  state.BeginMainFrameAborted(CommitEarlyOutReason::kFinishedNoUpdates);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BeginMainFrameState::IDLE);
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());
}

TEST(SchedulerStateMachineTest, AllowSkippingActiveTreeFirstDraws) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Impl-side invalidation creates a pending tree which is activated but not
  // drawn in this frame.
  bool needs_first_draw_on_activation = false;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 1);
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  state.OnBeginImplFrameIdle();

  // Now we have a main frame.
  state.SetNeedsBeginMainFrame();
  frame_id.sequence_number++;
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);

  // We should be able to activate this tree without drawing the active tree.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
}

TEST(SchedulerStateMachineTest, DelayDrawIfAnimationWorkletsPending) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  // This test verifies that having pending mutations from Animation Worklets on
  // the active tree will not trigger the deadline early.
  state.SetNeedsBeginMainFrame();
  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 10);
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());
  // Started async mutation cycle for animation worklets.
  state.NotifyAnimationWorkletStateChange(
      SchedulerStateMachine::AnimationWorkletState::PROCESSING,
      SchedulerStateMachine::TreeType::ACTIVE);
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::REGULAR,
            state.CurrentBeginImplFrameDeadlineMode());
  // Second queued state change.
  state.NotifyAnimationWorkletStateChange(
      SchedulerStateMachine::AnimationWorkletState::PROCESSING,
      SchedulerStateMachine::TreeType::ACTIVE);
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::REGULAR,
            state.CurrentBeginImplFrameDeadlineMode());
  // First mutation cycle completes. Still waiting on queued mutation cycle
  // before being ready to draw.
  state.NotifyAnimationWorkletStateChange(
      SchedulerStateMachine::AnimationWorkletState::IDLE,
      SchedulerStateMachine::TreeType::ACTIVE);
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::REGULAR,
            state.CurrentBeginImplFrameDeadlineMode());
  // Queued mutation cycle completes. Now ready to draw.
  state.NotifyAnimationWorkletStateChange(
      SchedulerStateMachine::AnimationWorkletState::IDLE,
      SchedulerStateMachine::TreeType::ACTIVE);
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());

  // AnimationWorkletState does not effect CanDraw, only whether an early draw
  // deadline should be used (crbug/937975).
  state.SetNeedsRedraw(true);
  state.OnBeginImplFrameDeadline();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BeginImplFrameState::INSIDE_DEADLINE);
  EXPECT_TRUE(state.ShouldDraw());
  state.NotifyAnimationWorkletStateChange(
      SchedulerStateMachine::AnimationWorkletState::PROCESSING,
      SchedulerStateMachine::TreeType::ACTIVE);
  EXPECT_TRUE(state.ShouldDraw());
  state.NotifyAnimationWorkletStateChange(
      SchedulerStateMachine::AnimationWorkletState::IDLE,
      SchedulerStateMachine::TreeType::ACTIVE);
  EXPECT_TRUE(state.ShouldDraw());
}

TEST(SchedulerStateMachineTest, BlockActivationIfAnimationWorkletsPending) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Verify that pending mutations from Animation Worklets block activation.
  state.SetNeedsBeginMainFrame();
  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 10);
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  state.NotifyAnimationWorkletStateChange(
      SchedulerStateMachine::AnimationWorkletState::PROCESSING,
      SchedulerStateMachine::TreeType::PENDING);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyAnimationWorkletStateChange(
      SchedulerStateMachine::AnimationWorkletState::IDLE,
      SchedulerStateMachine::TreeType::PENDING);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
}

TEST(SchedulerStateMachineTest, BlockActivationIfPaintWorkletsPending) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  state.SetNeedsBeginMainFrame();
  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 10);
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Post-commit, we start working on PaintWorklets. It is not valid to activate
  // until they are done.
  state.NotifyPaintWorkletStateChange(
      SchedulerStateMachine::PaintWorkletState::PROCESSING);
  // We (correctly) cannot call state.NotifyReadyToActivate() here as it hits a
  // DCHECK because PaintWorklets are ongoing.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyPaintWorkletStateChange(
      SchedulerStateMachine::PaintWorkletState::IDLE);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
}

TEST(SchedulerStateMachineTest,
     BlockActivationIfPaintWorkletsPendingEvenWhenAbortingFrame) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  state.SetNeedsBeginMainFrame();
  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 10);
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Even if we are aborting the frame, we must paint the pending tree before we
  // activate it (because we cannot have an unpainted active tree).
  state.NotifyPaintWorkletStateChange(
      SchedulerStateMachine::PaintWorkletState::PROCESSING);
  state.SetVisible(false);
  ASSERT_TRUE(state.ShouldAbortCurrentFrame());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.NotifyPaintWorkletStateChange(
      SchedulerStateMachine::PaintWorkletState::IDLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
}

TEST(SchedulerStateMachineTest, TestFullPipelineModeDoesntBlockAfterCommit) {
  SchedulerSettings settings;
  settings.wait_for_all_pipeline_stages_before_draw = true;
  StateMachine state(settings);
  SET_UP_STATE(state);

  const bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.SetNeedsBeginMainFrame();
  state.SetNeedsRedraw(true);

  viz::BeginFrameId frame_id = viz::BeginFrameId(0, 10);
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  state.NotifyReadyToDraw();

  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BeginImplFrameState::INSIDE_BEGIN_FRAME);
  // Go all the way until ready to draw, but make sure we're not within
  // the frame deadline, so actual draw doesn't happen...
  EXPECT_FALSE(state.ShouldDraw());

  // ... then have another commit ...
  state.SetNeedsBeginMainFrame();
  frame_id.sequence_number++;
  state.OnBeginImplFrame(frame_id, kAnimateOnly);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);
  // ... and make sure we're in a state where we can proceed,
  // rather than draw being blocked by the pending tree.
  state.OnBeginImplFrameDeadline();
  EXPECT_TRUE(state.ShouldDraw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
}

TEST(SchedulerStateMachineTest,
     PauseRenderingSuppressesCommitsAndInvalidations) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Set up a main frame in a state where we're waiting for a commit.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);

  // Main thread sends a notification to pause rendering before this frame
  // commits.
  state.SetPauseRendering(true);

  // Finish the impl frame before the main thread responds.
  state.OnBeginImplFrameDeadline();

  // We're still subscribed to BeginFrames since the pending commit needs to be
  // activated and drawn.
  EXPECT_TRUE(state.ShouldSubscribeToBeginFrames());
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Now the main thread responds which triggers commit.
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::POST_COMMIT);

  // The frame finishes before we're ready to activate, should still remain
  // subscribed to BeginFrames for activation.
  state.OnBeginImplFrameDeadline();
  EXPECT_TRUE(state.ShouldSubscribeToBeginFrames());
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Trigger activation. We should still remain subscribed to BeginFrames for
  // draw.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.ShouldSubscribeToBeginFrames());
  EXPECT_TRUE(state.BeginFrameNeeded());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();

  // Request main frame. These are suppressed because rendering is paused.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.OnBeginImplFrameDeadline();

  // Request an impl-side invalidation and start a new frame. The invalidations
  // are paused to avoid new updates which have to be drained through the
  // pipeline.
  state.SetNeedsImplSideInvalidation(true);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Now that the last commit has been drawn, we shouldn't need BeginFrames
  // anymore.
  EXPECT_TRUE(state.ShouldSubscribeToBeginFrames());
  EXPECT_FALSE(state.BeginFrameNeeded());

  // Unpause rendering. This should send the suppressed BeginMainFrame and
  // trigger impl-side invalidation.
  state.SetPauseRendering(false);
  EXPECT_TRUE(state.ShouldSubscribeToBeginFrames());
  EXPECT_TRUE(state.BeginFrameNeeded());

  state.IssueNextBeginImplFrame();
  state.set_should_defer_invalidation_for_fast_main_frame(false);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::PERFORM_IMPL_SIDE_INVALIDATION);
}

// Text fixture class for the SchedulerStateMachine tests. Parameterized to
// include a boolean which indicates whether frame rate limits are enabled
// or not i.e. whether the disable_frame_rate_limit flag is set.
class DisableFrameRateLimitSchedulerStateMachineTests
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  SchedulerSettings GetSchedulerSettings() {
    SchedulerSettings settings;
    settings.disable_frame_rate_limit = GetParam();
    return settings;
  }
};

TEST_P(DisableFrameRateLimitSchedulerStateMachineTests,
       TestImplLatencyTakesPriority) {
  SchedulerSettings default_scheduler_settings = GetSchedulerSettings();
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state);

  // This test ensures that impl-draws are prioritized over main thread updates
  // in prefer impl latency mode.
  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Verify the deadline is not triggered early until we enter
  // prefer impl latency mode.
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  state.SetTreePrioritiesAndScrollState(
      SMOOTHNESS_TAKES_PRIORITY,
      ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER);
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());

  // Trigger the deadline.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.DidReceiveCompositorFrameAck();

  // Request a new commit and finish the previous one.
  state.SetNeedsBeginMainFrame();
  FinishPreviousCommitAndDrawWithoutExitingDeadline(&state);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Finish the previous commit and draw it.
  FinishPreviousCommitAndDrawWithoutExitingDeadline(&state);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);

  // Verify we do not send another BeginMainFrame if was are submit-frame
  // throttled and did not just submit one.
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.IssueNextBeginImplFrame();
  // If disable_frame_rate_limit is enabled, then draws aren't throttled in
  // the SchedulerStateMachine. We need to update the expectations accordingly.
  if (default_scheduler_settings.disable_frame_rate_limit) {
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME);
  } else {
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  }
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

INSTANTIATE_TEST_SUITE_P(DisableFrameRateLimitSchedulerStateMachineTests,
                         DisableFrameRateLimitSchedulerStateMachineTests,
                         testing::Bool());

// Text fixture class for the ScrollingSchedulerStateMachineTest tests.
// Parameterized to include a boolean which indicates whether frame rate limits
// are enabled or not i.e. whether the disable_frame_rate_limit flag is set.
class ScrollingSchedulerStateMachineTest
    : public DisableFrameRateLimitSchedulerStateMachineTests {
 public:
  ScrollingSchedulerStateMachineTest();
  ~ScrollingSchedulerStateMachineTest() override = default;

  void BeginImplFrameWaitingForScrollEvent();

  void SetUp() override;

 protected:
  SchedulerSettings InitScrollDeadlineMode();

  SchedulerSettings scheduler_settings_;

 public:
  // Having `state` private breaks the testing macros
  StateMachine state;
};

ScrollingSchedulerStateMachineTest::ScrollingSchedulerStateMachineTest()
    : state(InitScrollDeadlineMode()) {}

void ScrollingSchedulerStateMachineTest::BeginImplFrameWaitingForScrollEvent() {
  state.IssueNextBeginImplFrame();
  state.set_waiting_for_scroll_event(true);
}

SchedulerSettings ScrollingSchedulerStateMachineTest::InitScrollDeadlineMode() {
  scheduler_settings_ =
      DisableFrameRateLimitSchedulerStateMachineTests::GetSchedulerSettings();
  scheduler_settings_.scroll_deadline_mode_enabled = true;
  return scheduler_settings_;
}

void ScrollingSchedulerStateMachineTest::SetUp() {
  DisableFrameRateLimitSchedulerStateMachineTests::SetUp();
  SET_UP_STATE(state);
  state.set_is_scrolling(true);
  state.SetTreePrioritiesAndScrollState(
      SMOOTHNESS_TAKES_PRIORITY,
      ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER);
}

// Tests that when we should wait for scroll events, that we do not send
// BeginMainFrame. And that either receiving a scroll, or reaching the deadline,
// that we unblock BeginMainFrames.
TEST_P(ScrollingSchedulerStateMachineTest, ScrollModeBlocksBeginMainFrame) {
  state.SetNeedsBeginMainFrame();

  // Once the frame starts, we are told to wait for scroll event.
  BeginImplFrameWaitingForScrollEvent();
  EXPECT_TRUE(state.ShouldWaitForScrollEvent());
  EXPECT_FALSE(state.ShouldSendBeginMainFrame());

  // Once we are told to stop waiting, then BeginMainFrame should be unblocked.
  state.set_waiting_for_scroll_event(false);
  EXPECT_FALSE(state.ShouldWaitForScrollEvent());
  EXPECT_TRUE(state.ShouldSendBeginMainFrame());

  // Start next frame
  BeginImplFrameWaitingForScrollEvent();
  EXPECT_TRUE(state.ShouldWaitForScrollEvent());
  EXPECT_FALSE(state.ShouldSendBeginMainFrame());

  // The deadline should also unblock the BeginMainFrame.
  state.OnBeginImplFrameDeadline();
  EXPECT_FALSE(state.ShouldWaitForScrollEvent());
  EXPECT_TRUE(state.ShouldSendBeginMainFrame());
}

// Tests that even if we want to delay for scrolls, if we weren't prepared to
// draw immediately, that we use a longer deadline.
TEST_P(ScrollingSchedulerStateMachineTest, ScrollModeBlockedByNoImmediateMode) {
  // We apply back pressure on frame production. After submission we do not
  // allow Immediate mode until `DidReceiveCompositorFrameAck`.
  state.DidSubmitCompositorFrame();
  // We want to draw more.
  state.SetNeedsRedraw(true);

  // While under back-pressure we should not trigger scroll deadline
  BeginImplFrameWaitingForScrollEvent();
  // If disable_frame_rate_limit is set, then draws are not throttled. The
  // ShouldTriggerBeginImplFrameDeadlineImmediately() function returns true
  // in this case. Adjust the expectations accordingly. Please note that
  // the expectation changes are just for the tests to pass.
  //
  // The disable_frame_rate_limit switch is not enabled by default. It is
  // likely that some of the assumptions made in the SchedulerStateMachine
  // class are not true and we need further testing.
  if (scheduler_settings_.disable_frame_rate_limit) {
    EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  } else {
    EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  }
  EXPECT_TRUE(state.ShouldWaitForScrollEvent());
  // If disable_frame_rate_limit is set, then draws are not throttled. The
  // ShouldTriggerBeginImplFrameDeadlineImmediately() function returns true
  // in this case. Adjust the expectations accordingly.
  if (scheduler_settings_.disable_frame_rate_limit) {
    EXPECT_EQ(
        SchedulerStateMachine::BeginImplFrameDeadlineMode::WAIT_FOR_SCROLL,
        state.CurrentBeginImplFrameDeadlineMode());
  } else {
    EXPECT_NE(
        SchedulerStateMachine::BeginImplFrameDeadlineMode::WAIT_FOR_SCROLL,
        state.CurrentBeginImplFrameDeadlineMode());
  }

  // When we receive the Ack then we should be able select scroll deadline
  // again.
  state.DidReceiveCompositorFrameAck();
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  EXPECT_TRUE(state.ShouldWaitForScrollEvent());
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::WAIT_FOR_SCROLL,
            state.CurrentBeginImplFrameDeadlineMode());
}

INSTANTIATE_TEST_SUITE_P(ScrollingSchedulerStateMachineTest,
                         ScrollingSchedulerStateMachineTest,
                         testing::Bool());

class WarmUpCompositorSchedulerStateMachineTest : public testing::Test {
 public:
  WarmUpCompositorSchedulerStateMachineTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWarmUpCompositor);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that `SetShouldWarmUp()` will start initial `LayerTreeFrameSink`
// creation even if invisible.
TEST_F(WarmUpCompositorSchedulerStateMachineTest,
       SetShouldWarmUpWillStartLayerTreeFrameSinkCreation) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(false);

  state.SetShouldWarmUp();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  state.DidCreateAndInitializeLayerTreeFrameSink();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
}

}  // namespace
}  // namespace cc
