// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/slim_scheduler_state_machine.h"

#include "base/time/time.h"
#include "cc/scheduler/scheduler_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(SlimSchedulerStateMachineTest, NextActionIsOncePerFrame) {
  SchedulerSettings settings;
  SlimSchedulerStateMachine state_machine(settings);
  state_machine.SetVisible(true);
  state_machine.SetCanDraw(true);
  state_machine.WillBeginLayerTreeFrameSinkCreation();
  state_machine.DidCreateAndInitializeLayerTreeFrameSink();
  state_machine.WillCommit(/*commit_had_no_updates=*/false);
  state_machine.WillActivate();

  // Request a redraw.
  state_machine.SetNeedsRedraw();

  // Begin a frame.
  viz::BeginFrameArgs args = viz::BeginFrameArgs();
  args.frame_id = viz::BeginFrameId(1, 1);
  args.frame_time = base::TimeTicks::Now();
  args.interval = base::Milliseconds(16);

  state_machine.OnBeginImplFrame(args);
  state_machine.OnBeginImplFrameDeadline();

  // The first action should be DRAW_IF_POSSIBLE.
  EXPECT_EQ(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE,
            state_machine.NextAction());

  // Subsequent calls to NextAction should return NONE.
  EXPECT_EQ(SchedulerStateMachine::Action::NONE, state_machine.NextAction());

  state_machine.OnBeginImplFrameIdle();

  // After the frame is idle, we can begin a new frame.
  args.frame_time += base::Milliseconds(16);
  state_machine.OnBeginImplFrame(args);

  // Request a redraw again.
  state_machine.SetNeedsRedraw();
  state_machine.OnBeginImplFrameDeadline();

  // The first action should be DRAW_IF_POSSIBLE again.
  EXPECT_EQ(SchedulerStateMachine::Action::DRAW_IF_POSSIBLE,
            state_machine.NextAction());
  EXPECT_EQ(SchedulerStateMachine::Action::NONE, state_machine.NextAction());

  state_machine.OnBeginImplFrameIdle();
}

TEST(SlimSchedulerStateMachineTest, ActionsInIdleState) {
  SchedulerSettings settings;
  SlimSchedulerStateMachine state_machine(settings);
  state_machine.SetVisible(true);
  state_machine.SetCanDraw(true);
  state_machine.WillBeginLayerTreeFrameSinkCreation();
  state_machine.DidCreateAndInitializeLayerTreeFrameSink();

  // Start a frame to reset did_send_begin_main_frame_for_current_frame_.
  viz::BeginFrameArgs args = viz::BeginFrameArgs();
  args.frame_id = viz::BeginFrameId(1, 1);
  args.frame_time = base::TimeTicks::Now();
  args.interval = base::Milliseconds(16);
  state_machine.OnBeginImplFrame(args);

  // Transition to IDLE state.
  // We must set needs_begin_main_frame_ before going idle so that
  // did_send_begin_main_frame_for_current_frame_ is not set to true in
  // OnBeginImplFrameIdle.
  state_machine.SetNeedsBeginMainFrame(/*now=*/false);
  state_machine.OnBeginImplFrameDeadline();
  state_machine.OnBeginImplFrameIdle();

  // Ensure we are in IDLE state.
  EXPECT_EQ(SchedulerStateMachine::BeginImplFrameState::IDLE,
            state_machine.begin_impl_frame_state());

  // 1. Validate SEND_BEGIN_MAIN_FRAME can happen while idle.
  EXPECT_EQ(SchedulerStateMachine::Action::SEND_BEGIN_MAIN_FRAME,
            state_machine.NextAction());
  state_machine.WillSendBeginMainFrame();

  // 2. Validate COMMIT can happen while idle.
  state_machine.NotifyReadyToCommit();
  EXPECT_EQ(SchedulerStateMachine::Action::COMMIT, state_machine.NextAction());
  state_machine.WillCommit(/*commit_had_no_updates=*/false);
  state_machine.DidCommit();

  // 3. Validate ACTIVATE_SYNC_TREE can happen while idle.
  state_machine.NotifyReadyToActivate();
  // POST_COMMIT has higher priority than ACTIVATE_SYNC_TREE in IDLE state.
  EXPECT_EQ(SchedulerStateMachine::Action::POST_COMMIT,
            state_machine.NextAction());
  EXPECT_EQ(SchedulerStateMachine::Action::ACTIVATE_SYNC_TREE,
            state_machine.NextAction());
  state_machine.WillActivate();
}

}  // namespace
}  // namespace cc
