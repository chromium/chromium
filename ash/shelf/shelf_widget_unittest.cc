// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include "ash/constants/ash_switches.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/hotseat_transition_animator.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

ShelfWidget* GetShelfWidget() {
  return AshTestBase::GetPrimaryShelf()->shelf_widget();
}

ShelfLayoutManager* GetShelfLayoutManager() {
  return GetShelfWidget()->shelf_layout_manager();
}

void TestLauncherAlignment(aura::Window* root,
                           ShelfAlignment alignment,
                           const gfx::Rect& expected) {
  Shelf::ForWindow(root)->SetAlignment(alignment);
  EXPECT_EQ(expected.ToString(), display::Screen::GetScreen()
                                     ->GetDisplayNearestWindow(root)
                                     .work_area()
                                     .ToString());
}

using SessionState = session_manager::SessionState;
using ShelfWidgetTest = AshTestBase;

TEST_F(ShelfWidgetTest, TestAlignment) {
  UpdateDisplay("401x400");
  const int bottom_inset = 400 - ShelfConfig::Get()->shelf_size();
  {
    SCOPED_TRACE("Single Bottom");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          ShelfAlignment::kBottom,
                          gfx::Rect(0, 0, 401, bottom_inset));
  }
  {
    SCOPED_TRACE("Single Locked");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          ShelfAlignment::kBottomLocked,
                          gfx::Rect(0, 0, 401, bottom_inset));
  }
  {
    SCOPED_TRACE("Single Right");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(), ShelfAlignment::kRight,
                          gfx::Rect(0, 0, bottom_inset + 1, 400));
  }
  {
    SCOPED_TRACE("Single Left");
    TestLauncherAlignment(
        Shell::GetPrimaryRootWindow(), ShelfAlignment::kLeft,
        gfx::Rect(ShelfConfig::Get()->shelf_size(), 0, bottom_inset + 1, 400));
  }
}

TEST_F(ShelfWidgetTest, TestAlignmentForMultipleDisplays) {
  UpdateDisplay("301x300,501x500");
  const int shelf_inset_first = 300 - ShelfConfig::Get()->shelf_size();
  const int shelf_inset_second = 500 - ShelfConfig::Get()->shelf_size();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  {
    SCOPED_TRACE("Primary Bottom");
    TestLauncherAlignment(root_windows[0], ShelfAlignment::kBottom,
                          gfx::Rect(0, 0, 301, shelf_inset_first));
  }
  {
    SCOPED_TRACE("Primary Locked");
    TestLauncherAlignment(root_windows[0], ShelfAlignment::kBottomLocked,
                          gfx::Rect(0, 0, 301, shelf_inset_first));
  }
  {
    SCOPED_TRACE("Primary Right");
    TestLauncherAlignment(root_windows[0], ShelfAlignment::kRight,
                          gfx::Rect(0, 0, shelf_inset_first + 1, 300));
  }
  {
    SCOPED_TRACE("Primary Left");
    TestLauncherAlignment(root_windows[0], ShelfAlignment::kLeft,
                          gfx::Rect(ShelfConfig::Get()->shelf_size(), 0,
                                    shelf_inset_first + 1, 300));
  }
  {
    SCOPED_TRACE("Secondary Bottom");
    TestLauncherAlignment(root_windows[1], ShelfAlignment::kBottom,
                          gfx::Rect(301, 0, 501, shelf_inset_second));
  }
  {
    SCOPED_TRACE("Secondary Locked");
    TestLauncherAlignment(root_windows[1], ShelfAlignment::kBottomLocked,
                          gfx::Rect(301, 0, 501, shelf_inset_second));
  }
  {
    SCOPED_TRACE("Secondary Right");
    TestLauncherAlignment(root_windows[1], ShelfAlignment::kRight,
                          gfx::Rect(301, 0, shelf_inset_second + 1, 500));
  }
  {
    SCOPED_TRACE("Secondary Left");
    TestLauncherAlignment(root_windows[1], ShelfAlignment::kLeft,
                          gfx::Rect(301 + ShelfConfig::Get()->shelf_size(), 0,
                                    shelf_inset_second + 1, 500));
  }
}

class ShelfWidgetDarkLightModeTest : public ShelfWidgetTest {
 public:
  void SetUp() override {
    ShelfWidgetTest::SetUp();

    // Enable tablet mode transition screenshots to simulate production behavior
    // where shelf layers get recreated during the tablet mode transition.
    TabletModeController::SetUseScreenshotForTest(true);
  }
};

