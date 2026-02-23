// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/webview_scheduler_state_machine.h"

namespace cc {

WebviewSchedulerStateMachine::WebviewSchedulerStateMachine(
    const SchedulerSettings& settings)
    : SchedulerStateMachine(settings) {
  DCHECK(settings.using_synchronous_renderer_compositor);
}

WebviewSchedulerStateMachine::~WebviewSchedulerStateMachine() = default;

bool WebviewSchedulerStateMachine::ShouldActivateSyncTreeBeforeDraw() const {
  // We should not activate a second tree before drawing the first one.
  // Relax this requirement for synchronous compositor where scheduler does not
  // control draw, and blocking commit may lead to bad scheduling.
  return false;
}

bool WebviewSchedulerStateMachine::ShouldBeginMainFrameWhenIdle() const {
  // It's okay to send BeginMainFrame for the synchronous compositor
  // because the main thread is always high latency in that case.
  return false;
}

// TODO(crbug.com/485930107): Move invalidation-related bools
// to Webview-only scheduler.
bool WebviewSchedulerStateMachine::ShouldInvalidateLayerTreeFrameSink() const {
  // Do not invalidate more than once per begin frame.
  if (did_invalidate_layer_tree_frame_sink_) {
    return false;
  }

  // Only the synchronous compositor requires invalidations.
  // Invalidations are only performed inside a BeginFrame.
  if (begin_impl_frame_state_ != BeginImplFrameState::INSIDE_BEGIN_FRAME) {
    return false;
  }

  // Don't invalidate for draw if we cannot draw.
  // TODO(sunnyps): needs_prepare_tiles_ is needed here because PrepareTiles is
  // called only inside the deadline / draw phase. We could remove this if we
  // allowed PrepareTiles to happen in OnBeginImplFrame.
  return (needs_redraw_ && !PendingDrawsShouldBeAborted()) ||
         needs_prepare_tiles_;
}

SchedulerStateMachine::BeginImplFrameDeadlineMode
WebviewSchedulerStateMachine::CurrentBeginImplFrameDeadlineMode() const {
  // No deadline for synchronous compositor, or when outside the begin frame.
  return BeginImplFrameDeadlineMode::NONE;
}

}  // namespace cc
