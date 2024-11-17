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
#include "base/test/scoped_feature_list.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class SplitViewMultiDisplayClamshellTest : public AshTestBase {
 public:
  SplitViewMultiDisplayClamshellTest() = default;
  SplitViewMultiDisplayClamshellTest(
      const SplitViewMultiDisplayClamshellTest&) = delete;
  SplitViewMultiDisplayClamshellTest& operator=(
      const SplitViewMultiDisplayClamshellTest&) = delete;
  ~SplitViewMultiDisplayClamshellTest() override = default;

  display::Display GetPrimaryDisplay() {
    return display::Screen::GetScreen()->GetPrimaryDisplay();
  }

  display::Display GetSecondaryDisplay() {
    return display::test::DisplayManagerTestApi(display_manager())
        .GetSecondaryDisplay();
  }

  gfx::Point GetDragPoint(aura::Window* window) {
    gfx::Point drag_point;
    if (auto* overview_session = OverviewController::Get()->overview_session();
        overview_session && overview_session->IsWindowInOverview(window)) {
      drag_point = gfx::ToRoundedPoint(GetOverviewItemForWindow(window)
                                           ->GetTransformedBounds()
                                           .CenterPoint());
    } else {
      drag_point = window->GetBoundsInScreen().top_center();
      drag_point.Offset(0, 10);
    }
    return drag_point;
  }

  // Returns the expected snapped bounds on `display`, where the first element
  // is the primary bounds and the second element is secondary bounds.
  std::pair<gfx::Rect, gfx::Rect> GetExpectedSnappedBounds(
      const display::Display& display) {
    const gfx::Rect work_area(display.work_area());
    gfx::Rect primary_bounds, secondary_bounds;
    if (IsLayoutHorizontal(display)) {
      work_area.SplitVertically(primary_bounds, secondary_bounds);
    } else {
      work_area.SplitHorizontally(primary_bounds, secondary_bounds);
    }
    return std::make_pair(primary_bounds, secondary_bounds);
  }
};

// Tests that using the shortcut to move the snapped window to another display
// works as intended.
TEST_F(SplitViewMultiDisplayClamshellTest, MoveWindowToDisplayShortcut) {
  UpdateDisplay("1200x900,800x600");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());

  std::unique_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 200, 200)));
  std::unique_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 200, 200)));

  // Also create `w3` and `w4` on each display so we can start partial overview.
  std::unique_ptr<aura::Window> w3(CreateTestWindow(gfx::Rect(0, 0, 200, 200)));
  std::unique_ptr<aura::Window> w4(
      CreateTestWindow(gfx::Rect(1200, 0, 200, 200)));

  auto* split_view_controller1 =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  auto* split_view_controller2 =
      SplitViewController::Get(Shell::GetRootWindowForDisplayId(
          display_manager_test.GetSecondaryDisplay().id()));
  auto* overview_controller = OverviewController::Get();

  // Test for both setups.
  enum class TestCase { kFasterSplitScreenSetup, kOverviewDragToSnap };
  const auto kTestCases = {TestCase::kFasterSplitScreenSetup,
                           TestCase::kOverviewDragToSnap};

  for (const auto kTestCase : kTestCases) {
    // 1 - Snap `w1` to primary on display 1.
    if (kTestCase == TestCase::kOverviewDragToSnap) {
      ToggleOverview();
      ASSERT_TRUE(overview_controller->InOverviewSession());
    }
    split_view_controller1->SnapWindow(
        w1.get(), SnapPosition::kPrimary,
        WindowSnapActionSource::kDragWindowToEdgeToSnap);

    // Expect we start splitview on root 1 but not root 2.
    EXPECT_TRUE(overview_controller->InOverviewSession());
    EXPECT_TRUE(split_view_controller1->InSplitViewMode());
    EXPECT_FALSE(split_view_controller2->InSplitViewMode());

    // Use the shortcut ALT+SEARCH+M to move `w1` to display 2.
    wm::ActivateWindow(w1.get());
    PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
    const gfx::Rect work_area2 =
        display_manager_test.GetSecondaryDisplay().work_area();
    EXPECT_EQ(gfx::Rect(work_area2.x(), 0, work_area2.width() / 2,
                        work_area2.height()),
              w1->GetBoundsInScreen());

    // Expect we ended overview and splitview.
    EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
    EXPECT_FALSE(split_view_controller1->InSplitViewMode());
    EXPECT_FALSE(split_view_controller2->InSplitViewMode());

    // 2 - Snap `w1` to secondary on display 2.
    if (kTestCase == TestCase::kOverviewDragToSnap) {
      ToggleOverview();
      ASSERT_TRUE(overview_controller->InOverviewSession());
    }
    split_view_controller2->SnapWindow(
        w1.get(), SnapPosition::kSecondary,
        WindowSnapActionSource::kDragWindowToEdgeToSnap);

    // Expect we start splitview on root 2 but not root 1.
    EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
    EXPECT_FALSE(split_view_controller1->InSplitViewMode());
    EXPECT_TRUE(split_view_controller2->InSplitViewMode());

    // Use the shortcut ALT+SEARCH+M to move `w1` to display 1.
    wm::ActivateWindow(w1.get());
    PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
    const gfx::Rect work_area1 =
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
    EXPECT_EQ(gfx::Rect(work_area1.width() / 2, 0, work_area1.width() / 2,
                        work_area1.height()),
              w1->GetBoundsInScreen());

    // Expect we ended overview and splitview.
    EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
    EXPECT_FALSE(split_view_controller2->InSplitViewMode());
    EXPECT_FALSE(split_view_controller1->InSplitViewMode());
  }
}