TEST_F(ShelfWidgetDarkLightModeTest, TabletModeTransition) {
  ShelfWidget* const shelf_widget = GetShelfWidget();

  TabletMode::Waiter enter_waiter(/*enable=*/true);
  TabletModeControllerTestApi().EnterTabletMode();
  enter_waiter.Wait();
  shelf_widget->background_animator_for_testing()
      ->CompleteAnimationForTesting();

  EXPECT_EQ(shelf_widget->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevated),
            shelf_widget->GetShelfBackgroundColor());

  EXPECT_EQ(0.0, shelf_widget->GetOpaqueBackground()->background_blur());

  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->ToggleColorMode();

  EXPECT_EQ(shelf_widget->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevated),
            shelf_widget->GetShelfBackgroundColor());
  EXPECT_EQ(0.0f, shelf_widget->GetOpaqueBackground()->background_blur());

  TabletMode::Waiter leave_waiter(/*enable=*/false);
  TabletModeControllerTestApi().LeaveTabletMode();
  leave_waiter.Wait();
  shelf_widget->background_animator_for_testing()
      ->CompleteAnimationForTesting();

  EXPECT_EQ(shelf_widget->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevated),
            shelf_widget->GetShelfBackgroundColor());
  EXPECT_GT(shelf_widget->GetOpaqueBackground()->background_blur(), 0.0f);

  dark_light_mode_controller->ToggleColorMode();

  EXPECT_EQ(shelf_widget->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevated),
            shelf_widget->GetShelfBackgroundColor());
  EXPECT_GT(shelf_widget->GetOpaqueBackground()->background_blur(), 0.0f);
}

TEST_F(ShelfWidgetDarkLightModeTest, TabletModeTransitionWithWindowOpen) {
  ShelfWidget* const shelf_widget = GetShelfWidget();
  auto window = AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 800, 800));

  TabletMode::Waiter enter_waiter(/*enable=*/true);
  TabletModeControllerTestApi().EnterTabletMode();
  enter_waiter.Wait();
  shelf_widget->background_animator_for_testing()
      ->CompleteAnimationForTesting();

  EXPECT_EQ(shelf_widget->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBase),
            shelf_widget->GetShelfBackgroundColor());
  EXPECT_EQ(0.0f, shelf_widget->GetOpaqueBackground()->background_blur());

  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->ToggleColorMode();

  EXPECT_EQ(shelf_widget->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBase),
            shelf_widget->GetShelfBackgroundColor());
  EXPECT_EQ(0.0f, shelf_widget->GetOpaqueBackground()->background_blur());

  TabletMode::Waiter leave_waiter(/*enable=*/false);
  TabletModeControllerTestApi().LeaveTabletMode();
  leave_waiter.Wait();
  shelf_widget->background_animator_for_testing()
      ->CompleteAnimationForTesting();

  EXPECT_EQ(shelf_widget->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevated),
            shelf_widget->GetShelfBackgroundColor());
  EXPECT_GT(shelf_widget->GetOpaqueBackground()->background_blur(), 0.0f);

  dark_light_mode_controller->ToggleColorMode();

  EXPECT_EQ(shelf_widget->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevated),
            shelf_widget->GetShelfBackgroundColor());
  EXPECT_GT(shelf_widget->GetOpaqueBackground()->background_blur(), 0.0f);
}

