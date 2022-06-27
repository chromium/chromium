// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/tablet_mode_float_window_resizer.h"

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
  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  if (bounds != GetTarget()->bounds())
    SetBoundsDuringResize(bounds);
}

void TabletModeFloatWindowResizer::CompleteDrag() {
  // Keep the bounds where it was last dragged to.
  // TODO(crbug.com/1338715): Magnetize to the closest corner on release.
}

void TabletModeFloatWindowResizer::RevertDrag() {
  GetTarget()->SetBounds(details().initial_bounds_in_parent);
}

void TabletModeFloatWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {}

}  // namespace ash
