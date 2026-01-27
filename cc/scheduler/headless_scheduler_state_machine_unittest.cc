// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/headless_scheduler_state_machine.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/scheduler.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/scheduler/scheduler_state_machine.h"
#include "cc/tiles/tile_priority.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
using Action = SchedulerStateMachine::Action;
using BeginImplFrameState = SchedulerStateMachine::BeginImplFrameState;
namespace {

SchedulerSettings HeadlessSchedulerSettings() {
  SchedulerSettings scheduler_settings;
  scheduler_settings.wait_for_all_pipeline_stages_before_draw = true;
  return scheduler_settings;
}

class HeadlessSchedulerTestStateMachine : public HeadlessSchedulerStateMachine {
 public:
  HeadlessSchedulerTestStateMachine()
      : HeadlessSchedulerStateMachine(HeadlessSchedulerSettings()) {}

  void IssueBeginImplFrame(uint64_t sequence_number) {
    auto args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, sequence_number, now_ticks_,
        now_ticks_ + base::Hertz(60), base::Hertz(60),
        viz::BeginFrameArgs::NORMAL);
    OnBeginImplFrame(args);
  }

  void SetUpState() {
    SetVisible(true);
    SetCanDraw(true);
    WillBeginLayerTreeFrameSinkCreation();
    DidCreateAndInitializeLayerTreeFrameSink();
  }

  // Convenience methods for accessing private members of
  // SchedulerStateMachine.
  bool NeedsCommit() const { return needs_begin_main_frame_; }

  BeginMainFrameState GetBeginMainFrameState() const {
    return begin_main_frame_state_;
  }

  bool ShouldDraw() const {
    return HeadlessSchedulerStateMachine::ShouldDraw();
  }

  void SetNeedsRedraw(bool needs_redraw) { needs_redraw_ = needs_redraw; }

 private:
  base::TimeTicks now_ticks_;
};

// TODO(gaiko): For each NONE check, we need a
//  corresponding `if(state.begin_impl_frame_state() == INSIDE_DEADLINE)

TEST(HeadlessSchedulerStateMachineTest, TestFullPipelineMode) {
  HeadlessSchedulerTestStateMachine state;
  state.SetUpState();

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // While we are waiting for an main frame or pending tree activation, we
  // should even block while we can't draw.
  state.SetCanDraw(false);

  // Begin the frame.
  uint64_t sequence_number = 10;
  state.IssueBeginImplFrame(sequence_number);
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());

  // If main thread defers commits, don't wait for it.
  state.SetDeferBeginMainFrame(true);
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());
  state.SetDeferBeginMainFrame(false);

  EXPECT_EQ(Action::SEND_BEGIN_MAIN_FRAME, state.NextAction());
  state.WillSendBeginMainFrame();

  EXPECT_EQ(SchedulerStateMachine::BeginMainFrameState::SENT,
            state.GetBeginMainFrameState());
  EXPECT_FALSE(state.NeedsCommit());

  EXPECT_EQ(Action::NONE, state.NextAction());
  // We are blocking on the main frame.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());

  // Tell the scheduler the frame finished.
  state.NotifyReadyToCommit();
  EXPECT_EQ(SchedulerStateMachine::BeginMainFrameState::READY_TO_COMMIT,
            state.GetBeginMainFrameState());
  // We are blocking on commit.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());
  // Commit.
  EXPECT_EQ(Action::COMMIT, state.NextAction());
  state.WillCommit(/*commit_had_no_updates=*/false);
  state.DidCommit();
  EXPECT_EQ(Action::POST_COMMIT, state.NextAction());
  state.DidPostCommit();
  // We are blocking on activation.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());
  EXPECT_EQ(Action::NONE, state.NextAction());

  // We should prepare tiles even though we are not in the deadline, otherwise
  // we would get stuck here.
  EXPECT_FALSE(state.ShouldPrepareTiles());
  state.SetNeedsPrepareTiles();
  EXPECT_TRUE(state.ShouldPrepareTiles());
  EXPECT_EQ(Action::PREPARE_TILES, state.NextAction());
  state.WillPrepareTiles();

  // Ready to activate, but not draw.
  state.NotifyReadyToActivate();
  EXPECT_EQ(Action::ACTIVATE_SYNC_TREE, state.NextAction());
  state.WillActivate();
  // We should no longer block, because can_draw is still false, and we are no
  // longer waiting for activation.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());

  // However, we should continue to block on ready to draw if we can draw.
  state.SetCanDraw(true);
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());
  EXPECT_EQ(Action::NONE, state.NextAction());

  // Ready to draw triggers immediate deadline.
  state.NotifyReadyToDraw();
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());

  state.OnBeginImplFrameDeadline();
  EXPECT_EQ(Action::DRAW_IF_POSSIBLE, state.NextAction());
  EXPECT_EQ(BeginImplFrameState::INSIDE_DEADLINE,
            state.begin_impl_frame_state());
  state.WillDraw();
  state.DidDraw(DrawResult::kSuccess);
  state.DidSubmitCompositorFrame();
  EXPECT_EQ(Action::NONE, state.NextAction());

  // In full-pipe mode, CompositorFrameAck should always arrive before any
  // subsequent BeginFrame.
  state.DidReceiveCompositorFrameAck();
  EXPECT_EQ(Action::NONE, state.NextAction());

  // Request a redraw without main frame.
  state.SetNeedsRedraw(true);

  // Redraw should happen immediately since there is no pending tree and active
  // tree is ready to draw.
  sequence_number++;
  state.IssueBeginImplFrame(sequence_number);
  EXPECT_EQ(Action::NONE, state.NextAction());
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());

  // Redraw on impl-side only.
  state.OnBeginImplFrameDeadline();
  EXPECT_EQ(Action::DRAW_IF_POSSIBLE, state.NextAction());
  EXPECT_EQ(BeginImplFrameState::INSIDE_DEADLINE,
            state.begin_impl_frame_state());
  state.WillDraw();
  state.DidDraw(DrawResult::kSuccess);
  EXPECT_EQ(Action::NONE, state.NextAction());
  state.DidSubmitCompositorFrame();
  EXPECT_EQ(Action::NONE, state.NextAction());

  // In full-pipe mode, CompositorFrameAck should always arrive before any
  // subsequent BeginFrame.
  state.DidReceiveCompositorFrameAck();
  EXPECT_EQ(Action::NONE, state.NextAction());

  // Request a redraw on active frame and a main frame.
  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();

  sequence_number++;
  state.IssueBeginImplFrame(sequence_number);
  EXPECT_EQ(Action::SEND_BEGIN_MAIN_FRAME, state.NextAction());
  state.WillSendBeginMainFrame();
  EXPECT_EQ(Action::NONE, state.NextAction());
  // Blocked on main frame.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());

  // Even with SMOOTHNESS_TAKES_PRIORITY, we don't prioritize impl thread and we
  // should wait for main frame.
  state.SetTreePrioritiesAndScrollState(
      SMOOTHNESS_TAKES_PRIORITY,
      ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER, false);
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::BLOCKED,
            state.CurrentBeginImplFrameDeadlineMode());

  // Abort commit and ensure that we don't block anymore.
  state.BeginMainFrameAborted(CommitEarlyOutReason::kFinishedNoUpdates);
  //   EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::Action::NONE);
  EXPECT_EQ(Action::NONE, state.NextAction());
  EXPECT_EQ(SchedulerStateMachine::BeginMainFrameState::IDLE,
            state.GetBeginMainFrameState());
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameDeadlineMode::IMMEDIATE,
            state.CurrentBeginImplFrameDeadlineMode());
}

