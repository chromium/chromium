// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/tablet_mode_float_window_resizer.h"

#include "ash/shell.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/window_state.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/window.h"

namespace ash {

TabletModeFloatWindowResizer::TabletModeFloatWindowResizer(
    WindowState* window_state)
    : WindowResizer(window_state) {
  DCHECK(chromeos::wm::features::IsFloatWindowEnabled());
}

TabletModeFloatWindowResizer::~TabletModeFloatWindowResizer() {
  window_state_->DeleteDragDetails();
}

void TabletModeFloatWindowResizer::Drag(const gfx::PointF& location_in_parent,
                                        int event_flags) {
  last_location_in_parent_ = location_in_parent;

  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  if (bounds != GetTarget()->bounds())
    SetBoundsDuringResize(bounds);
}

void TabletModeFloatWindowResizer::CompleteDrag() {
  aura::Window* float_window = GetTarget();
  auto* float_controller = Shell::Get()->float_controller();
  DCHECK(float_controller->IsFloated(float_window));
  float_controller->OnDragCompleted(last_location_in_parent_);
}

void TabletModeFloatWindowResizer::RevertDrag() {
  GetTarget()->SetBounds(details().initial_bounds_in_parent);
}

void TabletModeFloatWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {
  // TODO(crbug.com/1338715): Tuck the window to the side on fling or swipe.
}

}  // namespace ash
