// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_cue_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chromeos/ui/frame/multitask_menu/multitask_button.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view_test_api.h"
#include "chromeos/ui/frame/multitask_menu/split_button_view.h"
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

// The vertical position of the multitask menu, in the window (and widget)
// coordinates.
constexpr int kVerticalPosition = 8;

}  // namespace

class TabletModeMultitaskMenuTest : public AshTestBase {
 public:
  TabletModeMultitaskMenuTest() = default;
  TabletModeMultitaskMenuTest(const TabletModeMultitaskMenuTest&) = delete;
  TabletModeMultitaskMenuTest& operator=(const TabletModeMultitaskMenuTest&) =
      delete;
  ~TabletModeMultitaskMenuTest() override = default;

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
        multitask_menu->widget()->GetWindowBoundsInScreen().bottom_center() +
        gfx::Vector2d(0, 10));

    DCHECK(!GetMultitaskMenu());
  }

  void PressHalfButton(const aura::Window* window, bool left) {
    ShowMultitaskMenu(*window);
    auto* multitask_menu_view = GetMultitaskMenuView(GetMultitaskMenu());
    const gfx::Rect half_bounds =
        chromeos::MultitaskMenuViewTestApi(multitask_menu_view)
            .GetHalfButton()
            ->GetBoundsInScreen();
    GetEventGenerator()->GestureTapAt(left ? half_bounds.left_center()
                                           : half_bounds.right_center());
    auto* split_view_controller = SplitViewController::Get(window);
    DCHECK_EQ(split_view_controller->GetPositionOfSnappedWindow(window),
              left ? SnapPosition::kPrimary : SnapPosition::kSecondary);
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

  TabletModeMultitaskMenuController* GetMultitaskMenuController() {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_menu_controller();
  }

  TabletModeMultitaskMenu* GetMultitaskMenu() {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_menu_controller()
        ->multitask_menu();
  }

  chromeos::MultitaskMenuView* GetMultitaskMenuView(
      TabletModeMultitaskMenu* multitask_menu) const {
    return multitask_menu->GetMultitaskMenuViewForTesting();
  }

 protected:
  base::HistogramTester histogram_tester_;
};

// Tests that a scroll down gesture from the top center activates the
// multitask menu.
TEST_F(TabletModeMultitaskMenuTest, BasicShowMenu) {
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
  chromeos::MultitaskMenuViewTestApi test_api(multitask_menu_view);
  EXPECT_TRUE(test_api.GetHalfButton());
  EXPECT_TRUE(multitask_menu_view->partial_button());
  EXPECT_TRUE(test_api.GetFullButton());
  EXPECT_TRUE(test_api.GetFloatButton());

  // Verify that the menu is horizontally centered.
  EXPECT_EQ(multitask_menu->widget()
                ->GetContentsView()
                ->GetBoundsInScreen()
                .CenterPoint()
                .x(),
            window->GetBoundsInScreen().CenterPoint().x());
}

TEST_F(TabletModeMultitaskMenuTest, SwipeDownTargetArea) {
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
TEST_F(TabletModeMultitaskMenuTest, PressMoveAndReleaseTouch) {
  auto window = CreateAppWindow(gfx::Rect(800, 600));
  ShowMultitaskMenu(*window);

  // Press and move the touch slightly to mimic a real tap.
  auto* half_button = chromeos::MultitaskMenuViewTestApi(
                          GetMultitaskMenuView(GetMultitaskMenu()))
                          .GetHalfButton();
  GetEventGenerator()->set_current_screen_location(
      half_button->GetBoundsInScreen().left_center());
  GetEventGenerator()->PressMoveAndReleaseTouchBy(0, 3);

  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(window.get())->GetStateType());
}

