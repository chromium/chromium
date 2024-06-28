// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_test_util.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/test/test_non_client_frame_view_ash.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "testing/gtest/include/gtest/gtest.h"

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

SplitViewOverviewSession* VerifySplitViewOverviewSession(
    aura::Window* window,
    bool faster_split_screen_setup) {
  auto* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(
      overview_controller->overview_session()->IsWindowInOverview(window));

  SplitViewOverviewSession* split_view_overview_session =
      RootWindowController::ForWindow(window)->split_view_overview_session();
  EXPECT_TRUE(split_view_overview_session);
  gfx::Rect expected_grid_bounds = GetWorkAreaBoundsForWindow(window);
  expected_grid_bounds.Subtract(window->GetBoundsInScreen());

  if (GetSplitViewDivider() && GetSplitViewDivider()->divider_widget()) {
    expected_grid_bounds.Subtract(GetSplitViewDividerBoundsInScreen());
  }

  // Clamp the length on the side that can be shrunk by resizing to avoid going
  // below the threshold i.e. 1/3 of the corresponding work area length.
  // TODO(b/349902164): Get the const from overview instead of hardcoding 3.
  const bool is_horizontal = IsLayoutHorizontal(Shell::GetPrimaryRootWindow());
  const int min_length = (is_horizontal ? GetWorkAreaBounds().width()
                                        : GetWorkAreaBounds().height()) /
                         3;
  if (is_horizontal) {
    expected_grid_bounds.set_width(
        std::max(expected_grid_bounds.width(), min_length));
  } else {
    expected_grid_bounds.set_height(
        std::max(expected_grid_bounds.height(), min_length));
  }

  auto* root_window = window->GetRootWindow();
  if (!Shell::Get()->IsInTabletMode()) {
    EXPECT_EQ(expected_grid_bounds, GetOverviewGridBounds(root_window));
  }

  EXPECT_TRUE(
      expected_grid_bounds.Contains(GetOverviewGridBounds(root_window)));

  if (!Shell::Get()->IsInTabletMode() && faster_split_screen_setup) {
    auto* overview_grid = GetOverviewGridForRoot(window->GetRootWindow());
    EXPECT_TRUE(overview_grid->faster_splitview_widget());
    EXPECT_FALSE(overview_grid->no_windows_widget());
    EXPECT_FALSE(overview_grid->GetSaveDeskButtonContainer());
    EXPECT_FALSE(overview_grid->desks_bar_view());
  }

  return split_view_overview_session;
}

void VerifyNotSplitViewOverviewSession(aura::Window* window) {
  EXPECT_FALSE(IsInOverviewSession());
  EXPECT_FALSE(
      SplitViewController::Get(window->GetRootWindow())->InSplitViewMode());
  EXPECT_FALSE(
      RootWindowController::ForWindow(window)->split_view_overview_session());
}

}  // namespace ash
