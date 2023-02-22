// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"

#include <memory>

#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_cue.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/frame/multitask_menu/split_button_view.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_handler.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The vertical position used to end drag to show and start drag to hide.
// TODO(b/267184500): Revert this value back to 100 when the feedback button is
// removed.
constexpr int kMenuDragPoint = 110;

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

  void PressPartialPrimary(const aura::Window& window) {
    ShowMultitaskMenu(window);
    GetEventGenerator()->GestureTapAt(GetMultitaskMenuView(GetMultitaskMenu())
                                          ->partial_button()
                                          ->GetBoundsInScreen()
                                          .left_center());
  }

  void PressPartialSecondary(const aura::Window& window) {
    ShowMultitaskMenu(window);
    gfx::Rect partial_bounds(GetMultitaskMenuView(GetMultitaskMenu())
                                 ->partial_button()
                                 ->GetBoundsInScreen());
    gfx::Point secondary_center(
        gfx::Point(partial_bounds.x() + partial_bounds.width() * 0.67f,
                   partial_bounds.y() + partial_bounds.y() * 0.5f));
    GetEventGenerator()->GestureTapAt(secondary_center);
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
    views::View* multitask_menu_view =
        multitask_menu->GetMultitaskMenuViewForTesting();
    EXPECT_EQ(chromeos::MultitaskMenuView::kViewClassName,
              multitask_menu_view->GetClassName());
    return static_cast<chromeos::MultitaskMenuView*>(multitask_menu_view);
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

// Tests that the bottom window can open the multitask menu in portrait mode. In
// portrait primary view, the bottom window is on the right.
// ----------------------------
// |  PRIMARY   |  SECONDARY  |
// ----------------------------
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ShowBottomMenuPortraitPrimary) {
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  ASSERT_EQ(chromeos::OrientationType::kPortraitPrimary,
            test_api.GetCurrentOrientation());

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  split_view_controller->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  wm::ActivateWindow(window2.get());

  // Event generation coordinates are relative to the natural origin, but
  // `window` bounds are relative to the portrait origin. Scroll from the
  // divider toward the right to open the menu.
  const gfx::Rect bounds(window2->bounds());
  const gfx::Point start(bounds.y() + 8, bounds.CenterPoint().x());
  const gfx::Point end(bounds.y() + kMenuDragPoint, bounds.CenterPoint().x());
  GetEventGenerator()->GestureScrollSequence(start, end,
                                             base::Milliseconds(100),
                                             /*steps=*/3);
  ASSERT_TRUE(GetMultitaskMenu());
}

// Tests that the bottom window can open the multitask menu in portrait mode. In
// portrait secondary view, the bottom window is on the left.
// ----------------------------
// |  SECONDARY  |   PRIMARY  |
// ----------------------------
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
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  split_view_controller->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  // Event generation coordinates are relative to the natural origin, but
  // `window` bounds are relative to the portrait origin. Scroll from the
  // divider toward the left to open the menu.
  const gfx::Rect bounds(window2->bounds());
  const gfx::Point start(bounds.height(), bounds.CenterPoint().x());
  const gfx::Point end(bounds.height() - kMenuDragPoint,
                       bounds.CenterPoint().x());
  GetEventGenerator()->GestureScrollSequence(start, end,
                                             base::Milliseconds(100),
                                             /*steps=*/3);
  EXPECT_TRUE(GetMultitaskMenu());
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

