// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_cue.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/frame/multitask_menu/split_button_view.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/display/display_switches.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The vertical distance used to end drag to show and start drag to hide.
constexpr int kMenuDragPoint = 100;

}  // namespace

class TabletModeMultitaskMenuEventHandlerTest : public AshTestBase {
 public:
  TabletModeMultitaskMenuEventHandlerTest()
      : scoped_feature_list_(chromeos::wm::features::kWindowLayoutMenu) {}
  TabletModeMultitaskMenuEventHandlerTest(
      const TabletModeMultitaskMenuEventHandlerTest&) = delete;
  TabletModeMultitaskMenuEventHandlerTest& operator=(
      const TabletModeMultitaskMenuEventHandlerTest&) = delete;
  ~TabletModeMultitaskMenuEventHandlerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // This allows us to snap to the bottom in portrait mode.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kUseFirstDisplayAsInternal);

    AshTestBase::SetUp();

    TabletModeControllerTestApi().EnterTabletMode();
  }

  void GenerateScroll(int x, int start_y, int end_y) {
    GetEventGenerator()->GestureScrollSequence(
        gfx::Point(x, start_y), gfx::Point(x, end_y), base::Milliseconds(100),
        /*steps=*/3);
  }

  void ShowMultitaskMenu(const aura::Window& window) {
    GenerateScroll(/*x=*/window.bounds().CenterPoint().x(),
                   /*start_y=*/window.bounds().y() + 8,
                   /*end_y=*/window.bounds().y() + kMenuDragPoint);
  }

  void DismissMenu(TabletModeMultitaskMenu* multitask_menu) {
    GetEventGenerator()->GestureTapAt(
        GetMultitaskMenuView(multitask_menu)->bounds().bottom_center() +
        gfx::Vector2d(0, 10));
    DCHECK(!GetMultitaskMenu());
  }

  void PressHalfButton(const aura::Window* window, bool left) {
    ShowMultitaskMenu(*window);
    auto* multitask_menu_view = GetMultitaskMenuView(GetMultitaskMenu());
    const gfx::Rect half_bounds =
        multitask_menu_view->half_button_for_testing()->GetBoundsInScreen();
    GetEventGenerator()->GestureTapAt(left ? half_bounds.left_center()
                                           : half_bounds.right_center());
    auto* split_view_controller = SplitViewController::Get(window);
    DCHECK_EQ(split_view_controller->GetPositionOfSnappedWindow(window),
              left ? SplitViewController::SnapPosition::kPrimary
                   : SplitViewController::SnapPosition::kSecondary);
  }

  void PressPartialPrimary(const aura::Window& window) {
    ShowMultitaskMenu(window);
    DCHECK(GetMultitaskMenu());
    GetEventGenerator()->GestureTapAt(GetMultitaskMenuView(GetMultitaskMenu())
                                          ->partial_button()
                                          ->GetBoundsInScreen()
                                          .left_center());
  }

  void PressPartialSecondary(const aura::Window& window) {
    ShowMultitaskMenu(window);
    DCHECK(GetMultitaskMenu());
    GetEventGenerator()->GestureTapAt(GetMultitaskMenuView(GetMultitaskMenu())
                                          ->partial_button()
                                          ->GetRightBottomButton()
                                          ->GetBoundsInScreen()
                                          .CenterPoint());
  }

  TabletModeMultitaskMenuEventHandler* GetMultitaskMenuEventHandler() {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_menu_event_handler();
  }

  TabletModeMultitaskMenu* GetMultitaskMenu() {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_menu_event_handler()
        ->multitask_menu();
  }

  chromeos::MultitaskMenuView* GetMultitaskMenuView(
      TabletModeMultitaskMenu* multitask_menu) const {
    return multitask_menu->GetMultitaskMenuViewForTesting();
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a scroll down gesture from the top center activates the
// multitask menu.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, BasicShowMenu) {
  auto window = CreateAppWindow();

  ShowMultitaskMenu(*window);

  TabletModeMultitaskMenu* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  ASSERT_TRUE(multitask_menu->widget()->GetContentsView()->GetVisible());

  // Tests that a regular window that can be snapped and floated has all
  // buttons.
  chromeos::MultitaskMenuView* multitask_menu_view =
      GetMultitaskMenuView(multitask_menu);
  ASSERT_TRUE(multitask_menu_view);
  EXPECT_TRUE(multitask_menu_view->half_button_for_testing());
  EXPECT_TRUE(multitask_menu_view->partial_button());
  EXPECT_TRUE(multitask_menu_view->full_button_for_testing());
  EXPECT_TRUE(multitask_menu_view->float_button_for_testing());

  // Verify that the menu is horizontally centered.
  EXPECT_EQ(multitask_menu->widget()
                ->GetContentsView()
                ->GetBoundsInScreen()
                .CenterPoint()
                .x(),
            window->GetBoundsInScreen().CenterPoint().x());
}