class ShelfWidgetLayoutBasicsTest
    : public ShelfWidgetTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ShelfWidgetLayoutBasicsTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kHideShelfControlsInTabletMode, std::get<1>(GetParam()));
  }
  ~ShelfWidgetLayoutBasicsTest() override = default;

  void SetUp() override {
    ShelfWidgetTest::SetUp();

    if (std::get<0>(GetParam())) {
      ash::TabletModeControllerTestApi().EnterTabletMode();
    } else {
      ash::TabletModeControllerTestApi().LeaveTabletMode();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// First parameter - whether test should run in tablet mode.
// Second parameter - whether shelf navigation buttons are enabled in tablet
// mode.
INSTANTIATE_TEST_SUITE_P(All,
                         ShelfWidgetLayoutBasicsTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Makes sure the shelf is initially sized correctly in clamshell/tablet.
TEST_P(ShelfWidgetLayoutBasicsTest, LauncherInitiallySized) {
  ShelfWidget* shelf_widget = GetShelfWidget();
  ShelfLayoutManager* shelf_layout_manager = GetShelfLayoutManager();
  ASSERT_TRUE(shelf_layout_manager);
  ASSERT_TRUE(shelf_widget->status_area_widget());
  int status_width =
      shelf_widget->status_area_widget()->GetWindowBoundsInScreen().width();
  // Test only makes sense if the status is > 0, which it better be.
  EXPECT_GT(status_width, 0);

  const int total_width =
      screen_util::GetDisplayBoundsWithShelf(shelf_widget->GetNativeWindow())
          .width();

  const gfx::Rect nav_widget_clip_rect =
      shelf_widget->navigation_widget()->GetLayer()->clip_rect();
  if (!nav_widget_clip_rect.IsEmpty())
    EXPECT_EQ(gfx::Point(), nav_widget_clip_rect.origin());

  int nav_width =
      nav_widget_clip_rect.IsEmpty()
          ? shelf_widget->navigation_widget()->GetWindowBoundsInScreen().width()
          : nav_widget_clip_rect.width();
  ASSERT_GE(nav_width, 0);
  if (nav_width) {
    nav_width -= ShelfConfig::Get()->control_button_edge_spacing(
        true /* is_primary_axis_edge */);
  }

  const int hotseat_width =
      shelf_widget->hotseat_widget()->GetWindowBoundsInScreen().width();
  const int margins = 2 * ShelfConfig::Get()->GetAppIconGroupMargin();

  EXPECT_EQ(status_width, total_width - nav_width - hotseat_width - margins);
}

TEST_F(ShelfWidgetTest, CheckVerticalShelfCornersInOverviewMode) {
  // Verify corners of the shelf on the left alignment.
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(ShelfAlignment::kLeft, GetPrimaryShelf()->alignment());

  ShelfWidget* const shelf_widget = GetShelfWidget();
  ui::Layer* opaque_background_layer =
      shelf_widget->GetDelegateViewOpaqueBackgroundLayerForTesting();

  // The gfx::RoundedCornersF object is considered empty when all of the
  // corners are squared (no effective radius).
  EXPECT_FALSE(opaque_background_layer->rounded_corner_radii().IsEmpty());

  OverviewController* overview_controller = OverviewController::Get();
  // Enter overview mode. Expect the shelf with square corners.
  EnterOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(opaque_background_layer->rounded_corner_radii().IsEmpty());

  // Exit overview mode. Expect the shelf with rounded corners.
  ExitOverview();
  WaitForOverviewAnimation(/*enter=*/false);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(opaque_background_layer->rounded_corner_radii().IsEmpty());

  // Verify corners of the shelf on the right alignment.
  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(ShelfAlignment::kRight, GetPrimaryShelf()->alignment());
  EXPECT_FALSE(opaque_background_layer->rounded_corner_radii().IsEmpty());

  // Enter overview mode. Expect the shelf with square corners.
  EnterOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(opaque_background_layer->rounded_corner_radii().IsEmpty());

  // Exit overview mode. Expect the shelf with rounded corners.
  ExitOverview();
  WaitForOverviewAnimation(/*enter=*/false);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(opaque_background_layer->rounded_corner_radii().IsEmpty());
}

// Verifies when the shell is deleted with a full screen window we don't crash.
TEST_F(ShelfWidgetTest, DontReferenceShelfAfterDeletion) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = GetContext();
  // Widget is now owned by the parent window.
  widget->Init(std::move(params));
  widget->SetFullscreen(true);
}

// Verifies shelf is created with correct size after user login and when its
// container and status widget has finished sizing.
// See http://crbug.com/252533
TEST_F(ShelfWidgetTest, ShelfInitiallySizedAfterLogin) {
  ClearLogin();
  UpdateDisplay("300x200,400x300");

  // Both displays have a shelf controller.
  aura::Window::Windows roots = Shell::GetAllRootWindows();
  Shelf* shelf1 = Shelf::ForWindow(roots[0]);
  Shelf* shelf2 = Shelf::ForWindow(roots[1]);
  ASSERT_TRUE(shelf1);
  ASSERT_TRUE(shelf2);

  // Both shelf controllers have a shelf widget.
  ShelfWidget* shelf_widget1 = shelf1->shelf_widget();
  ShelfWidget* shelf_widget2 = shelf2->shelf_widget();
  ASSERT_TRUE(shelf_widget1);
  ASSERT_TRUE(shelf_widget2);

  // Simulate login.
  CreateUserSessions(1);

  const int total_width1 =
      screen_util::GetDisplayBoundsWithShelf(shelf_widget1->GetNativeWindow())
          .width();
  const int nav_width1 =
      shelf_widget1->navigation_widget()->GetWindowBoundsInScreen().width() -
      ShelfConfig::Get()->control_button_edge_spacing(
          true /* is_primary_axis_edge */);
  const int hotseat_width1 =
      shelf_widget1->hotseat_widget()->GetWindowBoundsInScreen().width();
  const int margins = 2 * ShelfConfig::Get()->GetAppIconGroupMargin();

  const int total_width2 =
      screen_util::GetDisplayBoundsWithShelf(shelf_widget2->GetNativeWindow())
          .width();
  const int nav_width2 =
      shelf_widget2->navigation_widget()->GetWindowBoundsInScreen().width() -
      ShelfConfig::Get()->control_button_edge_spacing(
          true /* is_primary_axis_edge */);
  const int hotseat_width2 =
      shelf_widget2->hotseat_widget()->GetWindowBoundsInScreen().width();

  // The shelf view and status area horizontally fill the shelf widget.
  const int status_width1 =
      shelf_widget1->status_area_widget()->GetWindowBoundsInScreen().width();
  EXPECT_GT(status_width1, 0);
  EXPECT_EQ(total_width1,
            nav_width1 + hotseat_width1 + margins + status_width1);

  const int status_width2 =
      shelf_widget2->status_area_widget()->GetWindowBoundsInScreen().width();
  EXPECT_GT(status_width2, 0);
  EXPECT_EQ(total_width2,
            nav_width2 + hotseat_width2 + margins + status_width2);
}

// Tests that the shelf has a slightly larger hit-region for touch-events when
// it's in the auto-hidden state.
TEST_F(ShelfWidgetTest, HiddenShelfHitTestTouch) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfWidget* shelf_widget = GetShelfWidget();
  gfx::Rect shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(!shelf_bounds.IsEmpty());
  ShelfLayoutManager* shelf_layout_manager =
      shelf_widget->shelf_layout_manager();
  ASSERT_TRUE(shelf_layout_manager);
  EXPECT_EQ(SHELF_VISIBLE, shelf_layout_manager->visibility_state());

  // Create a widget to make sure that the shelf does auto-hide.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = GetContext();
  // Widget is now owned by the parent window.
  widget->Init(std::move(params));
  widget->Show();

  aura::Window* root = shelf_widget->GetNativeWindow()->GetRootWindow();
  ui::EventTargeter* targeter =
      root->GetHost()->dispatcher()->GetDefaultEventTargeter();
  // Touch just over the shelf. Since the shelf is visible, the window-targeter
  // should not find the shelf as the target.
  {
    gfx::Point event_location(20, shelf_bounds.y() - 1);
    ui::TouchEvent touch(ui::EventType::kTouchPressed, event_location,
                         ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    EXPECT_NE(shelf_widget->GetNativeWindow(),
              targeter->FindTargetForEvent(root, &touch));
  }

  // Now auto-hide (hidden) the shelf.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_layout_manager->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_layout_manager->auto_hide_state());
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(!shelf_bounds.IsEmpty());

  // Touch just over the shelf again. This time, the targeter should find the
  // shelf as the target.
  {
    gfx::Point event_location(20, shelf_bounds.y() - 1);
    ui::TouchEvent touch(ui::EventType::kTouchPressed, event_location,
                         ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    EXPECT_EQ(shelf_widget->GetNativeWindow(),
              targeter->FindTargetForEvent(root, &touch));
  }
}

// Tests that the shelf lets mouse-events close to the edge fall through to the
// window underneath.
TEST_F(ShelfWidgetTest, ShelfEdgeOverlappingWindowHitTestMouse) {
  UpdateDisplay("500x400");
  ShelfWidget* shelf_widget = GetShelfWidget();
  gfx::Rect shelf_bounds = shelf_widget->GetWindowBoundsInScreen();

  EXPECT_TRUE(!shelf_bounds.IsEmpty());
  ShelfLayoutManager* shelf_layout_manager =
      shelf_widget->shelf_layout_manager();
  ASSERT_TRUE(shelf_layout_manager);
  EXPECT_EQ(SHELF_VISIBLE, shelf_layout_manager->visibility_state());

  // Create a Widget which overlaps the shelf in both left and bottom
  // alignments.
  const int kOverlapSize = 15;
  const int kWindowHeight = 200;
  const int kWindowWidth = 200;
  const gfx::Rect bounds(shelf_bounds.height() - kOverlapSize,
                         shelf_bounds.y() - kWindowHeight + kOverlapSize,
                         kWindowWidth, kWindowHeight);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = bounds;
  params.context = GetContext();
  // Widget is now owned by the parent window.
  widget->Init(std::move(params));
  // Explicitly set the bounds which will allow the widget to overlap the shelf.
  widget->SetBounds(bounds);
  widget->Show();
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(widget_bounds.Intersects(shelf_bounds));

  aura::Window* root = widget->GetNativeWindow()->GetRootWindow();
  ui::EventTargeter* targeter =
      root->GetHost()->dispatcher()->GetDefaultEventTargeter();
  {
    // Create a mouse-event targeting the top of the shelf widget. The
    // window-targeter should find |widget| as the target (instead of the
    // shelf).
    gfx::Point event_location(widget_bounds.x() + 5, shelf_bounds.y() + 1);
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                         event_location, ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(widget->GetNativeWindow(), target);
  }

  // Change shelf alignment to verify that the targeter insets are updated.
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kLeft);
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  {
    // Create a mouse-event targeting the right edge of the shelf widget. The
    // window-targeter should find |widget| as the target (instead of the
    // shelf).
    gfx::Point event_location(shelf_bounds.right() - 1, widget_bounds.y() + 5);
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                         event_location, ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(widget->GetNativeWindow(), target);
  }

  // Now restore shelf alignment (bottom) and auto-hide (hidden) the shelf.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_layout_manager->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_layout_manager->auto_hide_state());
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(!shelf_bounds.IsEmpty());

  // Move |widget| so it still overlaps the shelf.
  widget->SetBounds(gfx::Rect(0,
                              shelf_bounds.y() - kWindowHeight + kOverlapSize,
                              kWindowWidth, kWindowHeight));
  widget_bounds = widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(widget_bounds.Intersects(shelf_bounds));
  {
    // Create a mouse-event targeting the top of the shelf widget. This time,
    // window-target should find the shelf as the target.
    gfx::Point event_location(widget_bounds.x() + 5, shelf_bounds.y() + 1);
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                         event_location, ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(shelf_widget->GetNativeWindow(), target);
  }
}

class TransitionAnimationWaiter
    : public HotseatTransitionAnimator::TestObserver {
 public:
  explicit TransitionAnimationWaiter(
      HotseatTransitionAnimator* hotseat_transition_animator)
      : hotseat_transition_animator_(hotseat_transition_animator) {
    hotseat_transition_animator_->SetTestObserver(this);
  }
  ~TransitionAnimationWaiter() override {
    hotseat_transition_animator_->SetTestObserver(nullptr);
  }

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void OnTransitionTestAnimationEnded() override {
    DCHECK(run_loop_.get());
    run_loop_->Quit();
  }

  raw_ptr<HotseatTransitionAnimator> hotseat_transition_animator_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Tests the drag handle bounds and visibility when the in-app shelf is shown.
TEST_F(ShelfWidgetTest, OpaqueBackgroundAndDragHandleTransition) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ash::TabletModeControllerTestApi().EnterTabletMode();
  UpdateDisplay("800x700");

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());

  // Create a window to transition to the in-app shelf.
  auto window = AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 800, 800));

  {
    TransitionAnimationWaiter waiter(
        GetShelfWidget()->hotseat_transition_animator());
    waiter.Wait();
  }

  // Ensure the ShelfWidget and drag handle have the correct bounds and
  // visibility for in-app shelf.
  EXPECT_EQ(GetShelfWidget()->GetWindowBoundsInScreen(),
            gfx::Rect(0, 660, 800, 40));
  EXPECT_EQ(GetShelfWidget()->GetDragHandle()->bounds(),
            gfx::Rect(360, 18, 80, 4));
  ASSERT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());

  window->Hide();

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
}

