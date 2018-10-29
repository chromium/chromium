// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"

#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {
// TODO(edcourtney): Consider varying the animation duration based on how far
// the pip window has to move.
const int kPipSnapToEdgeAnimationDurationMs = 50;
}  // namespace

PipWindowResizer::PipWindowResizer(wm::WindowState* window_state)
    : WindowResizer(window_state) {
  window_state->OnDragStarted(details().window_component);
}

PipWindowResizer::~PipWindowResizer() {}

void PipWindowResizer::Drag(const gfx::Point& location_in_parent,
                            int event_flags) {
  last_location_in_screen_ = location_in_parent;
  ::wm::ConvertPointToScreen(GetTarget()->parent(), &last_location_in_screen_);

  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  display::Display display = window_state()->GetDisplay();

  ::wm::ConvertRectToScreen(GetTarget()->parent(), &bounds);
  bounds = PipPositioner::GetBoundsForDrag(display, bounds);
  ::wm::ConvertRectFromScreen(GetTarget()->parent(), &bounds);

  if (bounds != GetTarget()->bounds()) {
    moved_or_resized_ = true;
    GetTarget()->SetBounds(bounds);
  }
}

void PipWindowResizer::CompleteDrag() {
  window_state()->OnCompleteDrag(last_location_in_screen_);
  window_state()->DeleteDragDetails();
  window_state()->ClearRestoreBounds();
  window_state()->set_bounds_changed_by_user(moved_or_resized_);

  // Animate the PIP window to its resting position.
  gfx::Rect bounds = PipPositioner::GetRestingPosition(
      window_state()->GetDisplay(), GetTarget()->GetBoundsInScreen());
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kPipSnapToEdgeAnimationDurationMs);
  wm::SetBoundsEvent event(wm::WM_EVENT_SET_BOUNDS, bounds, /*animate=*/true,
                           duration);
  window_state()->OnWMEvent(&event);

  // If the pip work area changes (e.g. message center, virtual keyboard),
  // we want to restore to the last explicitly set position.
  // TODO(edcourtney): This may not be the best place for this. Consider
  // doing this a different way or saving these bounds at a later point when
  // the work area changes.
  window_state()->SaveCurrentBoundsForRestore();
}

void PipWindowResizer::RevertDrag() {
  // Handle cancel as a complete drag for pip. Having the PIP window
  // go back to where it was on cancel looks strange, so instead just
  // will just stop it where it is and animate to the edge of the screen.
  CompleteDrag();
}

void PipWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {
  CompleteDrag();
}

}  // namespace ash
