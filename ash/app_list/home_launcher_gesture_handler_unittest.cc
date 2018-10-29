// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/home_launcher_gesture_handler.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using Mode = HomeLauncherGestureHandler::Mode;

class HomeLauncherGestureHandlerTest : public AshTestBase {
 public:
  HomeLauncherGestureHandlerTest() = default;
  ~HomeLauncherGestureHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  }

  // Create a test window and set the base transform to identity and
  // the base opacity to opaque for easier testing.
  virtual std::unique_ptr<aura::Window> CreateWindowForTesting() {
    std::unique_ptr<aura::Window> window = CreateTestWindow();

    window->SetTransform(gfx::Transform());
    window->layer()->SetOpacity(1.f);
    return window;
  }

  HomeLauncherGestureHandler* GetGestureHandler() {
    return Shell::Get()->app_list_controller()->home_launcher_gesture_handler();
  }

  void DoPress(Mode mode) {
    DCHECK_NE(mode, Mode::kNone);
    gfx::Point press_location;
    if (mode == Mode::kSlideUpToShow) {
      press_location = Shelf::ForWindow(Shell::GetPrimaryRootWindow())
                           ->GetIdealBounds()
                           .CenterPoint();
    }

    GetGestureHandler()->OnPressEvent(mode, press_location);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeLauncherGestureHandlerTest);
};

// Tests that the gesture handler will not have a window to act on if there are
// none in the mru list.
TEST_F(HomeLauncherGestureHandlerTest, NeedsOneWindowToShow) {
  DoPress(Mode::kSlideUpToShow);
  EXPECT_FALSE(GetGestureHandler()->window());

  auto window = CreateWindowForTesting();
  DoPress(Mode::kSlideUpToShow);
  EXPECT_TRUE(GetGestureHandler()->window());
}

// Tests that the gesture handler will not have a window to act on if there are
// none in the mru list, or if they are not minimized.
TEST_F(HomeLauncherGestureHandlerTest, NeedsOneMinimizedWindowToHide) {
  DoPress(Mode::kSlideDownToHide);
  EXPECT_FALSE(GetGestureHandler()->window());

  auto window = CreateWindowForTesting();
  DoPress(Mode::kSlideDownToHide);
  EXPECT_FALSE(GetGestureHandler()->window());

  wm::GetWindowState(window.get())->Minimize();
  DoPress(Mode::kSlideDownToHide);
  EXPECT_TRUE(GetGestureHandler()->window());
}

// Tests that if there are other visible windows behind the most recent one,
// they get hidden on press event so that the home launcher is visible.
TEST_F(HomeLauncherGestureHandlerTest, ShowWindowsAreHidden) {
  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window3 = CreateWindowForTesting();
  ASSERT_TRUE(window1->IsVisible());
  ASSERT_TRUE(window2->IsVisible());
  ASSERT_TRUE(window3->IsVisible());

  // Test that the most recently activated window is visible, but the others are
  // not.
  ::wm::ActivateWindow(window1.get());
  DoPress(Mode::kSlideUpToShow);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
}

TEST_F(HomeLauncherGestureHandlerTest, CancellingSlideUp) {
  UpdateDisplay("400x456");

  auto window = CreateWindowForTesting();
  ASSERT_TRUE(window->IsVisible());

  // Tests that when cancelling a scroll that was on the bottom half, the window
  // is still visible.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 1.f);
  GetGestureHandler()->Cancel();
  EXPECT_TRUE(window->IsVisible());

  // Tests that when cancelling a scroll that was on the top half, the window is
  // now invisible.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 100), 1.f);
  GetGestureHandler()->Cancel();
  EXPECT_FALSE(window->IsVisible());
}

// Tests that if we fling with enough velocity while sliding up, the launcher
// becomes visible even if the event is released below the halfway mark.
TEST_F(HomeLauncherGestureHandlerTest, FlingingSlideUp) {
  UpdateDisplay("400x456");

  auto window = CreateWindowForTesting();
  ASSERT_TRUE(window->IsVisible());

  // Tests that flinging down in this mode will keep the window visible.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 10.f);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300), nullptr);
  ASSERT_TRUE(window->IsVisible());

  // Tests that flinging up in this mode will hide the window and show the
  // home launcher.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), -10.f);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300), nullptr);
  EXPECT_FALSE(window->IsVisible());
}