// Tests the shelf widget does not animate for hotseat transitions during tablet
// mode start.
TEST_F(ShelfWidgetTest, NoAnimatingBackgroundDuringTabletModeStartToInApp) {
  UpdateDisplay("800x700");
  // Create a window so tablet mode uses in-app shelf.
  auto window = AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 800, 800));

  EXPECT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ash::TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());
}

// Tests the shelf widget does not animate for hotseat transitions during tablet
// mode end.
TEST_F(ShelfWidgetTest, NoAnimatingBackgroundDuringTabletModeEndFromInApp) {
  UpdateDisplay("800x700");
  ash::TabletModeControllerTestApi().EnterTabletMode();

  // Create a window so tablet mode uses in-app shelf.
  auto window = AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 800, 800));

  EXPECT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ash::TabletModeControllerTestApi().LeaveTabletMode();

  EXPECT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());
}

// Tests the shelf widget does not animate for hotseat transitions during tablet
// mode start with no app windows.
TEST_F(ShelfWidgetTest, NoAnimatingBackgroundDuringTabletModeStartToHome) {
  UpdateDisplay("800x700");

  EXPECT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ash::TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());
}

// Tests the shelf widget does not animate for hotseat transitions during tablet
// mode end with no app windows.
TEST_F(ShelfWidgetTest, NoAnimatingBackgroundDuringTabletModeEndFromHome) {
  UpdateDisplay("800x700");
  ash::TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ash::TabletModeControllerTestApi().LeaveTabletMode();

  EXPECT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());
}

