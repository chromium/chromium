// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/overview/overview_button_tray.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/login_status.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/window_factory.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "services/ws/public/cpp/input_devices/input_device_client_test_api.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

const char kTrayOverview[] = "Tray_Overview";

OverviewButtonTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
      ->overview_button_tray();
}

OverviewButtonTray* GetSecondaryTray() {
  return StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget()
      ->overview_button_tray();
}

ui::GestureEvent CreateTapEvent(
    base::TimeDelta delta_from_start = base::TimeDelta()) {
  return ui::GestureEvent(0, 0, 0, base::TimeTicks() + delta_from_start,
                          ui::GestureEventDetails(ui::ET_GESTURE_TAP));
}

// Helper function to perform a double tap on the overview button tray.
void PerformDoubleTap() {
  ui::GestureEvent tap = CreateTapEvent();
  GetTray()->PerformAction(tap);
  GetTray()->PerformAction(tap);
}

}  // namespace

class OverviewButtonTrayTest : public AshTestBase {
 public:
  OverviewButtonTrayTest() = default;
  ~OverviewButtonTrayTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kUseFirstDisplayAsInternal);

    AshTestBase::SetUp();

    ws::InputDeviceClientTestApi().SetKeyboardDevices({ui::InputDevice(
        3, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard")});
    base::RunLoop().RunUntilIdle();
  }

  void NotifySessionStateChanged() {
    GetTray()->OnSessionStateChanged(
        Shell::Get()->session_controller()->GetSessionState());
  }

 protected:
  views::ImageView* GetImageView(OverviewButtonTray* tray) {
    return tray->icon_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewButtonTrayTest);
};

// Ensures that creation doesn't cause any crashes and adds the image icon.
TEST_F(OverviewButtonTrayTest, BasicConstruction) {
  EXPECT_TRUE(GetImageView(GetTray()));
}

// Test that tablet mode toggle changes visibility.
// OverviewButtonTray should only be visible when TabletMode is enabled.
// By default the system should not have TabletMode enabled.
TEST_F(OverviewButtonTrayTest, TabletModeObserverOnTabletModeToggled) {
  ASSERT_FALSE(GetTray()->visible());
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetTray()->visible());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetTray()->visible());
}

// Tests that activating this control brings up window selection mode.
TEST_F(OverviewButtonTrayTest, PerformAction) {
  ASSERT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Overview Mode only works when there is a window
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  GetTray()->PerformAction(CreateTapEvent());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Verify tapping on the button again closes overview mode.
  GetTray()->PerformAction(CreateTapEvent());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
}

TEST_F(OverviewButtonTrayTest, PerformDoubleTapAction) {
  ASSERT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Add two windows and activate the second one to test quick switch.
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  wm::ActivateWindow(window2.get());
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Verify that after double tapping, we have switched to window 1.
  PerformDoubleTap();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Verify that if we double tap on the window selection page, it acts as two
  // taps, and ends up on the window selection page again.
  ui::GestureEvent tap = CreateTapEvent();
  ASSERT_TRUE(wm::IsActiveWindow(window1.get()));
  GetTray()->PerformAction(tap);
  ASSERT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  PerformDoubleTap();
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Verify that if we minimize a window, double tapping the overlay tray button
  // will bring up the window, and it should be the active window.
  GetTray()->PerformAction(tap);
  ASSERT_TRUE(!Shell::Get()->window_selector_controller()->IsSelecting());
  ASSERT_TRUE(wm::IsActiveWindow(window1.get()));
  wm::GetWindowState(window2.get())->Minimize();
  ASSERT_EQ(window2->layer()->GetTargetOpacity(), 0.0);
  PerformDoubleTap();
  EXPECT_EQ(window2->layer()->GetTargetOpacity(), 1.0);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that tapping on the control will record the user action Tray_Overview.
TEST_F(OverviewButtonTrayTest, TrayOverviewUserAction) {
  ASSERT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  // With one window present, tapping on the control to enter overview mode
  // should record the user action.
  base::UserActionTester user_action_tester;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  GetTray()->PerformAction(
      CreateTapEvent(OverviewButtonTray::kDoubleTapThresholdMs));
  ASSERT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kTrayOverview));

  // Tapping on the control to exit overview mode should record the
  // user action.
  GetTray()->PerformAction(
      CreateTapEvent(OverviewButtonTray::kDoubleTapThresholdMs * 2));
  ASSERT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(2, user_action_tester.GetActionCount(kTrayOverview));
}

// Tests that a second OverviewButtonTray has been created, and only shows
// when TabletMode has been enabled,  when we are using multiple displays.
// By default the DisplayManger is in extended mode.
TEST_F(OverviewButtonTrayTest, DisplaysOnBothDisplays) {
  UpdateDisplay("400x400,200x200");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetTray()->visible());
  EXPECT_FALSE(GetSecondaryTray()->visible());
  TabletModeControllerTestApi().EnterTabletMode();
  base::RunLoop().RunUntilIdle();
  // DisplayConfigurationObserver enables mirror mode when tablet mode is
  // enabled. Disable mirror mode to test tablet mode with multiple displays.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetTray()->visible());
  EXPECT_TRUE(GetSecondaryTray()->visible());
}

// Tests if Maximize Mode is enabled before a secondary display is attached
// that the second OverviewButtonTray should be created in a visible state.
// TODO(oshima/jonross): This fails with RunIntilIdle after UpdateDisplay,
// so disabling mirror mode after enabling tablet mode does not work.
// https://crbug.com/798857.
TEST_F(OverviewButtonTrayTest, DISABLED_SecondaryTrayCreatedVisible) {
  TabletModeControllerTestApi().EnterTabletMode();
  UpdateDisplay("400x400,200x200");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetSecondaryTray()->visible());
}