// Tests that if we fling with enough velocity while sliding up, the launcher
// becomes visible even if the event is released below the halfway mark.
TEST_F(HomeLauncherGestureHandlerTest, FlingingSlideDown) {
  UpdateDisplay("400x456");

  auto window = CreateWindowForTesting();
  wm::GetWindowState(window.get())->Minimize();
  ASSERT_FALSE(window->IsVisible());

  // Tests that flinging up in this mode will not show the mru window.
  DoPress(Mode::kSlideDownToHide);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 100), -10.f);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100), nullptr);
  ASSERT_FALSE(window->IsVisible());

  // Tests that flinging down in this mode will show the mru window.
  DoPress(Mode::kSlideDownToHide);
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 100), 10.f);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100), nullptr);
  EXPECT_TRUE(window->IsVisible());
}

TEST_F(HomeLauncherGestureHandlerTest, SlidingBelowPressPoint) {
  UpdateDisplay("400x456");

  auto window = CreateWindowForTesting();
  ASSERT_TRUE(window->IsVisible());

  // Tests that the windows transform does not change when trying to slide below
  // the press event location.
  GetGestureHandler()->OnPressEvent(Mode::kSlideUpToShow, gfx::Point(0, 400));
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 420), 1.f);
  EXPECT_EQ(gfx::Transform(), window->transform());

  // Tests that OnReleaseEvent returns true when checking if the release point
  // is below the press point.
  bool released_below;
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 420), &released_below);
  EXPECT_TRUE(released_below);
}

// Tests that the home launcher gestures work with overview mode as expected.
TEST_F(HomeLauncherGestureHandlerTest, OverviewMode) {
  UpdateDisplay("400x456");

  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsMinimized());
  EXPECT_FALSE(wm::GetWindowState(window2.get())->IsMinimized());

  WindowSelectorController* controller =
      Shell::Get()->window_selector_controller();
  controller->ToggleOverview();
  const int window1_initial_translation =
      window1->transform().To2dTranslation().y();
  const int window2_initial_translation =
      window2->transform().To2dTranslation().y();
  DoPress(Mode::kSlideUpToShow);
  EXPECT_FALSE(GetGestureHandler()->window());

  // Tests that while scrolling the window transform changes.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 1.f);
  EXPECT_NE(window1_initial_translation,
            window1->transform().To2dTranslation().y());
  EXPECT_NE(window2_initial_translation,
            window2->transform().To2dTranslation().y());

  // Tests that after releasing at below the halfway point, we remain in
  // overview mode.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300), nullptr);
  EXPECT_TRUE(controller->IsSelecting());
  EXPECT_EQ(window1_initial_translation,
            window1->transform().To2dTranslation().y());
  EXPECT_EQ(window2_initial_translation,
            window2->transform().To2dTranslation().y());

  // Tests that after releasing on the bottom half, overview mode has been
  // exited, and the two windows have been minimized to show the home launcher.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100), nullptr);
  EXPECT_FALSE(controller->IsSelecting());
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMinimized());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsMinimized());
}

// Tests that HomeLauncherGestureHandler works as expected when one window is
// snapped, and overview mode is active on the other side.
TEST_F(HomeLauncherGestureHandlerTest, SplitviewOneSnappedWindow) {
  UpdateDisplay("400x456");

  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();

  // Snap one window and leave overview mode open with the other window.
  WindowSelectorController* window_selector_controller =
      Shell::Get()->window_selector_controller();
  window_selector_controller->ToggleOverview();
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  split_view_controller->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(window_selector_controller->IsSelecting());
  ASSERT_TRUE(split_view_controller->IsSplitViewModeActive());

  const int window2_initial_translation =
      window2->transform().To2dTranslation().y();
  DoPress(Mode::kSlideUpToShow);
  EXPECT_EQ(window1.get(), GetGestureHandler()->window());

  // Tests that while scrolling the window transforms change.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 1.f);
  EXPECT_NE(window1->transform(), gfx::Transform());
  EXPECT_NE(window2_initial_translation,
            window2->transform().To2dTranslation().y());

  // Tests that after releasing at below the halfway point, we remain in
  // both splitview and overview mode.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300), nullptr);
  EXPECT_EQ(window1->transform(), gfx::Transform());
  EXPECT_EQ(window2_initial_translation,
            window2->transform().To2dTranslation().y());
  EXPECT_TRUE(window_selector_controller->IsSelecting());
  EXPECT_TRUE(split_view_controller->IsSplitViewModeActive());

  // Tests that after releasing on the bottom half, overivew and splitview have
  // both been exited, and both windows are minimized to show the home launcher.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100), nullptr);
  EXPECT_FALSE(window_selector_controller->IsSelecting());
  EXPECT_FALSE(split_view_controller->IsSplitViewModeActive());
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMinimized());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsMinimized());
}