// Tests the shelf widget does not animate for hotseat transitions if the screen
// is locked.
TEST_F(ShelfWidgetTest, NoAnimatingBackgroundOnLockScreen) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  UpdateDisplay("800x700");

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());

  // Create a window to transition to the in-app shelf.
  auto window = AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 800, 800));

  // At this point animations have zero duration, so the transition happens
  // immediately.
  ASSERT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());

  GetSessionControllerClient()->SetSessionState(SessionState::LOCKED);

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Hide the test window, and verify that does not start shelf animation.
  window->Hide();

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());

  // Ensure the ShelfWidget and the drag handle are still hidden (i.e. in home
  // screen state) when the screen is unlocked.
  GetSessionControllerClient()->SetSessionState(SessionState::ACTIVE);
  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());
}

TEST_F(ShelfWidgetTest, NoAnimationAfterDragPastIdealBounds) {
  UpdateDisplay("800x700");

  // Enable shelf auto-hide (shelf should still be visible until a widget is
  // shown).
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ASSERT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  ASSERT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Create a widget to make sure that the shelf does auto-hide.
  auto widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  ASSERT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Enable animations, and swipe up across the whole screen to bring up the
  // shelf.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const gfx::Point end(display_bounds.top_center());
  const base::TimeDelta kTimeDelta = base::Milliseconds(100);
  const int kNumScrollSteps = 4;
  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);
  ASSERT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  ASSERT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // The shelf should not be animating when the drag is complete.
  EXPECT_FALSE(GetShelfWidget()->GetLayer()->GetAnimator()->is_animating());
}

