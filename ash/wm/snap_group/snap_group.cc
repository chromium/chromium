// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_op.h"
#include "chromeos/ui/base/display_util.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

}  // namespace

SnapGroup::SnapGroup(aura::Window* window1, aura::Window* window2) {
  auto* window_state1 = WindowState::Get(window1);
  auto* window_state2 = WindowState::Get(window2);
  CHECK(window_state1->IsSnapped() && window_state2->IsSnapped() &&
        window_state1->GetStateType() != window_state2->GetStateType());

  // Always assign `window1_` to the primary window and `window2_` to the
  // secondary window.
  if (window_state1->GetStateType() ==
      chromeos::WindowStateType::kPrimarySnapped) {
    window1_ = window1;
    window2_ = window2;
  } else {
    window1_ = window2;
    window2_ = window1;
  }

  auto* split_view_controller =
      SplitViewController::Get(window1_->GetRootWindow());
  CHECK_EQ(split_view_controller->state(),
           SplitViewController::State::kBothSnapped);

  StartObservingWindows();
}

SnapGroup::~SnapGroup() {
  StopObservingWindows();
}

aura::Window* SnapGroup::GetTopMostWindowInGroup() const {
  return window_util::IsStackedBelow(window1_, window2_) ? window2_ : window1_;
}

void SnapGroup::MinimizeWindows() {
  auto* window1_state = WindowState::Get(window1_);
  auto* window2_state = WindowState::Get(window2_);
  CHECK(!window1_state->IsMinimized() && !window2_state->IsMinimized());
  window1_state->Minimize();
  window2_state->Minimize();
}

void SnapGroup::SwapWindows() {
  base::AutoReset<bool> auto_reset(&is_swapping_, true);
  auto* window_state1 = WindowState::Get(window1_);
  auto* window_state2 = WindowState::Get(window2_);
  CHECK_EQ(chromeos::WindowStateType::kPrimarySnapped,
           window_state1->GetStateType());
  CHECK_EQ(chromeos::WindowStateType::kSecondarySnapped,
           window_state2->GetStateType());
  const WindowSnapWMEvent snap_secondary(
      WM_EVENT_SNAP_SECONDARY,
      window_state1->snap_ratio().value_or(chromeos::kDefaultSnapRatio),
      WindowSnapActionSource::kNotSpecified);
  const WindowSnapWMEvent snap_primary(
      WM_EVENT_SNAP_PRIMARY,
      window_state2->snap_ratio().value_or(chromeos::kDefaultSnapRatio),
      WindowSnapActionSource::kNotSpecified);
  window_state1->OnWMEvent(&snap_secondary);
  window_state2->OnWMEvent(&snap_primary);
  std::swap(window1_, window2_);
}

void SnapGroup::OnWindowDestroying(aura::Window* window) {
  if (window != window1_ && window != window2_) {
    return;
  }

  // `this` will be destroyed after this line.
  SnapGroupController::Get()->RemoveSnapGroup(this);
}

void SnapGroup::OnPreWindowStateTypeChange(WindowState* window_state,
                                           chromeos::WindowStateType old_type) {
  if (is_swapping_) {
    // The windows can be swapped without breaking the group.
    return;
  }
  if (chromeos::IsSnappedWindowStateType(old_type) &&
      window_state->IsMinimized()) {
    // The windows can be minimized without breaking the group.
    return;
  }
  // Destroys `this`. Note if a window is still snapped but to the opposite
  // side, it will break the group and SnapGroupController will start overview.
  // If the window was still snapped in the same position and simply changed
  // snap ratios, it would not send a state change and reach here.
  SnapGroupController::Get()->RemoveSnapGroup(this);
}

void SnapGroup::StartObservingWindows() {
  CHECK(window1_);
  CHECK(window2_);
  for (aura::Window* window : {window1_, window2_}) {
    window->AddObserver(this);
    WindowState::Get(window)->AddObserver(this);
  }
}

void SnapGroup::StopObservingWindows() {
  for (aura::Window* window : {window1_, window2_}) {
    if (window) {
      window->RemoveObserver(this);
      WindowState::Get(window)->RemoveObserver(this);
    }
  }
  window1_ = nullptr;
  window2_ = nullptr;
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