// Tests that swipe to close works as expected when there are two snapped
// windows.
TEST_F(HomeLauncherGestureHandlerTest, SplitviewTwoSnappedWindows) {
  UpdateDisplay("400x456");

  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();

  // Snap two windows to start.
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  split_view_controller->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller->SnapWindow(window2.get(), SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_controller->IsSplitViewModeActive());

  // Make |window1| the most recent used window. It should be the main window in
  // HomeLauncherGestureHandler.
  ::wm::ActivateWindow(window1.get());
  DoPress(Mode::kSlideUpToShow);
  EXPECT_EQ(window1.get(), GetGestureHandler()->window());
  EXPECT_EQ(window2.get(), GetGestureHandler()->window2());

  // Tests that while scrolling the window transforms change.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 1.f);
  EXPECT_NE(window1->transform(), gfx::Transform());
  EXPECT_NE(window2->transform(), gfx::Transform());

  // Tests that after releasing at below the halfway point, we remain in
  // splitview.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300), nullptr);
  EXPECT_EQ(window1->transform(), gfx::Transform());
  EXPECT_EQ(window2->transform(), gfx::Transform());
  EXPECT_TRUE(split_view_controller->IsSplitViewModeActive());

  // Tests that after releasing on the bottom half, splitview has been ended,
  // and the two windows have been minimized to show the home launcher.
  DoPress(Mode::kSlideUpToShow);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100), nullptr);
  EXPECT_FALSE(split_view_controller->IsSplitViewModeActive());
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMinimized());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsMinimized());
}

class HomeLauncherModeGestureHandlerTest
    : public HomeLauncherGestureHandlerTest,
      public testing::WithParamInterface<Mode> {
 public:
  HomeLauncherModeGestureHandlerTest() : mode_(GetParam()) {}
  virtual ~HomeLauncherModeGestureHandlerTest() = default;

  // HomeLauncherGestureHandlerTest:
  std::unique_ptr<aura::Window> CreateWindowForTesting() override {
    std::unique_ptr<aura::Window> window =
        HomeLauncherGestureHandlerTest::CreateWindowForTesting();
    if (mode_ == Mode::kSlideDownToHide)
      wm::GetWindowState(window.get())->Minimize();
    return window;
  }

 protected:
  Mode mode_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeLauncherModeGestureHandlerTest);
};

INSTANTIATE_TEST_CASE_P(,
                        HomeLauncherModeGestureHandlerTest,
                        testing::Values(Mode::kSlideDownToHide,
                                        Mode::kSlideUpToShow));

// Tests that the window transform and opacity changes as we scroll.
TEST_P(HomeLauncherModeGestureHandlerTest, TransformAndOpacityChangesOnScroll) {
  auto window = CreateWindowForTesting();

  DoPress(mode_);
  ASSERT_TRUE(GetGestureHandler()->window());

  // Test that on scrolling to a point on the top half of the work area, the
  // window's opacity is between 0 and 0.5 and its transform has changed.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 100), 1.f);
  const gfx::Transform top_half_transform = window->transform();
  EXPECT_NE(gfx::Transform(), top_half_transform);
  EXPECT_GT(window->layer()->opacity(), 0.f);
  EXPECT_LT(window->layer()->opacity(), 0.5f);

  // Test that on scrolling to a point on the bottom half of the work area, the
  // window's opacity is between 0.5 and 1 and its transform has changed.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 1.f);
  EXPECT_NE(gfx::Transform(), window->transform());
  EXPECT_NE(gfx::Transform(), top_half_transform);
  EXPECT_GT(window->layer()->opacity(), 0.5f);
  EXPECT_LT(window->layer()->opacity(), 1.f);
}

// Tests that releasing a drag at the bottom of the work area will show the
// window.
TEST_P(HomeLauncherModeGestureHandlerTest, BelowHalfShowsWindow) {
  UpdateDisplay("400x400");
  auto window3 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();

  DoPress(mode_);
  ASSERT_TRUE(GetGestureHandler()->window());
  ASSERT_FALSE(window2->IsVisible());
  ASSERT_FALSE(window3->IsVisible());

  // After a scroll the transform and opacity are no longer the identity and 1.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300), 1.f);
  EXPECT_NE(gfx::Transform(), window1->transform());
  EXPECT_NE(1.f, window1->layer()->opacity());

  // Tests the transform and opacity have returned to the identity and 1.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300), nullptr);
  EXPECT_EQ(gfx::Transform(), window1->transform());
  EXPECT_EQ(1.f, window1->layer()->opacity());

  if (mode_ == Mode::kSlideDownToHide)
    return;

  // The other windows return to their original visibility if mode is swiping
  // up.
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
}

