// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include "ash/shell.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/window_positioning_utils.h"
#include "base/check.h"
#include "chromeos/ui/base/display_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

bool IsStackedBelow(aura::Window* win1, aura::Window* win2) {
  CHECK_NE(win1, win2);
  CHECK_EQ(win1->parent(), win2->parent());

  const auto& children = win1->parent()->children();
  auto win1_iter = base::ranges::find(children, win1);
  auto win2_iter = base::ranges::find(children, win2);
  CHECK(win1_iter != children.end());
  CHECK(win2_iter != children.end());
  return win1_iter < win2_iter;
}

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

void SnapGroup::OnWindowStackingChanged(aura::Window* window) {
  // Update the stacking order of the other window in the snap group so that the
  // two windows are always placed on top, both of which will be stacked below
  // the `split_view_divider` if applicable afterwards.
  aura::Window* top_window =
      IsStackedBelow(window1_, window2_) ? window2_ : window1_;
  aura::Window* target_window = top_window == window1_ ? window2_ : window1_;
  auto* parent_container = target_window->parent();
  parent_container->StackChildBelow(target_window, top_window);

    // Update the stacking order of the `split_view_divider` to be on top of the
    // `top_window` which makes the overall stacking order become
    // `divider_widget->GetNativeWindow()` --> `top_window` --> `target_window`.
    aura::Window* root_window = window1_->GetRootWindow();
    auto* split_view_controller = SplitViewController::Get(root_window);
    DCHECK(split_view_controller);
    auto* split_view_divider = split_view_controller->split_view_divider();
    DCHECK(split_view_divider);
    auto* divider_widget = split_view_divider->divider_widget();
    DCHECK(divider_widget);
    parent_container->StackChildAbove(divider_widget->GetNativeWindow(),
                                      top_window);
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