// Tests that snap across multi-displays works correctly. Pressing escape key
// afte each test to avoid Snap Group creation with `kSnapGroup` enabled.
// Regression test for b/331663949.
TEST_F(SplitViewMultiDisplayClamshellTest, SnapToCorrectDisplay) {
  UpdateDisplay("800x600,800x600");

  // Create 2 test windows with non-overlapping bounds so we can drag them.
  std::unique_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  std::unique_ptr<aura::Window> w2(
      CreateTestWindow(gfx::Rect(400, 0, 400, 400)));

  // Drag to snap `w1` to primary on display 1.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(0, 100);
  EXPECT_EQ(GetExpectedSnappedBounds(GetPrimaryDisplay()).first,
            w1->GetBoundsInScreen());
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  // Drag to snap `w2` to secondary on display 1.
  event_generator->set_current_screen_location(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(799, 100);
  EXPECT_EQ(GetExpectedSnappedBounds(GetPrimaryDisplay()).second,
            w2->GetBoundsInScreen());
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  // Drag to snap `w2` to primary on display 2.
  event_generator->set_current_screen_location(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(800, 100);
  EXPECT_EQ(GetExpectedSnappedBounds(GetSecondaryDisplay()).first,
            w2->GetBoundsInScreen());
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  // Drag to snap `w2` back to secondary on display 1.
  event_generator->set_current_screen_location(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(799, 100);
  EXPECT_EQ(GetExpectedSnappedBounds(GetPrimaryDisplay()).second,
            w2->GetBoundsInScreen());
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  // Drag to snap `w2` back to primary on display 2.
  event_generator->set_current_screen_location(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(800, 100);
  EXPECT_EQ(GetExpectedSnappedBounds(GetSecondaryDisplay()).first,
            w2->GetBoundsInScreen());
}

// Tests that drag to snap across different display sizes works correctly.
// Pressing escape key after each test to avoid Snap Group creation with
// `kSnapGroup` enabled.
TEST_F(SplitViewMultiDisplayClamshellTest, SnapDifferentDisplaySizes) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 200, 200)));
  std::unique_ptr<aura::Window> w2(
      CreateTestWindow(gfx::Rect(400, 0, 200, 200)));
  auto* event_generator = GetEventGenerator();

  const auto test_display_specs = {"800x600,1200x900", "1200x900,800x600",
                                   "1024x768,1920x1080"};

  for (const auto kTestDisplays : test_display_specs) {
    UpdateDisplay(kTestDisplays);
    SCOPED_TRACE(kTestDisplays);

    // Drag to snap `w1` to primary on display 1.
    wm::ActivateWindow(w1.get());
    event_generator->MoveMouseTo(GetDragPoint(w1.get()));
    const display::Display display1 = GetPrimaryDisplay();
    const gfx::Rect work_area1 = display1.work_area();
    event_generator->DragMouseTo(work_area1.origin());
    EXPECT_EQ(GetExpectedSnappedBounds(display1).first,
              w1->GetBoundsInScreen());
    PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);

    // Drag to snap `w2` to secondary on display 1.
    wm::ActivateWindow(w2.get());
    event_generator->MoveMouseTo(GetDragPoint(w2.get()));
    event_generator->DragMouseTo(work_area1.right() - 1, work_area1.y());
    EXPECT_EQ(GetExpectedSnappedBounds(display1).second,
              w2->GetBoundsInScreen());
    PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);

    // Drag to snap `w2` to primary on display 2.
    event_generator->set_current_screen_location(GetDragPoint(w2.get()));
    const display::Display display2 = GetSecondaryDisplay();
    const gfx::Rect work_area2 = display2.work_area();
    event_generator->DragMouseTo(work_area2.origin());
    EXPECT_EQ(GetExpectedSnappedBounds(display2).first,
              w2->GetBoundsInScreen());
    PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);

    // Drag to snap `w2` back to secondary on display 1.
    event_generator->set_current_screen_location(GetDragPoint(w2.get()));
    event_generator->DragMouseTo(work_area1.right() - 1, work_area1.y());
    EXPECT_EQ(GetExpectedSnappedBounds(display1).second,
              w2->GetBoundsInScreen());
    PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);

    // Drag to snap `w2` back to primary on display 2.
    event_generator->set_current_screen_location(GetDragPoint(w2.get()));
    event_generator->DragMouseTo(work_area2.origin());
    EXPECT_EQ(GetExpectedSnappedBounds(display2).first,
              w2->GetBoundsInScreen());
    PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);

    // Drag to snap `w1` to secondary on display 2.
    wm::ActivateWindow(w1.get());
    event_generator->MoveMouseTo(GetDragPoint(w1.get()));
    event_generator->DragMouseTo(work_area2.right() - 1, work_area2.y());
    EXPECT_EQ(GetExpectedSnappedBounds(display2).second,
              w1->GetBoundsInScreen());
    PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  }
}