// Tests that a drag released at the top half of the work area will minimize the
// window under action.
TEST_P(HomeLauncherModeGestureHandlerTest, AboveHalfReleaseMinimizesWindow) {
  UpdateDisplay("400x400");
  auto window3 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();

  DoPress(mode_);
  ASSERT_TRUE(GetGestureHandler()->window());
  ASSERT_FALSE(window2->IsVisible());
  ASSERT_FALSE(window3->IsVisible());

  // Test that |window1| is minimized on release.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100), nullptr);
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMinimized());

  // The rest of the windows remain invisible, to show the home launcher.
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
}

// Tests on swipe up, the transient child of a window which is getting hidden
// will have its opacity and transform altered as well.
TEST_P(HomeLauncherModeGestureHandlerTest, WindowWithTransientChild) {
  UpdateDisplay("400x456");

  // Create a window with a transient child.
  auto parent = CreateWindowForTesting();
  auto child = CreateTestWindow(gfx::Rect(100, 100, 200, 200),
                                aura::client::WINDOW_TYPE_POPUP);
  child->SetTransform(gfx::Transform());
  child->layer()->SetOpacity(1.f);
  ::wm::AddTransientChild(parent.get(), child.get());

  // |parent| should be the window that is getting hidden.
  DoPress(mode_);
  ASSERT_EQ(parent.get(), GetGestureHandler()->window());

  // Tests that after scrolling to the halfway point, the transient child's
  // opacity and transform are halfway to their final values.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 200), 1.f);
  EXPECT_LE(0.45f, child->layer()->opacity());
  EXPECT_GE(0.55f, child->layer()->opacity());
  EXPECT_NE(gfx::Transform(), child->transform());

  // Tests that after releasing on the bottom half, the transient child reverts
  // to its original values.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300), nullptr);
  EXPECT_EQ(1.0f, child->layer()->opacity());
  EXPECT_EQ(gfx::Transform(), child->transform());
}

// Tests that when tablet mode is ended while in the middle of a scroll session,
// the window is advanced to its end state.
TEST_P(HomeLauncherModeGestureHandlerTest, EndScrollOnTabletModeEnd) {
  auto window = CreateWindowForTesting();

  DoPress(mode_);
  ASSERT_TRUE(GetGestureHandler()->window());

  // Scroll to a point above the halfway mark of the work area.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 50), 1.f);
  EXPECT_TRUE(GetGestureHandler()->window());
  EXPECT_FALSE(wm::GetWindowState(window.get())->IsMinimized());

  // Tests that on exiting tablet mode, |window| gets minimized and is no longer
  // tracked by the gesture handler.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_FALSE(GetGestureHandler()->window());
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMinimized());
}

// Tests that the variables get set as expected during dragging, and get reset
// after finishing a drag.
TEST_P(HomeLauncherModeGestureHandlerTest, AnimatingToEndResetsState) {
  // Create a window with a transient child to test that case.
  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto child = CreateTestWindow(gfx::Rect(100, 100, 200, 200),
                                aura::client::WINDOW_TYPE_POPUP);
  ::wm::AddTransientChild(window1.get(), child.get());
  ::wm::ActivateWindow(window1.get());

  // For swipe down to hide launcher, all windows must be minimized.
  if (mode_ == Mode::kSlideDownToHide) {
    wm::GetWindowState(window2.get())->Minimize();
    wm::GetWindowState(window1.get())->Minimize();
  }

  // Tests that the variables which change when dragging are as expected.
  DoPress(mode_);
  EXPECT_EQ(window1.get(), GetGestureHandler()->window());
  EXPECT_TRUE(GetGestureHandler()->last_event_location_);
  EXPECT_EQ(mode_, GetGestureHandler()->mode_);
  // We only need to hide windows when swiping up, so this will only be non
  // empty in that case.
  if (mode_ == Mode::kSlideUpToShow)
    EXPECT_FALSE(GetGestureHandler()->hidden_windows_.empty());
  EXPECT_FALSE(GetGestureHandler()->transient_descendants_values_.empty());

  // Tests that after a drag, the variables are either null or empty.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(10, 10), nullptr);
  EXPECT_FALSE(GetGestureHandler()->window());
  EXPECT_FALSE(GetGestureHandler()->last_event_location_);
  EXPECT_EQ(Mode::kNone, GetGestureHandler()->mode_);
  EXPECT_TRUE(GetGestureHandler()->hidden_windows_.empty());
  EXPECT_TRUE(GetGestureHandler()->transient_descendants_values_.empty());
}

}  // namespace ash
