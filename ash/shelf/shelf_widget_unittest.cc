// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/bind_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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
  UpdateDisplay("400x400");
  const int bottom_inset = 400 - ShelfConfig::Get()->shelf_size();
  {
    SCOPED_TRACE("Single Bottom");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_BOTTOM,
                          gfx::Rect(0, 0, 400, bottom_inset));
  }
  {
    SCOPED_TRACE("Single Locked");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          SHELF_ALIGNMENT_BOTTOM_LOCKED,
                          gfx::Rect(0, 0, 400, bottom_inset));
  }
  {
    SCOPED_TRACE("Single Right");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_RIGHT,
                          gfx::Rect(0, 0, bottom_inset, 400));
  }
  {
    SCOPED_TRACE("Single Left");
    TestLauncherAlignment(
        Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_LEFT,
        gfx::Rect(ShelfConfig::Get()->shelf_size(), 0, bottom_inset, 400));
  }
}

TEST_F(ShelfWidgetTest, TestAlignmentForMultipleDisplays) {
  UpdateDisplay("300x300,500x500");
  const int shelf_inset_first = 300 - ShelfConfig::Get()->shelf_size();
  const int shelf_inset_second = 500 - ShelfConfig::Get()->shelf_size();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  {
    SCOPED_TRACE("Primary Bottom");
    TestLauncherAlignment(root_windows[0], SHELF_ALIGNMENT_BOTTOM,
                          gfx::Rect(0, 0, 300, shelf_inset_first));
  }
  {
    SCOPED_TRACE("Primary Locked");
    TestLauncherAlignment(root_windows[0], SHELF_ALIGNMENT_BOTTOM_LOCKED,
                          gfx::Rect(0, 0, 300, shelf_inset_first));
  }
  {
    SCOPED_TRACE("Primary Right");
    TestLauncherAlignment(root_windows[0], SHELF_ALIGNMENT_RIGHT,
                          gfx::Rect(0, 0, shelf_inset_first, 300));
  }
  {
    SCOPED_TRACE("Primary Left");
    TestLauncherAlignment(
        root_windows[0], SHELF_ALIGNMENT_LEFT,
        gfx::Rect(ShelfConfig::Get()->shelf_size(), 0, shelf_inset_first, 300));
  }
  {
    SCOPED_TRACE("Secondary Bottom");
    TestLauncherAlignment(root_windows[1], SHELF_ALIGNMENT_BOTTOM,
                          gfx::Rect(300, 0, 500, shelf_inset_second));
  }
  {
    SCOPED_TRACE("Secondary Locked");
    TestLauncherAlignment(root_windows[1], SHELF_ALIGNMENT_BOTTOM_LOCKED,
                          gfx::Rect(300, 0, 500, shelf_inset_second));
  }
  {
    SCOPED_TRACE("Secondary Right");
    TestLauncherAlignment(root_windows[1], SHELF_ALIGNMENT_RIGHT,
                          gfx::Rect(300, 0, shelf_inset_second, 500));
  }
  {
    SCOPED_TRACE("Secondary Left");
    TestLauncherAlignment(root_windows[1], SHELF_ALIGNMENT_LEFT,
                          gfx::Rect(300 + ShelfConfig::Get()->shelf_size(), 0,
                                    shelf_inset_second, 500));
  }
}

// Makes sure the shelf is initially sized correctly.
TEST_F(ShelfWidgetTest, LauncherInitiallySized) {
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
  const int nav_width =
      shelf_widget->navigation_widget()->GetWindowBoundsInScreen().width();
  const int hotseat_width =
      shelf_widget->hotseat_widget()->GetWindowBoundsInScreen().width();
  const int margins = ShelfConfig::Get()->home_button_edge_spacing() +
                      ShelfConfig::Get()->app_icon_group_margin();
  EXPECT_EQ(status_width, total_width - nav_width - hotseat_width - margins);
}