// Tests that snapping across a landscape and portrait display works correctly.
TEST_F(SplitViewMultiDisplayClamshellTest, LandscapeAndPortrait) {
  UpdateDisplay("800x600,600x800");

  std::unique_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 200, 200)));
  std::unique_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 200, 200)));

  // Also create `w3` and `w4` on each display so we can start partial overview.
  std::unique_ptr<aura::Window> w3(CreateTestWindow(gfx::Rect(0, 0, 200, 200)));
  std::unique_ptr<aura::Window> w4(
      CreateTestWindow(gfx::Rect(800, 0, 200, 200)));

  auto* event_generator = GetEventGenerator();
  wm::ActivateWindow(w1.get());
  WindowState* window_state1 = WindowState::Get(w1.get());

  // Drag to snap `w1` to primary on display 1.
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(0, 100);
  const display::Display display1 = GetPrimaryDisplay();
  const gfx::Rect work_area1 = display1.work_area();
  const gfx::Rect primary_bounds1 = GetExpectedSnappedBounds(display1).first;
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state1->GetStateType());
  EXPECT_EQ(primary_bounds1, w1->GetBoundsInScreen());

  // Drag to snap `w1` to primary on display 2.
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  const display::Display display2 = GetSecondaryDisplay();
  const gfx::Rect work_area2 = display2.work_area();
  // We need to drag a vertical movement > kSnapTriggerVerticalMoveThreshold in
  // order to snap to top. See `WorkspaceWindowResizer::Drag()`.
  event_generator->DragMouseTo(work_area1.bottom_right());
  event_generator->DragMouseTo(work_area2.top_center());

  // Test the bounds are the top 1/2 of display 2.
  const gfx::Rect primary_bounds2 = GetExpectedSnappedBounds(display2).first;
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state1->GetStateType());
  EXPECT_EQ(primary_bounds2, w1->GetBoundsInScreen());

  // Drag to snap `w1` to secondary on display 1.
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(work_area1.right() - 1, work_area1.y());

  const gfx::Rect secondary_bounds1 = GetExpectedSnappedBounds(display1).second;
  EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_EQ(secondary_bounds1, w1->GetBoundsInScreen());

  // Drag to snap `w1` to secondary on display 2.
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(work_area2.bottom_center());

  // Test the bounds are the bottom 1/2 of display 2.
  const gfx::Rect secondary_bounds2 = GetExpectedSnappedBounds(display2).second;
  EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_EQ(secondary_bounds2, w1->GetBoundsInScreen());
}

}  // namespace ash
