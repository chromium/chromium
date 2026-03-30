// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/slim_scheduler_state_machine.h"

#include "base/logging.h"
#include "cc/scheduler/scheduler_state_machine.h"

namespace cc {
using Action = SchedulerStateMachine::Action;

SlimSchedulerStateMachine::SlimSchedulerStateMachine(
    const SchedulerSettings& settings)
    : SchedulerStateMachine(settings) {}

SlimSchedulerStateMachine::~SlimSchedulerStateMachine() = default;

SchedulerStateMachine::Action SlimSchedulerStateMachine::NextAction() const {
  Action action = Action::NONE;
  switch (begin_impl_frame_state_) {
    case BeginImplFrameState::IDLE:
      if (!issued_actions_.count(Action::SEND_BEGIN_MAIN_FRAME) &&
          ShouldSendBeginMainFrame()) {
        action = Action::SEND_BEGIN_MAIN_FRAME;
      } else if (!issued_actions_.count(Action::POST_COMMIT) &&
                 ShouldRunPostCommit()) {
        action = Action::POST_COMMIT;
      } else if (!issued_actions_.count(Action::ACTIVATE_SYNC_TREE) &&
                 ShouldActivateSyncTree()) {
        action = Action::ACTIVATE_SYNC_TREE;
      } else if (!issued_actions_.count(Action::COMMIT) && ShouldCommit()) {
        action = Action::COMMIT;
      } else if (!issued_actions_.count(Action::PREPARE_TILES) &&
                 needs_prepare_tiles_ && !did_prepare_tiles_) {
        // `ShouldPrepareTiles` is restricted to within
        // `BeginImplFrameState::INSIDE_DEADLINE`. However we want tile
        // preparation to only outside of frame production.
        action = Action::PREPARE_TILES;
      } else if (!issued_actions_.count(
                     Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION) &&
                 ShouldBeginLayerTreeFrameSinkCreation()) {
        action = Action::BEGIN_LAYER_TREE_FRAME_SINK_CREATION;
      }
      break;
    case BeginImplFrameState::INSIDE_BEGIN_FRAME:
    case BeginImplFrameState::INSIDE_DEADLINE:
      if (!issued_actions_.count(Action::SEND_BEGIN_MAIN_FRAME) &&
          ShouldSendBeginMainFrame()) {
        action = Action::SEND_BEGIN_MAIN_FRAME;
      } else if (ShouldDraw()) {
        if (!issued_actions_.count(Action::DRAW_ABORT) &&
            PendingDrawsShouldBeAborted()) {
          action = Action::DRAW_ABORT;
        } else if (!issued_actions_.count(Action::DRAW_FORCED) &&
                   forced_redraw_state_ ==
                       ForcedRedrawOnTimeoutState::WAITING_FOR_DRAW) {
          action = Action::DRAW_FORCED;
        } else if (!issued_actions_.count(Action::DRAW_IF_POSSIBLE)) {
          action = Action::DRAW_IF_POSSIBLE;
        }
      } else if (!issued_actions_.count(
                     Action::PERFORM_IMPL_SIDE_INVALIDATION) &&
                 ShouldPerformImplSideInvalidation()) {
        action = Action::PERFORM_IMPL_SIDE_INVALIDATION;
      } else if (!issued_actions_.count(
                     Action::INVALIDATE_LAYER_TREE_FRAME_SINK) &&
                 ShouldInvalidateLayerTreeFrameSink()) {
        action = Action::INVALIDATE_LAYER_TREE_FRAME_SINK;
      }
      break;
  }

  issued_actions_.insert(action);
  return action;
}

SchedulerStateMachine::BeginImplFrameDeadlineMode
SlimSchedulerStateMachine::CurrentBeginImplFrameDeadlineMode() const {
  auto deadline = SchedulerStateMachine::CurrentBeginImplFrameDeadlineMode();
  if (deadline == BeginImplFrameDeadlineMode::REGULAR ||
      deadline == BeginImplFrameDeadlineMode::LATE) {
    deadline = BeginImplFrameDeadlineMode::IMMEDIATE;
  }
  return deadline;
}

void SlimSchedulerStateMachine::OnBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  issued_actions_.clear();
  SchedulerStateMachine::OnBeginImplFrame(args);
}

bool SlimSchedulerStateMachine::ShouldBlockBeginMainFrameWhenIdle() const {
  // Since we prioritize submitting the current frame at the start of VSync, we
  // must allow the Main frame to start while we are idle.
  return false;
}

}  // namespace cc