// Verifies when the shell is deleted with a full screen window we don't crash.
TEST_F(ShelfWidgetTest, DontReferenceShelfAfterDeletion) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
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
      shelf_widget1->navigation_widget()->GetWindowBoundsInScreen().width();
  const int hotseat_width1 =
      shelf_widget1->hotseat_widget()->GetWindowBoundsInScreen().width();
  const int margins = ShelfConfig::Get()->home_button_edge_spacing() +
                      ShelfConfig::Get()->app_icon_group_margin();

  const int total_width2 =
      screen_util::GetDisplayBoundsWithShelf(shelf_widget2->GetNativeWindow())
          .width();
  const int nav_width2 =
      shelf_widget2->navigation_widget()->GetWindowBoundsInScreen().width();
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

// Tests that the shelf lets mouse-events close to the edge fall through to the
// window underneath.
TEST_F(ShelfWidgetTest, ShelfEdgeOverlappingWindowHitTestMouse) {
  UpdateDisplay("400x400");
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
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(shelf_bounds.height() - kOverlapSize,
                            shelf_bounds.y() - kWindowHeight + kOverlapSize,
                            kWindowWidth, kWindowHeight);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(std::move(params));
  // Explicitly set the bounds which will allow the widget to overlap the shelf.
  widget->SetBounds(params.bounds);
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
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(widget->GetNativeWindow(), target);
  }

  // Change shelf alignment to verify that the targeter insets are updated.
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  shelf_layout_manager->LayoutShelf();
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  {
    // Create a mouse-event targeting the right edge of the shelf widget. The
    // window-targeter should find |widget| as the target (instead of the
    // shelf).
    gfx::Point event_location(shelf_bounds.right() - 1, widget_bounds.y() + 5);
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(widget->GetNativeWindow(), target);
  }

  // Now restore shelf alignment (bottom) and auto-hide (hidden) the shelf.
  shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_layout_manager->LayoutShelf();
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
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(shelf_widget->GetNativeWindow(), target);
  }
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
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
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
    ui::TouchEvent touch(
        ui::ET_TOUCH_PRESSED, event_location, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
    EXPECT_NE(shelf_widget->GetNativeWindow(),
              targeter->FindTargetForEvent(root, &touch));
  }

  // Now auto-hide (hidden) the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_layout_manager->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_layout_manager->auto_hide_state());
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(!shelf_bounds.IsEmpty());

  // Touch just over the shelf again. This time, the targeter should find the
  // shelf as the target.
  {
    gfx::Point event_location(20, shelf_bounds.y() - 1);
    ui::TouchEvent touch(
        ui::ET_TOUCH_PRESSED, event_location, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
    EXPECT_EQ(shelf_widget->GetNativeWindow(),
              targeter->FindTargetForEvent(root, &touch));
  }
}

class ShelfWidgetAfterLoginTest : public AshTestBase {
 public:
  ShelfWidgetAfterLoginTest() { set_start_session(false); }
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

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfWidgetAfterLoginTest);
};

