// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using chromeos::WindowStateType;

SplitViewController* split_view_controller() {
  return SplitViewController::Get(Shell::GetPrimaryRootWindow());
}

}  // namespace

using SplitViewControllerClamshellTest = AshTestBase;

// Tests we correctly end split view if partial overview is skipped and another
// window is snapped. Regression test for http://b/333600706.
TEST_F(SplitViewControllerClamshellTest, EndSplitView) {
  // Snap `w1` to start partial overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  split_view_controller()->SnapWindow(
      w1.get(), SnapPosition::kPrimary,
      WindowSnapActionSource::kDragWindowToEdgeToSnap);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->primary_window());
  // Skip partial overview.
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  // Test we cleared the observed window.
  EXPECT_FALSE(split_view_controller()->primary_window());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Drag to snap `w2` to the opposite side.
  wm::ActivateWindow(w2.get());
  const gfx::Rect w2_bounds(w2->GetBoundsInScreen());
  const gfx::Point drag_point(w2_bounds.CenterPoint().x(), w2_bounds.y() + 10);
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(drag_point);
  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  event_generator->DragMouseTo(work_area.right_center());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  // Test we cleared the observed window.
  EXPECT_FALSE(split_view_controller()->secondary_window());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

// Test that tablet <-> clamshell transition works correctly.
TEST_F(SplitViewControllerClamshellTest, ClamshellTabletTransition) {
  // 1. Test 1 snapped window.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  split_view_controller()->SnapWindow(
      w1.get(), SnapPosition::kPrimary,
      WindowSnapActionSource::kDragWindowToEdgeToSnap);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(split_view_controller()->primary_window(), w1.get());
  EXPECT_FALSE(split_view_controller()->secondary_window());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Enter tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(split_view_controller()->primary_window(), w1.get());
  EXPECT_FALSE(split_view_controller()->secondary_window());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Exit tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(split_view_controller()->primary_window(), w1.get());
  EXPECT_FALSE(split_view_controller()->secondary_window());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // 2. Test 2 snapped windows.
  split_view_controller()->SnapWindow(
      w2.get(), SnapPosition::kSecondary,
      WindowSnapActionSource::kDragWindowToEdgeToSnap);

  // Since we are in clamshell mode, `primary|secondary_window_` are nil.
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->primary_window());
  EXPECT_FALSE(split_view_controller()->secondary_window());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // Enter tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(split_view_controller()->primary_window(), w1.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), w2.get());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Exit tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->primary_window());
  EXPECT_FALSE(split_view_controller()->secondary_window());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Tests that using the keyboard shortcut never starts split view.
TEST_F(SplitViewControllerClamshellTest, Shortcut) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  wm::ActivateWindow(w1.get());
  PressAndReleaseKey(ui::VKEY_OEM_4, ui::EF_ALT_DOWN);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  wm::ActivateWindow(w2.get());
  PressAndReleaseKey(ui::VKEY_OEM_6, ui::EF_ALT_DOWN);
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

}  // namespace ash