TEST_F(TabletModeMultitaskMenuEventHandlerTest, SwipeDownTargetArea) {
  auto window = CreateTestWindow(gfx::Rect(800, 600));

  // Scroll down from the top left. Verify no menu.
  GenerateScroll(0, 1, kMenuDragPoint);
  ASSERT_FALSE(GetMultitaskMenu());

  // Scroll down from the top right. Verify no menu.
  GenerateScroll(window->bounds().right(), 1, kMenuDragPoint);
  ASSERT_FALSE(GetMultitaskMenu());

  // Swipe up in the target area. Verify no menu.
  GenerateScroll(window->bounds().CenterPoint().x(), kMenuDragPoint, 8);
  ASSERT_FALSE(GetMultitaskMenu());

  // Start swipe down from the top of the target area.
  GenerateScroll(window->bounds().CenterPoint().x(), 0, kMenuDragPoint);
  ASSERT_TRUE(GetMultitaskMenu());
  DismissMenu(GetMultitaskMenu());

  // Start swipe down from the bottom of the target area.
  // TODO(sophiewen): Replace this with `kHitRegionSize.height()`.
  GenerateScroll(window->bounds().CenterPoint().x(), 15, kMenuDragPoint);
  ASSERT_TRUE(GetMultitaskMenu());
  DismissMenu(GetMultitaskMenu());

  // End swipe down outside the menu.
  GenerateScroll(window->bounds().CenterPoint().x(), 0, 300);
  ASSERT_TRUE(GetMultitaskMenu());

  // Swipe up outside the menu. Verify we close the menu.
  GenerateScroll(window->bounds().CenterPoint().x(), 300, 200);
  ASSERT_FALSE(GetMultitaskMenu());
}

// Tests that a slight touch moved in the menu will trigger a button press.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, PressMoveAndReleaseTouch) {
  auto window = CreateAppWindow(gfx::Rect(800, 600));
  ShowMultitaskMenu(*window);

  // Press and move the touch slightly to mimic a real tap.
  auto* half_button =
      GetMultitaskMenuView(GetMultitaskMenu())->half_button_for_testing();
  GetEventGenerator()->set_current_screen_location(
      half_button->GetBoundsInScreen().left_center());
  GetEventGenerator()->PressMoveAndReleaseTouchBy(0, 3);

  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(window.get())->GetStateType());
}