// Tests that the tray loses visibility when a user logs out, and that it
// regains visibility when a user logs back in.
TEST_F(OverviewButtonTrayTest, VisibilityChangesForLoginStatus) {
  TabletModeControllerTestApi().EnterTabletMode();
  ClearLogin();
  Shell::Get()->UpdateAfterLoginStatusChange(LoginStatus::NOT_LOGGED_IN);
  EXPECT_FALSE(GetTray()->visible());
  CreateUserSessions(1);
  Shell::Get()->UpdateAfterLoginStatusChange(LoginStatus::USER);
  EXPECT_TRUE(GetTray()->visible());
  SetUserAddingScreenRunning(true);
  NotifySessionStateChanged();
  EXPECT_FALSE(GetTray()->visible());
  SetUserAddingScreenRunning(false);
  NotifySessionStateChanged();
  EXPECT_TRUE(GetTray()->visible());
}

// Tests that the tray only renders as active while selection is ongoing. Any
// dismissal of overview mode clears the active state.
TEST_F(OverviewButtonTrayTest, ActiveStateOnlyDuringOverviewMode) {
  ASSERT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  ASSERT_FALSE(GetTray()->is_active());

  // Overview Mode only works when there is a window
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));

  EXPECT_TRUE(Shell::Get()->window_selector_controller()->ToggleOverview());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_TRUE(GetTray()->is_active());

  EXPECT_TRUE(Shell::Get()->window_selector_controller()->ToggleOverview());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_FALSE(GetTray()->is_active());
}

// Test that a hide animation can complete.
TEST_F(OverviewButtonTrayTest, HideAnimationAlwaysCompletes) {
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetTray()->visible());
  GetTray()->SetVisible(false);
  EXPECT_FALSE(GetTray()->visible());
}

// Test that when a hide animation is aborted via deletion, the
// OverviewButton is still hidden.
TEST_F(OverviewButtonTrayTest, HideAnimationAlwaysCompletesOnDelete) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Long duration for hide animation, to allow it to be interrupted.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> hide_duration(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  GetTray()->SetVisible(false);

  aura::Window* root_window = Shell::GetRootWindowForDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  // Colone and delete the old layer tree.
  std::unique_ptr<ui::LayerTreeOwner> old_layer_tree_owner =
      ::wm::RecreateLayers(root_window);
  old_layer_tree_owner.reset();

  EXPECT_FALSE(GetTray()->visible());
}

// Tests that the overview button becomes visible when the user enters
// tablet mode with a system modal window open, and that it hides once
// the user exits tablet mode.
TEST_F(OverviewButtonTrayTest, VisibilityChangesForSystemModalWindow) {
  // TODO(jonross): When CreateTestWindow*() have been unified, use the
  // appropriate method to replace this setup. (crbug.com/483503)
  std::unique_ptr<aura::Window> window = window_factory::NewWindow();
  window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->Show();
  ParentWindowInPrimaryRootWindow(window.get());

  ASSERT_TRUE(Shell::IsSystemModalWindowOpen());
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetTray()->visible());
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetTray()->visible());
}

// Verify that quick switch works properly when one of the windows has a
// transient child.
TEST_F(OverviewButtonTrayTest, TransientChildQuickSwitch) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_POPUP);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();

  // Add |window2| as a transient child of |window1|, and focus |window1|.
  ::wm::AddTransientChild(window1.get(), window2.get());
  ::wm::ActivateWindow(window3.get());
  ::wm::ActivateWindow(window2.get());
  ::wm::ActivateWindow(window1.get());

  // Verify that after double tapping, we have switched to |window3|, even
  // though |window2| is more recently used.
  PerformDoubleTap();
  EXPECT_EQ(window3.get(), wm::GetActiveWindow());
}

// Verify that quick switch works properly when in split view mode.
TEST_F(OverviewButtonTrayTest, SplitviewModeQuickSwitch) {
  // Splitview is only available in tablet mode.
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();

  // Enter splitview mode. Snap |window1| to the left, this will be the default
  // splitview window.
  Shell::Get()->window_selector_controller()->ToggleOverview();
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  split_view_controller->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller->SnapWindow(window2.get(), SplitViewController::RIGHT);
  ASSERT_EQ(window1.get(), split_view_controller->GetDefaultSnappedWindow());
  EXPECT_EQ(window2.get(), wm::GetActiveWindow());

  // Verify that after double tapping, we have switched to |window3|, even
  // though |window1| is more recently used.
  PerformDoubleTap();
  EXPECT_EQ(window3.get(), split_view_controller->right_window());
  EXPECT_EQ(window3.get(), wm::GetActiveWindow());

  // Focus |window1|. Verify that after double tapping, |window2| is the on the
  // right side for splitview.
  wm::ActivateWindow(window1.get());
  PerformDoubleTap();
  EXPECT_EQ(window2.get(), split_view_controller->right_window());
  EXPECT_EQ(window2.get(), wm::GetActiveWindow());

  split_view_controller->EndSplitView();
}

// Tests that the tray remains visible when leaving tablet mode due to external
// mouse being connected.
TEST_F(OverviewButtonTrayTest, LeaveTabletModeBecauseExternalMouse) {
  TabletModeControllerTestApi().OpenLidToAngle(315.0f);
  EXPECT_TRUE(TabletModeControllerTestApi().IsTabletModeStarted());
  ASSERT_TRUE(GetTray()->visible());

  TabletModeControllerTestApi().AttachExternalMouse();
  EXPECT_FALSE(TabletModeControllerTestApi().IsTabletModeStarted());
  EXPECT_TRUE(GetTray()->visible());
}

}  // namespace ash