// Tests the shelf widget animations for hotseat transitions are stopped when
// the screen is locked.
TEST_F(ShelfWidgetTest, ScreenLockStopsHotseatTransitionAnimation) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  UpdateDisplay("800x700");

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create a window to transition to the in-app shelf.
  auto window = AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 800, 800));

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_TRUE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_TRUE(GetShelfWidget()
                  ->GetAnimatingBackground()
                  ->GetAnimator()
                  ->is_animating());

  GetSessionControllerClient()->SetSessionState(SessionState::LOCKED);

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());

  GetSessionControllerClient()->SetSessionState(SessionState::ACTIVE);

  // Ensure the ShelfWidget and the drag handle have the correct bounds and
  // visibility for in-app shelf when the screen is unlocked.
  ASSERT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  EXPECT_EQ(GetShelfWidget()->GetWindowBoundsInScreen(),
            gfx::Rect(0, 660, 800, 40));
  EXPECT_EQ(GetShelfWidget()->GetDragHandle()->bounds(),
            gfx::Rect(360, 18, 80, 4));
}

// Tests the shelf widget's opaque background gets reshown if hotseat transition
// from kShown state gets interrupted by a transition back to kShown state.
TEST_F(ShelfWidgetTest,
       OpaqueBackgroundReshownAfterTransitionFromHomeChangesBackToHome) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  UpdateDisplay("800x700");

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create a window to transition to the in-app shelf.
  auto window = AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 800, 800));

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_TRUE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_TRUE(GetShelfWidget()
                  ->GetAnimatingBackground()
                  ->GetAnimator()
                  ->is_animating());
  const gfx::Rect animating_background_bounds =
      GetShelfWidget()->GetAnimatingBackground()->bounds();
  EXPECT_NE(GetShelfWidget()->GetAnimatingBackground()->transform(),
            GetShelfWidget()->GetAnimatingBackground()->GetTargetTransform());

  // Go back home before the transition to in-app shelf finishes.
  window->Hide();

  // The shelf background should still be animating at this point, but to
  // different bounds.
  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_TRUE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_TRUE(GetShelfWidget()
                  ->GetAnimatingBackground()
                  ->GetAnimator()
                  ->is_animating());
  EXPECT_EQ(animating_background_bounds,
            GetShelfWidget()->GetAnimatingBackground()->bounds());
  // The original animation did not have a chance to update the transform yet,
  // so the current transform matches the original state, that matches the new
  // target state.
  EXPECT_EQ(GetShelfWidget()->GetAnimatingBackground()->transform(),
            GetShelfWidget()->GetAnimatingBackground()->GetTargetTransform());

  // Run the current animation to the end, end verify the opaque background is
  // reshown after ending tablet mode.
  GetShelfWidget()->GetAnimatingBackground()->GetAnimator()->StopAnimating();

  ASSERT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());

  ash::TabletModeControllerTestApi().LeaveTabletMode();

  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
}