// Tests that scroll down shows the menu as expected.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ScrollDownGestures) {
  UpdateDisplay("1600x1000");
  auto window = CreateTestWindow();

  // Scroll down from the top left. Verify that we do not show the menu.
  GenerateScroll(0, 1, kMenuDragPoint);
  ASSERT_FALSE(GetMultitaskMenu());

  // Scroll down from the top right. Verify that we do not show the menu.
  GenerateScroll(window->bounds().right(), 1, kMenuDragPoint);
  ASSERT_FALSE(GetMultitaskMenu());

  // Scroll down from the top center. Verify that we show the menu.
  GenerateScroll(window->bounds().CenterPoint().x(), 1, kMenuDragPoint);
  ASSERT_TRUE(GetMultitaskMenu());

  // Scroll up on the menu. Verify that we close the menu.
  GenerateScroll(window->bounds().CenterPoint().x(), kMenuDragPoint, 8);
  ASSERT_FALSE(GetMultitaskMenu());

  // Scroll down from the top left. Verify that we do not show the menu.
  GenerateScroll(0, 1, kMenuDragPoint);
  ASSERT_FALSE(GetMultitaskMenu());

  // Test that the entry metric is recorded once.
  histogram_tester_.ExpectBucketCount(
      chromeos::GetEntryTypeHistogramName(),
      chromeos::MultitaskMenuEntryType::kGestureScroll, 1);
}

// Tests that scroll up closes the menu as expected.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ScrollUpGestures) {
  auto window = CreateTestWindow();
  const int center_x = window->bounds().CenterPoint().x();
  const int center_y = window->bounds().CenterPoint().y();

  // Scroll up with no menu open. Verify no change.
  GenerateScroll(center_x, kMenuDragPoint, 8);
  ASSERT_FALSE(GetMultitaskMenu());

  ShowMultitaskMenu(*window);
  ASSERT_TRUE(GetMultitaskMenu());

  // Scroll down again. Verify that we still show the menu.
  GenerateScroll(center_x, 1, kMenuDragPoint);
  ASSERT_TRUE(GetMultitaskMenu());

  // Scroll up at a point outside the menu and above the shelf. Verify that we
  // close the menu.
  GenerateScroll(center_x, center_y, center_y - kMenuDragPoint);
  EXPECT_FALSE(GetMultitaskMenu());

  ShowMultitaskMenu(*window);
  ASSERT_TRUE(GetMultitaskMenu());

  // Scroll up on the menu. Verify that we close the menu.
  GenerateScroll(center_x, kMenuDragPoint, 8);
  EXPECT_FALSE(GetMultitaskMenu());
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
  const gfx::Rect divider_bounds =
      SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->split_view_divider()
          ->GetDividerBoundsInScreen(
              /*is_dragging*/ false);
  ASSERT_NEAR(work_area_bounds.width() * 0.5f,
              window->GetBoundsInScreen().width(), divider_bounds.width());

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
  const gfx::Rect divider_bounds =
      SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->split_view_divider()
          ->GetDividerBoundsInScreen(
              /*is_dragging*/ false);
  ASSERT_NEAR(work_area_bounds.width() * 0.67f, window->bounds().width(),
              divider_bounds.width());
  ASSERT_FALSE(GetMultitaskMenu());
  histogram_tester_.ExpectBucketCount(
      chromeos::GetActionTypeHistogramName(),
      chromeos::MultitaskMenuActionType::kPartialSplitButton, 1);

  // Test that secondary button snaps to 0.33f screen ratio.
  PressPartialSecondary(*window);
  ASSERT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(window.get())->GetStateType());
  ASSERT_NEAR(work_area_bounds.width() * 0.33f, window->bounds().width(),
              divider_bounds.width());
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
  ASSERT_NEAR(work_area.width() * 0.33f, window2->bounds().width(),
              kSplitviewDividerShortSideLength);
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
TEST_F(TabletModeMultitaskMenuEventHandlerTest, DismissCueOnShowMenu) {
  auto window = CreateAppWindow();

  auto* multitask_cue =
      GetMultitaskMenuEventHandler()->multitask_cue_for_testing();
  ASSERT_TRUE(multitask_cue);
  EXPECT_TRUE(multitask_cue->cue_layer());

  ShowMultitaskMenu(*window);

  multitask_cue = GetMultitaskMenuEventHandler()->multitask_cue_for_testing();
  ASSERT_TRUE(multitask_cue);
  EXPECT_FALSE(multitask_cue->cue_layer());
}

}  // namespace ash