TEST_F(ShelfWidgetAfterLoginTest, InitialValues) {
  // Ensure shelf components are created.
  Shelf* shelf = GetPrimaryShelf();
  ASSERT_NE(nullptr, shelf);
  ShelfWidget* shelf_widget = GetShelfWidget();
  ASSERT_NE(nullptr, shelf_widget);
  ASSERT_NE(nullptr, shelf_widget->shelf_view_for_testing());
  ASSERT_NE(nullptr, shelf_widget->login_shelf_view());
  ASSERT_NE(nullptr, shelf_widget->shelf_layout_manager());

  // Ensure settings are correct before login.
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_ALWAYS_HIDDEN, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Simulate login.
  CreateUserSessions(1);

  // Ensure settings are correct after login.
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  // "Hidden" is the default state when auto-hide is turned off.
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

TEST_F(ShelfWidgetAfterLoginTest, CreateAutoHideAlwaysShelf) {
  // The actual auto hide state is shown because there are no open windows.
  TestShelf(SHELF_ALIGNMENT_BOTTOM, SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            SHELF_AUTO_HIDE, SHELF_AUTO_HIDE_SHOWN);
}

TEST_F(ShelfWidgetAfterLoginTest, CreateAutoHideNeverShelf) {
  // The auto hide state 'HIDDEN' is returned for any non-auto-hide behavior.
  TestShelf(SHELF_ALIGNMENT_LEFT, SHELF_AUTO_HIDE_BEHAVIOR_NEVER, SHELF_VISIBLE,
            SHELF_AUTO_HIDE_HIDDEN);
}

TEST_F(ShelfWidgetAfterLoginTest, CreateAutoHideAlwaysHideShelf) {
  // The auto hide state 'HIDDEN' is returned for any non-auto-hide behavior.
  TestShelf(SHELF_ALIGNMENT_RIGHT, SHELF_AUTO_HIDE_ALWAYS_HIDDEN, SHELF_HIDDEN,
            SHELF_AUTO_HIDE_HIDDEN);
}

TEST_F(ShelfWidgetAfterLoginTest, CreateLockedShelf) {
  // The auto hide state 'HIDDEN' is returned for any non-auto-hide behavior.
  TestShelf(SHELF_ALIGNMENT_BOTTOM_LOCKED, SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            SHELF_VISIBLE, SHELF_AUTO_HIDE_HIDDEN);
}

class ShelfWidgetViewsVisibilityTest : public AshTestBase {
 public:
  ShelfWidgetViewsVisibilityTest() { set_start_session(false); }
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
                primary_shelf_widget_->login_shelf_view()->GetVisible());
      EXPECT_EQ(primary_shelf_visibility == kShelf,
                primary_shelf_widget_->shelf_view_for_testing()->GetVisible());
    }
    EXPECT_EQ(secondary_shelf_visibility == kNone,
              !secondary_shelf_widget_->IsVisible());
    if (secondary_shelf_visibility != kNone) {
      EXPECT_EQ(secondary_shelf_visibility == kLoginShelf,
                secondary_shelf_widget_->login_shelf_view()->GetVisible());
      EXPECT_EQ(
          secondary_shelf_visibility == kShelf,
          secondary_shelf_widget_->shelf_view_for_testing()->GetVisible());
    }
  }

 private:
  ShelfWidget* primary_shelf_widget_ = nullptr;
  ShelfWidget* secondary_shelf_widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShelfWidgetViewsVisibilityTest);
};

TEST_F(ShelfWidgetViewsVisibilityTest, LoginWebUiLockViews) {
  // Enable web UI login.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kShowWebUiLogin);
  ASSERT_NO_FATAL_FAILURE(InitShelfVariables());

  // Both shelf views are hidden when session state hasn't been initialized.
  ExpectVisible(SessionState::UNKNOWN, kNone /*primary*/, kNone /*secondary*/);
  // Web UI login is used, so views shelf is not visible during login.
  ExpectVisible(SessionState::OOBE, kLoginShelf, kNone);
  ExpectVisible(SessionState::LOGIN_PRIMARY, kLoginShelf, kNone);

  SimulateUserLogin("user1@test.com");

  ExpectVisible(SessionState::LOGGED_IN_NOT_ACTIVE, kNone, kNone);
  ExpectVisible(SessionState::ACTIVE, kShelf, kShelf);
  ExpectVisible(SessionState::LOCKED, kLoginShelf, kNone);
  ExpectVisible(SessionState::ACTIVE, kShelf, kShelf);
  ExpectVisible(SessionState::LOGIN_SECONDARY, kLoginShelf, kNone);
  ExpectVisible(SessionState::ACTIVE, kShelf, kShelf);
}

TEST_F(ShelfWidgetViewsVisibilityTest, LoginViewsLockViews) {
  // Enable views login. Views lock enabled by default.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(ash::features::kViewsLogin);

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
  ASSERT_TRUE(keyboard::WaitUntilShown());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // Times out if test fails.
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

TEST_F(ShelfWidgetVirtualKeyboardTest, TappingHidesVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint());
  generator->PressTouch();

  // Times out if test fails.
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

TEST_F(ShelfWidgetVirtualKeyboardTest, DoesNotHideLockedVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint());

  generator->ClickLeftButton();
  EXPECT_FALSE(keyboard::IsKeyboardHiding());

  generator->PressTouch();
  EXPECT_FALSE(keyboard::IsKeyboardHiding());
}

}  // namespace

}  // namespace ash
