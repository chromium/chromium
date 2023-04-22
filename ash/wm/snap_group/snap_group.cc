// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/window_positioning_utils.h"
#include "base/check.h"
#include "base/check_op.h"
#include "chromeos/ui/base/display_util.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

}  // namespace

SnapGroup::SnapGroup(aura::Window* window1, aura::Window* window2)
    : window1_(window1), window2_(window2) {
  auto* split_view_controller =
      SplitViewController::Get(window1_->GetRootWindow());
  CHECK_EQ(split_view_controller->state(),
           SplitViewController::State::kBothSnapped);

  StartObservingWindows();
}

SnapGroup::~SnapGroup() {
  StopObservingWindows();
}

void SnapGroup::OnWindowDestroying(aura::Window* window) {
  if (window != window1_ && window != window2_) {
    return;
  }

  // `this` will be destroyed after this line.
  Shell::Get()->snap_group_controller()->RemoveSnapGroup(this);
}

void SnapGroup::StartObservingWindows() {
  CHECK(window1_);
  CHECK(window2_);
  window1_->AddObserver(this);
  window2_->AddObserver(this);
}

void SnapGroup::StopObservingWindows() {
  if (window1_) {
    window1_->RemoveObserver(this);
    window1_ = nullptr;
  }

  if (window2_) {
    window2_->RemoveObserver(this);
    window2_ = nullptr;
  }
}

void SnapGroup::RestoreWindowsBoundsOnSnapGroupRemoved() {
  const display::Display& display1 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window1_);
  const display::Display& display2 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window2_);

  // TODO(michelefan@): Add multi-display support for snap group.
  DCHECK_EQ(display1, display2);

  gfx::Rect primary_window_bounds = window1_->GetBoundsInScreen();
  const int primary_x = primary_window_bounds.x();
  const int primary_y = primary_window_bounds.y();
  const int primary_width = primary_window_bounds.width();
  const int primary_height = primary_window_bounds.height();

  gfx::Rect secondary_window_bounds = window2_->GetBoundsInScreen();
  const int secondary_x = secondary_window_bounds.x();
  const int secondary_y = secondary_window_bounds.y();
  const int secondary_width = secondary_window_bounds.width();
  const int secondary_height = secondary_window_bounds.height();

  const int expand_delta = kSplitviewDividerShortSideLength / 2;

  if (chromeos::IsLandscapeOrientation(GetSnapDisplayOrientation(display1))) {
    primary_window_bounds.SetRect(primary_x, primary_y,
                                  primary_width + expand_delta, primary_height);
    secondary_window_bounds.SetRect(secondary_x - expand_delta, secondary_y,
                                    secondary_width + expand_delta,
                                    secondary_height);
  } else {
    primary_window_bounds.SetRect(primary_x, primary_y, primary_width,
                                  primary_height + expand_delta);
    secondary_window_bounds.SetRect(secondary_x, secondary_y - expand_delta,
                                    secondary_width,
                                    secondary_height + expand_delta);
  }

  const SetBoundsWMEvent window1_event(primary_window_bounds, /*animate=*/true);
  WindowState::Get(window1_)->OnWMEvent(&window1_event);
  const SetBoundsWMEvent window2_event(secondary_window_bounds,
                                       /*animate=*/true);
  WindowState::Get(window2_)->OnWMEvent(&window2_event);
}

}  // namespace ash
