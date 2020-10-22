// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_launcher_gesture_handler.h"

#include <algorithm>

#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/swipe_home_to_overview_controller.h"
#include "ash/shell.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace ash {

HomeLauncherGestureHandler::HomeLauncherGestureHandler() {
  tablet_mode_observer_.Add(Shell::Get()->tablet_mode_controller());
}

HomeLauncherGestureHandler::~HomeLauncherGestureHandler() {
}

bool HomeLauncherGestureHandler::OnPressEvent(Mode mode,
                                              const gfx::PointF& location) {
  // Do not start a new session if a window is currently being processed.
  if (!IsIdle())
    return false;

  display_ = display::Screen::GetScreen()->GetDisplayNearestPoint(
      gfx::ToRoundedPoint(location));
  if (!display_.is_valid())
    return false;

  if (!Shell::Get()->home_screen_controller()->IsHomeScreenVisible())
    return false;

  mode_ = mode;

  swipe_home_to_overview_controller_ =
      std::make_unique<SwipeHomeToOverviewController>(display_.id());
  return true;
}

bool HomeLauncherGestureHandler::OnScrollEvent(const gfx::PointF& location,
                                               float scroll_x,
                                               float scroll_y) {
  if (!IsDragInProgress())
    return false;

  DCHECK(display_.is_valid());

  swipe_home_to_overview_controller_->Drag(location, scroll_x, scroll_y);
  return true;
}

bool HomeLauncherGestureHandler::OnReleaseEvent(
    const gfx::PointF& location,
    base::Optional<float> velocity_y) {
  if (mode_ != Mode::kSwipeHomeToOverview)
    return false;

  swipe_home_to_overview_controller_->EndDrag(location, velocity_y);
  Reset();
  return true;
}

void HomeLauncherGestureHandler::Cancel() {
  if (!IsDragInProgress())
    return;

  swipe_home_to_overview_controller_->CancelDrag();
  Reset();
}

bool HomeLauncherGestureHandler::IsDragInProgress() const {
  return mode_ != Mode::kNone;
}

void HomeLauncherGestureHandler::OnTabletModeEnded() {
  if (IsIdle())
    return;

  if (mode_ == Mode::kSwipeHomeToOverview) {
    swipe_home_to_overview_controller_->CancelDrag();
    Reset();
  }
}

void HomeLauncherGestureHandler::Reset() {
  display_.set_id(display::kInvalidDisplayId);
  mode_ = Mode::kNone;
}

bool HomeLauncherGestureHandler::IsIdle() {
  return !IsDragInProgress();
}

}  // namespace ash