TEST_F(TabletModeMultitaskMenuEventHandlerTest, SwipeDownInSplitView) {
  auto window1 = CreateTestWindow(gfx::Rect(800, 600));
  PressHalfButton(window1.get(), /*left=*/true);
  auto window2 = CreateTestWindow(gfx::Rect(800, 600));
  PressHalfButton(window2.get(), /*left=*/false);

  // Swipe down on the left window. Test that the menu is shown.
  wm::ActivateWindow(window1.get());
  gfx::Rect left_bounds(window1->bounds());
  GenerateScroll(left_bounds.CenterPoint().x(), 0, 160);
  auto* multitask_menu_view = GetMultitaskMenuView(GetMultitaskMenu());
  ASSERT_TRUE(multitask_menu_view);
  ASSERT_TRUE(left_bounds.Contains(multitask_menu_view->GetBoundsInScreen()));

  // Swipe down on the right window. Test that it shows the menu.
  gfx::Rect right_bounds(window2->bounds());
  GenerateScroll(right_bounds.CenterPoint().x(), 0, 150);
  multitask_menu_view = GetMultitaskMenuView(GetMultitaskMenu());
  ASSERT_TRUE(multitask_menu_view);
  ASSERT_TRUE(right_bounds.Contains(multitask_menu_view->GetBoundsInScreen()));
}

// Tests no crash when swiping down another window during menu animation.
// http://b/276792842.
TEST_F(TabletModeMultitaskMenuEventHandlerTest,
       SwipeDownInSplitViewWhileAnimating) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create a larger display so the menu is within the window bounds when split.
  UpdateDisplay("1600x1000");

  auto window1 = CreateTestWindow(gfx::Rect(800, 600));
  PressHalfButton(window1.get(), /*left=*/true);
  auto window2 = CreateTestWindow(gfx::Rect(800, 600));
  PressHalfButton(window2.get(), /*left=*/false);

  // Start swipe down on the left window, then swap to the right window. Swipe
  // distance must be less than the menu height to start an animation.
  gfx::Rect left_bounds(window1->bounds());
  GenerateScroll(left_bounds.CenterPoint().x(), 0,
                 /*end_y=*/kMenuDragPoint);
  gfx::Rect right_bounds(window2->bounds());
  GenerateScroll(right_bounds.CenterPoint().x(), 0,
                 /*end_y=*/kMenuDragPoint);
  EXPECT_TRUE(window2->GetBoundsInScreen().Contains(
      GetMultitaskMenu()->widget()->GetWindowBoundsInScreen()));

  // Test that swipe down both windows at the same time doesn't crash.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressTouchId(0, gfx::Point(left_bounds.CenterPoint().x(), 0));
  generator->PressTouchId(1, gfx::Point(right_bounds.CenterPoint().x(), 0));
  generator->MoveTouchId(gfx::Point(0, kMenuDragPoint), 0);
  generator->MoveTouchId(gfx::Point(0, kMenuDragPoint), 1);
}

// Tests that the multitask menu cannot be shown while in pinned state.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, SwipeDownInPinnedWindow) {
  // Create and pin a window.
  std::unique_ptr<aura::Window> pinned_window = CreateAppWindow();
  wm::ActivateWindow(pinned_window.get());
  window_util::PinWindow(pinned_window.get(), /*trusted=*/true);

  GenerateScroll(pinned_window->bounds().CenterPoint().x(), 0, 150);
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that swipe down outside the menu doesn't crash. Test for b/266742428.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, SwipeDownMenuTwice) {
  auto window = CreateTestWindow(gfx::Rect(800, 600));

  // Scroll down to show the menu.
  GenerateScroll(window->bounds().CenterPoint().x(), 1, kMenuDragPoint);
  ASSERT_TRUE(GetMultitaskMenu());

  // Scroll down outside the menu. Verify that we close the menu.
  GenerateScroll(window->bounds().CenterPoint().x(), 400, 500);
  ASSERT_FALSE(GetMultitaskMenu());

  // Scroll down to show the menu again.
  GenerateScroll(window->bounds().CenterPoint().x(), 1, kMenuDragPoint);
  ASSERT_TRUE(GetMultitaskMenu());

  // Scroll down outside the menu again.
  GenerateScroll(window->bounds().CenterPoint().x(), 400, 500);
  ASSERT_FALSE(GetMultitaskMenu());

  histogram_tester_.ExpectBucketCount(
      chromeos::GetEntryTypeHistogramName(),
      chromeos::MultitaskMenuEntryType::kGestureScroll, 2);
}