TEST(HeadlessSchedulerStateMachineTest,
     TestFullPipelineModeDoesntBlockAfterCommit) {
  HeadlessSchedulerTestStateMachine state;
  state.SetUpState();

  const bool needs_first_draw_on_activation = true;
  state.SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
  state.SetNeedsBeginMainFrame();
  state.SetNeedsRedraw(true);

  uint64_t sequence_number = 10;
  state.IssueBeginImplFrame(sequence_number);
  EXPECT_EQ(Action::SEND_BEGIN_MAIN_FRAME, state.NextAction());
  state.WillSendBeginMainFrame();
  state.NotifyReadyToCommit();
  // Commit.
  EXPECT_EQ(Action::COMMIT, state.NextAction());
  state.WillCommit(/*commit_had_no_updates=*/false);
  state.DidCommit();
  EXPECT_EQ(Action::POST_COMMIT, state.NextAction());
  state.DidPostCommit();
  EXPECT_EQ(Action::NONE, state.NextAction());

  // Activate
  state.NotifyReadyToActivate();
  EXPECT_EQ(Action::ACTIVATE_SYNC_TREE, state.NextAction());
  state.WillActivate();
  state.NotifyReadyToDraw();

  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_EQ(BeginImplFrameState::INSIDE_BEGIN_FRAME,
            state.begin_impl_frame_state());
  // Go all the way until ready to draw, but make sure we're not within
  // the frame deadline, so actual draw doesn't happen...
  EXPECT_FALSE(state.ShouldDraw());

  // ... then have another commit ...
  state.SetNeedsBeginMainFrame();
  sequence_number++;
  state.IssueBeginImplFrame(sequence_number);
  EXPECT_EQ(Action::SEND_BEGIN_MAIN_FRAME, state.NextAction());
  state.WillSendBeginMainFrame();
  state.NotifyReadyToCommit();
  EXPECT_EQ(Action::COMMIT, state.NextAction());
  state.WillCommit(/*commit_had_no_updates=*/false);
  state.DidCommit();
  EXPECT_EQ(Action::POST_COMMIT, state.NextAction());
  state.DidPostCommit();
  // ... and make sure we're in a state where we can proceed,
  // rather than draw being blocked by the pending tree.
  state.OnBeginImplFrameDeadline();
  EXPECT_TRUE(state.ShouldDraw());
  EXPECT_EQ(Action::DRAW_IF_POSSIBLE, state.NextAction());
  EXPECT_EQ(BeginImplFrameState::INSIDE_DEADLINE,
            state.begin_impl_frame_state());
  state.WillDraw();
  state.DidDraw(DrawResult::kSuccess);
}

}  // namespace
}  // namespace cc
