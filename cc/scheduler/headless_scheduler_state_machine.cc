// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/headless_scheduler_state_machine.h"

#include "base/logging.h"
#include "cc/scheduler/scheduler_state_machine.h"

namespace cc {

HeadlessSchedulerStateMachine::HeadlessSchedulerStateMachine(
    const SchedulerSettings& settings)
    : SchedulerStateMachine(settings) {
  DCHECK(settings.wait_for_all_pipeline_stages_before_draw);
}

HeadlessSchedulerStateMachine::~HeadlessSchedulerStateMachine() = default;

bool HeadlessSchedulerStateMachine::CheckShouldDraw() const {
  // Wait for ready to draw in full-pipeline mode.
  return !active_tree_is_ready_to_draw_;
}

bool HeadlessSchedulerStateMachine::ShouldPrepareTiles() const {
  // In full-pipeline mode, we need to prepare tiles ASAP to ensure that we
  // don't get stuck.
  return needs_prepare_tiles_;
}

bool HeadlessSchedulerStateMachine::ShouldSubscribeToBeginFrames() const {
  // We can't handle BeginFrames when output surface isn't initialized.
  if (!HasInitializedLayerTreeFrameSink()) {
    return false;
  }

  // The propagation of the NeedsBeginFrame signal to viz is inherently racy
  // with issuing the next BeginFrame. In full-pipe mode, it is important we
  // don't miss a BeginFrame because our needsBeginFrames signal propagated to
  // viz too slowly. To avoid the race, we simply always request BeginFrames
  // from viz.
  return true;
}

// Avoids check against `commit_to_active_tree` in the classic
// scheduler.
bool HeadlessSchedulerStateMachine::CheckShouldBlockDeadlineIndefinitely()
    const {
  return false;
}

bool HeadlessSchedulerStateMachine::
    ShouldTriggerBeginImplFrameDeadlineImmediately() const {
  // If we aborted the current frame we should end the deadline right now.
  if (ShouldAbortCurrentFrame() && !has_pending_tree_) {
    return true;
  }

  // Throttle the deadline on CompositorFrameAck since we wont draw and submit
  // anyway.
  if (IsDrawThrottled()) {
    return false;
  }

  // Delay immediate draws when we have pending animation worklet updates to
  // give them time to produce output before we draw.
  if (processing_animation_worklets_for_active_tree_) {
    return false;
  }

  // In full-pipe mode, we just gave all pipeline stages a chance to contribute.
  // We shouldn't wait any longer in any case - even if there are no updates.
  return true;
}

bool HeadlessSchedulerStateMachine::CheckWillCommit() const {
  return !active_tree_needs_first_draw_;
}

}  // namespace cc