TEST_F(TabletModeMultitaskMenuTest, SwipeDownInSplitView) {
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
TEST_F(TabletModeMultitaskMenuTest, SwipeDownInSplitViewWhileAnimating) {
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
TEST_F(TabletModeMultitaskMenuTest, SwipeDownInPinnedWindow) {
  // Create and pin a window.
  std::unique_ptr<aura::Window> pinned_window = CreateAppWindow();
  wm::ActivateWindow(pinned_window.get());
  window_util::PinWindow(pinned_window.get(), /*trusted=*/true);

  GenerateScroll(pinned_window->bounds().CenterPoint().x(), 0, 150);
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that swipe down outside the menu doesn't crash. Test for b/266742428.
TEST_F(TabletModeMultitaskMenuTest, SwipeDownMenuTwice) {
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
TEST_F(TabletModeMultitaskMenuTest, MultiFingerSroll) {
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
TEST_F(TabletModeMultitaskMenuTest, PartialDrag) {
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
TEST_F(TabletModeMultitaskMenuTest, OnWindowDestroying) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(*window);
  ASSERT_TRUE(GetMultitaskMenu());

  // Close the window.
  window.reset();
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that tap outside the menu will close the menu.
TEST_F(TabletModeMultitaskMenuTest, CloseMultitaskMenuOnTap) {
  // Create a display and window that is bigger than the menu.
  UpdateDisplay("1600x1000");
  auto window = CreateAppWindow();

  ShowMultitaskMenu(*window);
  ASSERT_TRUE(GetMultitaskMenu());

  // Tap outside the menu. Verify that we close the menu.
  GetEventGenerator()->GestureTapAt(window->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that pressing a button before the show animation ends closes the menu
// (http://b/279355302).
TEST_F(TabletModeMultitaskMenuTest, CloseMultitaskMenuOnButtonPress) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Swipe down the menu partially to start an animation.
  auto window = CreateAppWindow();
  GenerateScroll(window->bounds().CenterPoint().x(), 0,
                 /*end_y=*/60);
  GestureTapOn(chromeos::MultitaskMenuViewTestApi(
                   GetMultitaskMenuView(GetMultitaskMenu()))
                   .GetFloatButton());

  // Wait for the TabletModeMultitaskMenuView layer to fade out.
  ui::LayerAnimationStoppedWaiter().Wait(
      GetMultitaskMenu()->widget()->GetContentsView()->layer());

  ASSERT_FALSE(GetMultitaskMenu());
}

TEST_F(TabletModeMultitaskMenuTest, CloseOnDoubleTapDivider) {
  auto window1 = CreateTestWindow(gfx::Rect(800, 600));
  auto window2 = CreateTestWindow(gfx::Rect(800, 600));

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);

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

TEST_F(TabletModeMultitaskMenuTest, HideMultitaskMenuInOverview) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(*window);

  auto* event_handler = TabletModeControllerTestApi()
                            .tablet_mode_window_manager()
                            ->tablet_mode_multitask_menu_controller();
  auto* multitask_menu = event_handler->multitask_menu();
  ASSERT_TRUE(multitask_menu);
  ASSERT_TRUE(multitask_menu->widget()->GetContentsView()->GetVisible());

  EnterOverview();

  // Verify that the menu is hidden.
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that the multitask menu gets updated after a button is pressed.
TEST_F(TabletModeMultitaskMenuTest, HalfButtonFunctionality) {
  // Create a test window wide enough to show the menu even after it is split in
  // half.
  UpdateDisplay("1600x1000");
  auto window = CreateTestWindow();
  ShowMultitaskMenu(*window);

  // Press the primary half split button.
  auto* half_button = chromeos::MultitaskMenuViewTestApi(
                          GetMultitaskMenuView(GetMultitaskMenu()))
                          .GetHalfButton();
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

TEST_F(TabletModeMultitaskMenuTest, PartialButtonFunctionality) {
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
TEST_F(TabletModeMultitaskMenuTest, AdjustedMenuBounds) {
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
  split_view_controller->SwapWindows();
  ShowMultitaskMenu(*window2);
  EXPECT_EQ(work_area.x(),
            GetMultitaskMenu()->widget()->GetWindowBoundsInScreen().x());
}

// Tests that the split buttons are enabled/disabled based on min sizes.
TEST_F(TabletModeMultitaskMenuTest, WindowMinimumSizes) {
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
  ASSERT_TRUE(
      chromeos::MultitaskMenuViewTestApi(multitask_menu_view).GetHalfButton());
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
  ASSERT_FALSE(
      chromeos::MultitaskMenuViewTestApi(multitask_menu_view).GetHalfButton());
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
  EXPECT_FALSE(
      chromeos::MultitaskMenuViewTestApi(multitask_menu_view).GetHalfButton());
  EXPECT_FALSE(multitask_menu_view->partial_button());
  GetMultitaskMenu()->Reset();

  // Snap `window` to 1/3 to set its snap ratio to 1/3.
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY,
                                    chromeos::kOneThirdSnapRatio);
  WindowState::Get(window.get())->OnWMEvent(&snap_left);
  const WMEvent restore(WM_EVENT_RESTORE);
  WindowState::Get(window.get())->OnWMEvent(&restore);

  // Set minimum size to make `window` snappable in 1/2 ratio but not in 1/3
  // ratio.
  delegate.set_minimum_size(gfx::Size(work_area_bounds.width() * 0.4, 0));

  // Half button should be visible according to snappability with the default
  // snap ratio instead of the window's current snap ratio.
  ShowMultitaskMenu(*window);
  multitask_menu_view = GetMultitaskMenuView(GetMultitaskMenu());
  EXPECT_TRUE(
      chromeos::MultitaskMenuViewTestApi(multitask_menu_view).GetHalfButton());
  EXPECT_TRUE(multitask_menu_view->partial_button()->GetEnabled());
  ASSERT_FALSE(multitask_menu_view->partial_button()
                   ->GetRightBottomButton()
                   ->GetEnabled());
}

// Tests that if a window cannot be snapped or floated, the buttons will not
// be shown.
TEST_F(TabletModeMultitaskMenuTest, HiddenButtons) {
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
  chromeos::MultitaskMenuViewTestApi test_api(multitask_menu_view);
  EXPECT_FALSE(test_api.GetHalfButton());
  EXPECT_FALSE(multitask_menu_view->partial_button());
  EXPECT_TRUE(test_api.GetFullButton());
  EXPECT_FALSE(test_api.GetFloatButton());
}

// Tests that the cue is still showing when the menu is opened, and it has been
// transformed to the correct position below the menu.
TEST_F(TabletModeMultitaskMenuTest, CueTransformOnShowMenu) {
  auto window = CreateAppWindow();

  auto* multitask_cue_controller =
      GetMultitaskMenuController()->multitask_cue_controller();
  ASSERT_TRUE(multitask_cue_controller);
  EXPECT_TRUE(multitask_cue_controller->cue_layer());

  // Cue should still be showing when the menu is activated.
  ShowMultitaskMenu(*window);
  ASSERT_TRUE(multitask_cue_controller);
  ui::Layer* cue_layer = multitask_cue_controller->cue_layer();
  EXPECT_TRUE(cue_layer);

  // Verify cue is transformed to the right position.
  const gfx::Rect expected_bounds(
      (window->bounds().width() - TabletModeMultitaskCueController::kCueWidth) /
          2,
      GetMultitaskMenuView(GetMultitaskMenu())->bounds().bottom() +
          kVerticalPosition + TabletModeMultitaskCueController::kCueYOffset,
      TabletModeMultitaskCueController::kCueWidth,
      TabletModeMultitaskCueController::kCueHeight);
  EXPECT_EQ(expected_bounds, cue_layer->GetTargetTransform().MapRect(
                                 cue_layer->GetTargetBounds()));

  // Cue should still dismiss via timer after menu opened.
  multitask_cue_controller->FireCueDismissTimerForTesting();

  ASSERT_TRUE(multitask_cue_controller);
  EXPECT_FALSE(multitask_cue_controller->cue_layer());
}

// Tests that the cue appears on the correct window when the multitask menu is
// activated on different windows in split view.
TEST_F(TabletModeMultitaskMenuTest, CueCorrectWindowInSplitView) {
  auto window1 = CreateAppWindow();
  PressPartialPrimary(*window1);
  auto window2 = CreateAppWindow();
  PressPartialSecondary(*window2);

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(split_view_controller->IsWindowInSplitView(window1.get()));
  ASSERT_TRUE(split_view_controller->IsWindowInSplitView(window2.get()));

  // Show the menu and cue on the first window.
  ShowMultitaskMenu(*window1);
  auto* multitask_cue_controller =
      GetMultitaskMenuController()->multitask_cue_controller();
  ASSERT_TRUE(multitask_cue_controller);
  EXPECT_TRUE(multitask_cue_controller->cue_layer());
  EXPECT_EQ(window1.get(), multitask_cue_controller->window_);

  // Show the menu and cue on the second window.
  ShowMultitaskMenu(*window2);
  multitask_cue_controller =
      GetMultitaskMenuController()->multitask_cue_controller();
  ASSERT_TRUE(multitask_cue_controller);
  EXPECT_TRUE(multitask_cue_controller->cue_layer());
  EXPECT_EQ(window2.get(), multitask_cue_controller->window_);
}

// Tests that the bottom window can open the multitask menu in portrait mode. In
// portrait primary view, the bottom window is on the right.
// ----------------------
// |   Top   |  Bottom  |
// ----------------------
TEST_F(TabletModeMultitaskMenuTest, ShowBottomMenuPortraitPrimary) {
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
  split_view_controller->SnapWindow(top_window.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(bottom_window.get(),
                                    SnapPosition::kSecondary);
  EXPECT_FALSE(
      IsPhysicallyLeftOrTop(SnapPosition::kSecondary, bottom_window.get()));
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
TEST_F(TabletModeMultitaskMenuTest, DISABLED_ShowBottomMenuPortraitSecondary) {
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
  split_view_controller->SnapWindow(bottom_window.get(),
                                    SnapPosition::kPrimary);
  split_view_controller->SnapWindow(top_window.get(), SnapPosition::kSecondary);
  EXPECT_FALSE(
      IsPhysicallyLeftOrTop(SnapPosition::kPrimary, bottom_window.get()));
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
TEST_F(TabletModeMultitaskMenuTest, NoCrashWhenExitingTabletMode) {
  // We need to use a non zero duration otherwise the fade out animation will
  // complete immediately and destroy the multitask menu before the tablet mode
  // window manager gets destroyed, which is not what happens on a real device.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto window = CreateAppWindow();
  ShowMultitaskMenu(*window);
  TabletModeControllerTestApi().LeaveTabletMode();
}

// Tests that update drag does not cause a crash. Test for http://b/290102602.
TEST_F(TabletModeMultitaskMenuTest, NoCrashDuringUpdateDrag) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  auto window = CreateAppWindow();

  // Partially drag down to start an animation. `end_y` must be less than half
  // the menu height to animate toward close.
  GenerateScroll(/*x=*/window->bounds().CenterPoint().x(), /*start_y=*/1,
                 /*end_y=*/50);
  auto* multitask_menu = GetMultitaskMenu();
  auto* menu_layer = multitask_menu->widget()->GetContentsView()->layer();
  ASSERT_TRUE(multitask_menu && menu_layer);
  ui::LayerAnimator* animator = menu_layer->GetAnimator();
  EXPECT_TRUE(animator->is_animating());
  // When animating to hide, the menu will be translated up by `translation_y`,
  // equal to the y translation in `initial_transform` in the constructor.
  const int translation_y =
      multitask_menu->widget()->GetContentsView()->height() + kVerticalPosition;
  auto transform = gfx::Transform::MakeTranslation(0, -translation_y);
  EXPECT_EQ(transform, menu_layer->GetTargetTransform());

  // Start another drag to abort the current animation.
  const int current_y = 10;
  multitask_menu->UpdateDrag(current_y, /*down=*/false);
  ASSERT_TRUE(multitask_menu && menu_layer);
  EXPECT_FALSE(animator->is_animating());
  const int initial_y =
      multitask_menu->widget()->GetContentsView()->bounds().bottom();
  transform = gfx::Transform::MakeTranslation(0, current_y - initial_y);
  EXPECT_EQ(transform, menu_layer->transform());

  // If we start a drag in a different direction, ensure we continue to set
  // transform.
  multitask_menu->UpdateDrag(current_y, /*down=*/true);
  ASSERT_TRUE(multitask_menu && menu_layer);
  EXPECT_FALSE(animator->is_animating());
  // transform = gfx::Transform::MakeTranslation(0, y - initial_y);
  EXPECT_EQ(transform, menu_layer->transform());
}

// Test that the window is created on the target window. This can crash if
// EventType::kScrollFlingStart is sent quickly enough after
// EventType::kGestureScrollUpdate, causing the controller to create the menu on
// the split view divider (b/293954921).
TEST_F(TabletModeMultitaskMenuTest, NoCrashWhenDraggingSplitViewDivider) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  UpdateDisplay("1600x1000");
  auto window = CreateAppWindow();
  PressPartialPrimary(*window);

  // Start dragging on the menu.
  const gfx::Point center_point(window->bounds().CenterPoint().x(), 0);
  auto* event_generator = GetEventGenerator();
  event_generator->PressTouchId(/*touch_id=*/0, center_point);
  event_generator->MoveTouchIdBy(/*touch_id=*/0, 0, 100);
  ASSERT_TRUE(GetMultitaskMenu());

  // Without releasing the first finger, start a fling on the divider and close
  // the menu (this can happen when it loses focus from the second touch).
  GetMultitaskMenu()->Reset();
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  auto* split_view_divider = split_view_controller->split_view_divider();
  const gfx::Point divider_center =
      split_view_divider->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint();
  event_generator->PressTouchId(/*touch_id=*/1, divider_center);
  event_generator->MoveTouchIdBy(/*touch_id=*/1, -10, 0);
  event_generator->ReleaseTouchId(/*touch_id=*/1);

  // Test that, even though the target window is the divider, we don't try to
  // create the menu on the split view divider.
  CHECK_EQ(GetMultitaskMenuController()->target_window_for_test(),
           split_view_divider->GetDividerWindow());
  EXPECT_FALSE(GetMultitaskMenu());
}

TEST_F(TabletModeMultitaskMenuTest, HidesWhenMinimized) {
  auto window = CreateAppWindow();
  ShowMultitaskMenu(*window);

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kWindowMinimize, {});
  ASSERT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_FALSE(GetMultitaskMenu());
}

TEST_F(TabletModeMultitaskMenuTest, NoTabletModeWindowManagerInKiosk) {
  SimulateKioskMode(user_manager::UserType::kKioskApp);
  auto window = CreateAppWindow(gfx::Rect(800, 600));

  // In Kiosk session there is no `TabletModeWindowManager` since the UI tablet
  // mode is blocked.
  EXPECT_EQ(TabletModeControllerTestApi().tablet_mode_window_manager(),
            nullptr);
}

namespace {

class TestHandler : public ui::EventHandler {
 public:
  TestHandler() = default;
  TestHandler(const TestHandler&) = delete;
  TestHandler& operator=(const TestHandler&) = delete;
  ~TestHandler() override = default;

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override { flags_ = event->flags(); }
  int GetFlagsAndReset() {
    EXPECT_NE(-1, flags_);
    int ret = flags_;
    flags_ = -1;
    return ret;
  }

  int flags() const { return flags_; }

 private:
  // latest flags.
  int flags_ = -1;
};

}  // namespace

// Make sure that swipe down will set the ui::EF_RESERVED_FOR_GESTURE flag
// on touch event, while swipe right will not.
TEST_F(TabletModeMultitaskMenuTest, BlockSwipeDown) {
  auto* menu_controller = TabletModeControllerTestApi()
                              .tablet_mode_window_manager()
                              ->tablet_mode_multitask_menu_controller();

  auto* root = Shell::GetPrimaryRootWindow();
  TestHandler test_handler;
  root->AddPreTargetHandler(&test_handler);

  auto window = CreateAppWindow();
  auto* generator = GetEventGenerator();

  // Start slightly off the edge.
  const gfx::Point starting_point(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().width() / 2,
      3);
  {
    // Emulate swipe down by touches.

    gfx::Point touch_point(starting_point);
    generator->PressTouch(touch_point);
    // Trigger Scroll Begin
    touch_point.set_y(5);
    generator->MoveTouch(touch_point);
    EXPECT_FALSE(menu_controller->is_drag_active_for_test());
    EXPECT_FALSE(menu_controller->reserved_for_gesture_sent_for_test());
    EXPECT_FALSE(test_handler.GetFlagsAndReset() & ui::EF_RESERVED_FOR_GESTURE);

    // Trigger Scroll Update which sets `is_drag_active` to true.
    touch_point.set_y(10);
    generator->MoveTouch(touch_point);
    EXPECT_TRUE(menu_controller->is_drag_active_for_test());
    EXPECT_FALSE(menu_controller->reserved_for_gesture_sent_for_test());
    EXPECT_FALSE(test_handler.GetFlagsAndReset() & ui::EF_RESERVED_FOR_GESTURE);

    // Next touch event should have the EF_RESERVED_FOR_GESTURE flag.
    touch_point.set_y(15);
    generator->MoveTouch(touch_point);
    EXPECT_TRUE(menu_controller->reserved_for_gesture_sent_for_test());
    EXPECT_TRUE(test_handler.GetFlagsAndReset() & ui::EF_RESERVED_FOR_GESTURE);

    // After this, touches should be blocked.
    touch_point.set_y(16);
    generator->MoveTouch(touch_point);
    EXPECT_EQ(-1, test_handler.flags());

    generator->ReleaseTouch();

    // The controller should be in clean state.
    EXPECT_FALSE(menu_controller->is_drag_active_for_test());
    EXPECT_FALSE(menu_controller->reserved_for_gesture_sent_for_test());
    EXPECT_EQ(-1, test_handler.flags());
  }

  // Emulate swipe right by touches. It should never trigger drag nor
  // set reserved_for_gesture_sent_for_test().
  {
    gfx::Point touch_point(starting_point);
    generator->PressTouch(touch_point);

    for (int i = 0; i < 3; i++) {
      touch_point.set_x(touch_point.x() + 5);
      generator->MoveTouch(touch_point);
      EXPECT_FALSE(menu_controller->is_drag_active_for_test());
      EXPECT_FALSE(menu_controller->reserved_for_gesture_sent_for_test());
      EXPECT_FALSE(test_handler.GetFlagsAndReset() &
                   ui::EF_RESERVED_FOR_GESTURE);
    }
    generator->ReleaseTouch();
  }

  // Cleanup
  root->RemovePreTargetHandler(&test_handler);
}

}  // namespace ash