class ShelfWidgetAfterLoginTest : public AshTestBase {
 public:
  ShelfWidgetAfterLoginTest() { set_start_session(false); }

  ShelfWidgetAfterLoginTest(const ShelfWidgetAfterLoginTest&) = delete;
  ShelfWidgetAfterLoginTest& operator=(const ShelfWidgetAfterLoginTest&) =
      delete;

  ~ShelfWidgetAfterLoginTest() override = default;

  void TestShelf(ShelfAlignment alignment,
                 ShelfAutoHideBehavior auto_hide_behavior,
                 ShelfVisibilityState expected_shelf_visibility_state,
                 ShelfAutoHideState expected_shelf_auto_hide_state) {
    // Simulate login.
    CreateUserSessions(1);

    // Simulate shelf settings being applied from profile prefs.
    Shelf* shelf = GetPrimaryShelf();
    ASSERT_NE(nullptr, shelf);
    shelf->SetAlignment(alignment);
    shelf->SetAutoHideBehavior(auto_hide_behavior);
    shelf->UpdateVisibilityState();

    // Ensure settings are applied correctly.
    EXPECT_EQ(alignment, shelf->alignment());
    EXPECT_EQ(auto_hide_behavior, shelf->auto_hide_behavior());
    EXPECT_EQ(expected_shelf_visibility_state, shelf->GetVisibilityState());
    EXPECT_EQ(expected_shelf_auto_hide_state, shelf->GetAutoHideState());
  }
};