// Tests no crash on multiple finger scrolls.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, MultiFingerSroll) {
  auto window = CreateTestWindow();
  const int center_x = window->bounds().CenterPoint().x();

  // Scroll down with 2 fingers.
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(center_x, 0),
      gfx::Point(center_x + 10, 0),
  };
  const int kSteps = 15;
  GetEventGenerator()->GestureMultiFingerScroll(kTouchPoints, points, 15,
                                                kSteps, 0, 150);
  EXPECT_TRUE(GetMultitaskMenu());
}

// Tests that a partial drag will show or hide the menu as expected.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, PartialDrag) {
  auto window = CreateTestWindow();
  // Scroll down less than half of the menu height. Tests that the menu does not
  // open.
  GenerateScroll(/*x=*/window->bounds().CenterPoint().x(), /*start_y=*/1,
                 /*end_y=*/20);
  ASSERT_FALSE(GetMultitaskMenu());

  // Scroll down more than half of the menu height. Tests that the menu opens.
  GenerateScroll(/*x=*/window->bounds().CenterPoint().x(), /*start_y=*/1,
                 /*end_y=*/100);
  ASSERT_TRUE(GetMultitaskMenu());
}

// Tests that the menu is closed when the window is closed or destroyed.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, OnWindowDestroying) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(*window);
  ASSERT_TRUE(GetMultitaskMenu());

  // Close the window.
  window.reset();
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that tap outside the menu will close the menu.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, CloseMultitaskMenuOnTap) {
  // Create a display and window that is bigger than the menu.
  UpdateDisplay("1600x1000");
  auto window = CreateAppWindow();

  ShowMultitaskMenu(*window);
  ASSERT_TRUE(GetMultitaskMenu());

  // Tap outside the menu. Verify that we close the menu.
  GetEventGenerator()->GestureTapAt(window->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(GetMultitaskMenu());
}

TEST_F(TabletModeMultitaskMenuEventHandlerTest, CloseOnDoubleTapDivider) {
  auto window1 = CreateTestWindow(gfx::Rect(800, 600));
  auto window2 = CreateTestWindow(gfx::Rect(800, 600));

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  // Open the menu on one of the windows.
  ShowMultitaskMenu(*window1);
  ASSERT_TRUE(GetMultitaskMenu());

  // Double tap on the divider center.
  const gfx::Point divider_center =
      split_view_controller->split_view_divider()
          ->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint();
  GetEventGenerator()->GestureTapAt(divider_center);
  GetEventGenerator()->GestureTapAt(divider_center);
  ASSERT_FALSE(GetMultitaskMenu());
}

TEST_F(TabletModeMultitaskMenuEventHandlerTest, HideMultitaskMenuInOverview) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(*window);

  auto* event_handler = TabletModeControllerTestApi()
                            .tablet_mode_window_manager()
                            ->tablet_mode_multitask_menu_event_handler();
  auto* multitask_menu = event_handler->multitask_menu();
  ASSERT_TRUE(multitask_menu);
  ASSERT_TRUE(multitask_menu->widget()->GetContentsView()->GetVisible());

  EnterOverview();

  // Verify that the menu is hidden.
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that the multitask menu gets updated after a button is pressed.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, HalfButtonFunctionality) {
  // Create a test window wide enough to show the menu even after it is split in
  // half.
  UpdateDisplay("1600x1000");
  auto window = CreateTestWindow();
  ShowMultitaskMenu(*window);

  // Press the primary half split button.
  auto* half_button =
      GetMultitaskMenuView(GetMultitaskMenu())->half_button_for_testing();
  GetEventGenerator()->GestureTapAt(
      half_button->GetBoundsInScreen().left_center());
  histogram_tester_.ExpectBucketCount(
      chromeos::GetActionTypeHistogramName(),
      chromeos::MultitaskMenuActionType::kHalfSplitButton, 1);

  // Verify that the window has been snapped in half.
  ASSERT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(window.get())->GetStateType());
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  EXPECT_EQ(work_area_bounds.width() * 0.5f,
            window->GetBoundsInScreen().width() +
                kSplitviewDividerShortSideLength / 2);

  // Verify that the multitask menu has been closed.
  ASSERT_FALSE(GetMultitaskMenu());

  // Verify that the multitask menu is centered on the new window size.
  ShowMultitaskMenu(*window);
  auto* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  EXPECT_EQ(window->GetBoundsInScreen().CenterPoint().x(),
            GetMultitaskMenuView(multitask_menu)
                ->GetBoundsInScreen()
                .CenterPoint()
                .x());
}

