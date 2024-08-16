// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_test_util.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

gfx::Rect GetOverviewGridBounds(aura::Window* root_window) {
  OverviewSession* overview_session = GetOverviewSession();
  return overview_session ? OverviewGridTestApi(root_window).bounds()
                          : gfx::Rect();
}

SplitViewController* GetSplitViewController() {
  return SplitViewController::Get(Shell::GetPrimaryRootWindow());
}

SplitViewDivider* GetSplitViewDivider() {
  return GetSplitViewController()->split_view_divider();
}

gfx::Rect GetSplitViewDividerBoundsInScreen() {
  return GetSplitViewDivider()->GetDividerBoundsInScreen(
      /*is_dragging=*/false);
}

const gfx::Rect GetWorkAreaBounds() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
}

const gfx::Rect GetWorkAreaBoundsForWindow(aura::Window* window) {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(window)
      .work_area();
}

void SnapOneTestWindow(aura::Window* window,
                       chromeos::WindowStateType state_type,
                       float snap_ratio,
                       WindowSnapActionSource snap_action_source) {
  WindowState* window_state = WindowState::Get(window);
  const WindowSnapWMEvent snap_event(
      state_type == chromeos::WindowStateType::kPrimarySnapped
          ? WM_EVENT_SNAP_PRIMARY
          : WM_EVENT_SNAP_SECONDARY,
      snap_ratio, snap_action_source);
  window_state->OnWMEvent(&snap_event);
  EXPECT_EQ(state_type, window_state->GetStateType());
}

void VerifySplitViewOverviewSession(aura::Window* window) {
  auto* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(
      overview_controller->overview_session()->IsWindowInOverview(window));

  auto* root_window = window->GetRootWindow();
  auto* split_view_controller = SplitViewController::Get(root_window);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_TRUE(split_view_controller->IsWindowInSplitView(window));

  EXPECT_TRUE(GetSplitViewOverviewSession(window));

  gfx::Rect expected_grid_bounds = GetWorkAreaBoundsForWindow(window);
  expected_grid_bounds.Subtract(window->GetBoundsInScreen());

  if (auto* divider = GetSplitViewDivider();
      divider && divider->divider_widget()) {
    expected_grid_bounds.Subtract(GetSplitViewDividerBoundsInScreen());
  }

  // Clamp the length on the side that can be shrunk by resizing to avoid going
  // below the threshold i.e. `kOverviewGridClampThresholdRatio` of the
  // corresponding work area length.
  const bool is_horizontal = IsLayoutHorizontal(Shell::GetPrimaryRootWindow());
  const int min_length = (is_horizontal ? GetWorkAreaBounds().width()
                                        : GetWorkAreaBounds().height()) *
                         kOverviewGridClampThresholdRatio;
  if (is_horizontal) {
    expected_grid_bounds.set_width(
        std::max(expected_grid_bounds.width(), min_length));
  } else {
    expected_grid_bounds.set_height(
        std::max(expected_grid_bounds.height(), min_length));
  }

  if (!Shell::Get()->IsInTabletMode()) {
    EXPECT_EQ(expected_grid_bounds, GetOverviewGridBounds(root_window));
  }

  EXPECT_TRUE(
      expected_grid_bounds.Contains(GetOverviewGridBounds(root_window)));

  if (!Shell::Get()->IsInTabletMode()) {
    auto* overview_grid = GetOverviewGridForRoot(window->GetRootWindow());
    EXPECT_TRUE(overview_grid->split_view_setup_widget());
    EXPECT_FALSE(overview_grid->no_windows_widget());
    // TODO(b/345814268): Consider destroying the widgets.
    const auto* save_desk_widget =
        overview_grid->save_desk_button_container_widget();
    EXPECT_FALSE(save_desk_widget && save_desk_widget->IsVisible());
    const auto* desks_bar_widget = overview_grid->desks_widget();
    EXPECT_FALSE(desks_bar_widget && desks_bar_widget->IsVisible());
  }
}

void VerifyNotSplitViewOrOverviewSession(aura::Window* window) {
  EXPECT_FALSE(IsInOverviewSession());
  EXPECT_FALSE(
      SplitViewController::Get(window->GetRootWindow())->InSplitViewMode());
  EXPECT_FALSE(
      RootWindowController::ForWindow(window)->split_view_overview_session());
}

}  // namespace ash