TEST_F(ShelfWidgetAfterLoginTest, InitialValues) {
  // Ensure shelf components are created.
  Shelf* shelf = GetPrimaryShelf();
  ASSERT_NE(nullptr, shelf);
  ShelfWidget* shelf_widget = GetShelfWidget();
  ASSERT_NE(nullptr, shelf_widget);
  ASSERT_NE(nullptr, shelf_widget->shelf_view_for_testing());
  ASSERT_NE(nullptr, shelf_widget->GetLoginShelfView());
  ASSERT_NE(nullptr, shelf_widget->shelf_layout_manager());

  // Ensure settings are correct before login.
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAlignment::kBottomLocked, shelf->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlwaysHidden, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Simulate login.
  CreateUserSessions(1);

  // Ensure settings are correct after login.
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
  // "Hidden" is the default state when auto-hide is turned off.
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

TEST_F(ShelfWidgetAfterLoginTest, CreateAutoHideAlwaysShelf) {
  // The actual auto hide state is shown because there are no open windows.
  TestShelf(ShelfAlignment::kBottom, ShelfAutoHideBehavior::kAlways,
            SHELF_AUTO_HIDE, SHELF_AUTO_HIDE_SHOWN);
}

TEST_F(ShelfWidgetAfterLoginTest, CreateAutoHideNeverShelf) {
  // The auto hide state 'HIDDEN' is returned for any non-auto-hide behavior.
  TestShelf(ShelfAlignment::kLeft, ShelfAutoHideBehavior::kNever, SHELF_VISIBLE,
            SHELF_AUTO_HIDE_HIDDEN);
}

TEST_F(ShelfWidgetAfterLoginTest, CreateAutoHideAlwaysHideShelf) {
  // The auto hide state 'HIDDEN' is returned for any non-auto-hide behavior.
  TestShelf(ShelfAlignment::kRight, ShelfAutoHideBehavior::kAlwaysHidden,
            SHELF_HIDDEN, SHELF_AUTO_HIDE_HIDDEN);
}

TEST_F(ShelfWidgetAfterLoginTest, CreateLockedShelf) {
  // The auto hide state 'HIDDEN' is returned for any non-auto-hide behavior.
  TestShelf(ShelfAlignment::kBottomLocked, ShelfAutoHideBehavior::kNever,
            SHELF_VISIBLE, SHELF_AUTO_HIDE_HIDDEN);
}

class ShelfWidgetViewsVisibilityTest : public AshTestBase {
 public:
  ShelfWidgetViewsVisibilityTest() { set_start_session(false); }

  ShelfWidgetViewsVisibilityTest(const ShelfWidgetViewsVisibilityTest&) =
      delete;
  ShelfWidgetViewsVisibilityTest& operator=(
      const ShelfWidgetViewsVisibilityTest&) = delete;

  ~ShelfWidgetViewsVisibilityTest() override = default;

  enum ShelfVisibility {
    kNone,        // Shelf views hidden.
    kShelf,       // ShelfView visible.
    kLoginShelf,  // LoginShelfView visible.
  };

  void InitShelfVariables() {
    // Create setup with 2 displays primary and secondary.
    UpdateDisplay("800x600,800x600");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    ASSERT_EQ(2u, root_windows.size());
    primary_shelf_widget_ = Shelf::ForWindow(root_windows[0])->shelf_widget();
    ASSERT_NE(nullptr, primary_shelf_widget_);
    secondary_shelf_widget_ = Shelf::ForWindow(root_windows[1])->shelf_widget();
    ASSERT_NE(nullptr, secondary_shelf_widget_);
  }

  void ExpectVisible(session_manager::SessionState state,
                     ShelfVisibility primary_shelf_visibility,
                     ShelfVisibility secondary_shelf_visibility) {
    GetSessionControllerClient()->SetSessionState(state);
    EXPECT_EQ(primary_shelf_visibility == kNone,
              !primary_shelf_widget_->IsVisible());
    if (primary_shelf_visibility != kNone) {
      EXPECT_EQ(primary_shelf_visibility == kLoginShelf,
                primary_shelf_widget_->GetLoginShelfView()->GetVisible());
      EXPECT_EQ(primary_shelf_visibility == kShelf,
                primary_shelf_widget_->shelf_view_for_testing()->GetVisible());
    }
    EXPECT_EQ(secondary_shelf_visibility == kNone,
              !secondary_shelf_widget_->IsVisible());
    if (secondary_shelf_visibility != kNone) {
      EXPECT_EQ(secondary_shelf_visibility == kLoginShelf,
                secondary_shelf_widget_->GetLoginShelfView()->GetVisible());
      EXPECT_EQ(
          secondary_shelf_visibility == kShelf,
          secondary_shelf_widget_->shelf_view_for_testing()->GetVisible());
    }
  }

 private:
  raw_ptr<ShelfWidget, DanglingUntriaged> primary_shelf_widget_ = nullptr;
  raw_ptr<ShelfWidget, DanglingUntriaged> secondary_shelf_widget_ = nullptr;
};

TEST_F(ShelfWidgetViewsVisibilityTest, LoginViewsLockViews) {
  ASSERT_NO_FATAL_FAILURE(InitShelfVariables());

  ExpectVisible(SessionState::UNKNOWN, kNone /*primary*/, kNone /*secondary*/);
  ExpectVisible(SessionState::OOBE, kLoginShelf, kNone);
  ExpectVisible(SessionState::LOGIN_PRIMARY, kLoginShelf, kNone);

  SimulateUserLogin("user1@test.com");

  ExpectVisible(SessionState::LOGGED_IN_NOT_ACTIVE, kLoginShelf, kNone);
  ExpectVisible(SessionState::ACTIVE, kShelf, kShelf);
  ExpectVisible(SessionState::LOCKED, kLoginShelf, kNone);
  ExpectVisible(SessionState::ACTIVE, kShelf, kShelf);
  ExpectVisible(SessionState::LOGIN_SECONDARY, kLoginShelf, kNone);
  ExpectVisible(SessionState::ACTIVE, kShelf, kShelf);
}

class ShelfWidgetVirtualKeyboardTest : public AshTestBase {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    ASSERT_TRUE(keyboard::IsKeyboardEnabled());
    keyboard::test::WaitUntilLoaded();

    // These tests only apply to the floating virtual keyboard, as it is the
    // only case where both the virtual keyboard and the shelf are visible.
    const gfx::Rect keyboard_bounds(0, 0, 1, 1);
    keyboard_ui_controller()->SetContainerType(
        keyboard::ContainerType::kFloating, keyboard_bounds, base::DoNothing());
  }

  keyboard::KeyboardUIController* keyboard_ui_controller() {
    return keyboard::KeyboardUIController::Get();
  }
};

TEST_F(ShelfWidgetVirtualKeyboardTest, ClickingHidesVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // Times out if test fails.
  ASSERT_TRUE(keyboard::test::WaitUntilHidden());
}

TEST_F(ShelfWidgetVirtualKeyboardTest, TappingHidesVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint());
  generator->PressTouch();

  // Times out if test fails.
  ASSERT_TRUE(keyboard::test::WaitUntilHidden());
}

TEST_F(ShelfWidgetVirtualKeyboardTest, DoesNotHideLockedVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint());

  generator->ClickLeftButton();
  EXPECT_FALSE(keyboard::test::IsKeyboardHiding());

  generator->PressTouch();
  EXPECT_FALSE(keyboard::test::IsKeyboardHiding());
}

}  // namespace

}  // namespace ash