TEST_F(TabletModeMultitaskMenuEventHandlerTest, PartialButtonFunctionality) {
  auto window = CreateTestWindow();

  // Test that primary button snaps to 0.67f screen ratio.
  PressPartialPrimary(*window);
  ASSERT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(window.get())->GetStateType());
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const int divider_delta = kSplitviewDividerShortSideLength / 2;
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio),
            window->bounds().width() + divider_delta);
  ASSERT_FALSE(GetMultitaskMenu());
  histogram_tester_.ExpectBucketCount(
      chromeos::GetActionTypeHistogramName(),
      chromeos::MultitaskMenuActionType::kPartialSplitButton, 1);

  // Test that secondary button snaps to 0.33f screen ratio.
  PressPartialSecondary(*window);
  ASSERT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(window.get())->GetStateType());
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kOneThirdSnapRatio),
            window->bounds().width() + divider_delta);
  ASSERT_FALSE(GetMultitaskMenu());
  histogram_tester_.ExpectBucketCount(
      chromeos::GetActionTypeHistogramName(),
      chromeos::MultitaskMenuActionType::kPartialSplitButton, 2);
}

// Tests that the menu bounds are adjusted if the window is narrower than the
// menu for two partial split windows.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, AdjustedMenuBounds) {
  auto window1 = CreateTestWindow();
  PressPartialPrimary(*window1);
  auto window2 = CreateTestWindow();
  PressPartialSecondary(*window2);

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(split_view_controller->IsWindowInSplitView(window1.get()));
  ASSERT_TRUE(split_view_controller->IsWindowInSplitView(window2.get()));

  // Test that the menu fits on the 1/3 window on the right.
  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  EXPECT_EQ(std::round(work_area.width() * chromeos::kOneThirdSnapRatio),
            window2->bounds().width() + kSplitviewDividerShortSideLength / 2);
  ShowMultitaskMenu(*window2);
  ASSERT_TRUE(GetMultitaskMenu());
  EXPECT_EQ(work_area.right(),
            GetMultitaskMenu()->widget()->GetWindowBoundsInScreen().right());

  // Swap windows so the 1/3 window is on the left. Test that the menu fits.
  split_view_controller->SwapWindows(
      SplitViewController::SwapWindowsSource::kDoubleTap);
  ShowMultitaskMenu(*window2);
  EXPECT_EQ(work_area.x(),
            GetMultitaskMenu()->widget()->GetWindowBoundsInScreen().x());
}

// Tests that the split buttons are enabled/disabled based on min sizes.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, WindowMinimumSizes) {
  UpdateDisplay("800x600");
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, /*id=*/-1, gfx::Rect(800, 600)));
  wm::ActivateWindow(window.get());
  EXPECT_TRUE(WindowState::Get(window.get())->CanMaximize());

  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  // Set the min width to 0.4 of the work area. Since 1/3 < minWidth <= 1/2,
  // only the 1/3 option is disabled.
  delegate.set_minimum_size(
      gfx::Size(work_area_bounds.width() * 0.4f, work_area_bounds.height()));
  ShowMultitaskMenu(*window);
  ASSERT_TRUE(GetMultitaskMenu());
  chromeos::MultitaskMenuView* multitask_menu_view =
      GetMultitaskMenuView(GetMultitaskMenu());
  ASSERT_TRUE(multitask_menu_view->half_button_for_testing());
  ASSERT_TRUE(multitask_menu_view->partial_button()->GetEnabled());
  ASSERT_FALSE(multitask_menu_view->partial_button()
                   ->GetRightBottomButton()
                   ->GetEnabled());
  GetMultitaskMenu()->Reset();

  // Set the min width to 0.6 of the work area. Since 1/2 < minWidth <= 2/3, the
  // half button is hidden and only the 2/3 option is enabled.
  delegate.set_minimum_size(
      gfx::Size(work_area_bounds.width() * 0.6f, work_area_bounds.height()));
  ShowMultitaskMenu(*window);
  multitask_menu_view = GetMultitaskMenuView(GetMultitaskMenu());
  EXPECT_FALSE(multitask_menu_view->half_button_for_testing());
  EXPECT_TRUE(multitask_menu_view->partial_button()->GetEnabled());
  ASSERT_FALSE(multitask_menu_view->partial_button()
                   ->GetRightBottomButton()
                   ->GetEnabled());
  GetMultitaskMenu()->Reset();

  // Set the min width to 0.7 of the work area. Since minWidth > 2/3, both the
  // split buttons are hidden.
  delegate.set_minimum_size(
      gfx::Size(work_area_bounds.width() * 0.7f, work_area_bounds.height()));
  ShowMultitaskMenu(*window);
  multitask_menu_view = GetMultitaskMenuView(GetMultitaskMenu());
  EXPECT_FALSE(multitask_menu_view->half_button_for_testing());
  EXPECT_FALSE(multitask_menu_view->partial_button());
}

// Tests that if a window cannot be snapped or floated, the buttons will not
// be shown.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, HiddenButtons) {
  UpdateDisplay("800x600");

  // A window with a minimum size of 600x600 will not be snappable or
  // floatable.
  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(700, 700)));
  window_delegate.set_minimum_size(gfx::Size(600, 600));
  wm::ActivateWindow(window.get());

  ShowMultitaskMenu(*window);

  // Tests that only one button, the fullscreen button shows up.
  TabletModeMultitaskMenu* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  chromeos::MultitaskMenuView* multitask_menu_view =
      GetMultitaskMenuView(multitask_menu);
  ASSERT_TRUE(multitask_menu_view);
  EXPECT_FALSE(multitask_menu_view->half_button_for_testing());
  EXPECT_FALSE(multitask_menu_view->partial_button());
  EXPECT_TRUE(multitask_menu_view->full_button_for_testing());
  EXPECT_FALSE(multitask_menu_view->float_button_for_testing());
}

// Tests that showing the menu will dismiss the visual cue (drag bar).
TEST_F(TabletModeMultitaskMenuEventHandlerTest, CueVisibleOnShowMenu) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto window = CreateAppWindow();

  auto* multitask_cue = GetMultitaskMenuEventHandler()->multitask_cue();
  ASSERT_TRUE(multitask_cue);
  EXPECT_TRUE(multitask_cue->cue_layer());

  // Wait for fade in to finish.
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(multitask_cue->cue_layer());

  // Cue should still be showing when the menu is activated.
  ShowMultitaskMenu(*window);
  ASSERT_TRUE(multitask_cue);
  EXPECT_TRUE(multitask_cue->cue_layer());

  multitask_cue->FireCueDismissTimerForTesting();

  // Wait for fade out to finish.
  animation_waiter.Wait(multitask_cue->cue_layer());
  ASSERT_TRUE(multitask_cue);
  EXPECT_FALSE(multitask_cue->cue_layer());
}

// Tests that the bottom window can open the multitask menu in portrait mode. In
// portrait primary view, the bottom window is on the right.
// ----------------------
// |   Top   |  Bottom  |
// ----------------------
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ShowBottomMenuPortraitPrimary) {
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  ASSERT_EQ(chromeos::OrientationType::kPortraitPrimary,
            test_api.GetCurrentOrientation());

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  std::unique_ptr<aura::Window> top_window(CreateAppWindow());
  std::unique_ptr<aura::Window> bottom_window(CreateAppWindow());
  split_view_controller->SnapWindow(
      top_window.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller->SnapWindow(
      bottom_window.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_FALSE(split_view_controller->IsPhysicalLeftOrTop(
      SplitViewController::SnapPosition::kSecondary, bottom_window.get()));
  wm::ActivateWindow(bottom_window.get());

  // Event generation coordinates are relative to the natural origin, but
  // `window` bounds are relative to the portrait origin. Scroll from the
  // divider toward the right to open the menu.
  const gfx::Rect bounds(bottom_window->GetBoundsInScreen());
  const gfx::Point start(bounds.y() + 8, bounds.CenterPoint().x());
  const gfx::Point end(bounds.y() + kMenuDragPoint, bounds.CenterPoint().x());
  GetEventGenerator()->GestureScrollSequence(start, end,
                                             base::Milliseconds(100),
                                             /*steps=*/3);
  auto* multitask_menu_view = GetMultitaskMenuView(GetMultitaskMenu());
  ASSERT_TRUE(multitask_menu_view);
  ASSERT_TRUE(bounds.Contains(multitask_menu_view->GetBoundsInScreen()));
}

// Tests that the bottom window can open the multitask menu in portrait mode. In
// portrait secondary view, the bottom window is on the left.
// ----------------------
// |  Bottom  |   Top   |
// ----------------------
// TODO(b/270175923): Temporarily disabled for decreased target area.
TEST_F(TabletModeMultitaskMenuEventHandlerTest,
       DISABLED_ShowBottomMenuPortraitSecondary) {
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  ASSERT_EQ(chromeos::OrientationType::kPortraitSecondary,
            test_api.GetCurrentOrientation());

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  std::unique_ptr<aura::Window> bottom_window(CreateAppWindow());
  std::unique_ptr<aura::Window> top_window(CreateAppWindow());
  split_view_controller->SnapWindow(
      bottom_window.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller->SnapWindow(
      top_window.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_FALSE(split_view_controller->IsPhysicalLeftOrTop(
      SplitViewController::SnapPosition::kPrimary, bottom_window.get()));
  wm::ActivateWindow(bottom_window.get());

  // Event generation coordinates are relative to the natural origin, but
  // `window` bounds are relative to the portrait origin. Scroll from the
  // divider toward the left to open the menu.
  const gfx::Rect bounds(bottom_window->bounds());
  const gfx::Point start(bounds.y() + 8, bounds.CenterPoint().x());
  const gfx::Point end(bounds.y() - kMenuDragPoint, bounds.CenterPoint().x());
  GetEventGenerator()->GestureScrollSequence(start, end,
                                             base::Milliseconds(100),
                                             /*steps=*/3);
  auto* multitask_menu_view = GetMultitaskMenuView(GetMultitaskMenu());
  ASSERT_TRUE(multitask_menu_view);
  ASSERT_TRUE(bounds.Contains(multitask_menu_view->GetBoundsInScreen()));
}

// Tests that when we exit tablet mode with the multitask menu open, there is no
// crash. Regression test for b/273835755.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, NoCrashWhenExitingTabletMode) {
  // We need to use a non zero duration otherwise the fade out animation will
  // complete immediately and destroy the multitask menu before the tablet mode
  // window manager gets destroyed, which is not what happens on a real device.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto window = CreateAppWindow();
  ShowMultitaskMenu(*window);
  TabletModeControllerTestApi().LeaveTabletMode();
}

TEST_F(TabletModeMultitaskMenuEventHandlerTest, HidesWhenMinimized) {
  auto window = CreateAppWindow();
  ShowMultitaskMenu(*window);

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      WINDOW_MINIMIZE, {});
  ASSERT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_FALSE(GetMultitaskMenu());
}

}  // namespace ash
