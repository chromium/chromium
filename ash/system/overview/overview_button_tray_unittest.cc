// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/overview/overview_button_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/login_status.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/types/event_type.h"
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

}  // namespace

class OverviewButtonTrayTest : public AshTestBase {
 public:
  OverviewButtonTrayTest() {
    scoped_features_.InitAndDisableFeature(
        features::kHideShelfControlsInTabletMode);
  }
  OverviewButtonTrayTest(const OverviewButtonTrayTest& other) = delete;
  OverviewButtonTrayTest& operator=(const OverviewButtonTrayTest& other) =
      delete;
  ~OverviewButtonTrayTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kUseFirstDisplayAsInternal);

    AshTestBase::SetUp();

    ui::DeviceDataManagerTestApi().SetKeyboardDevices({ui::KeyboardDevice(
        3, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard")});
    base::RunLoop().RunUntilIdle();
    // State change is asynchronous on the device. Do the same
    // in this unit tests.
    TabletModeController::SetUseScreenshotForTest(true);
  }

  void NotifySessionStateChanged() {
    GetTray()->OnSessionStateChanged(
        Shell::Get()->session_controller()->GetSessionState());
  }

  // Helper function to perform a double tap on the overview button tray.
  void PerformDoubleTap() {
    GestureTapOn(GetTray());
    GestureTapOn(GetTray());
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

 protected:
  views::ImageView* GetImageView(OverviewButtonTray* tray) {
    return tray->icon_;
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Ensures that creation doesn't cause any crashes and adds the image icon.
TEST_F(OverviewButtonTrayTest, BasicConstruction) {
  EXPECT_TRUE(GetImageView(GetTray()));
}

// Test that tablet mode toggle changes visibility.
// OverviewButtonTray should only be visible when TabletMode is enabled.
// By default the system should not have TabletMode enabled.
TEST_F(OverviewButtonTrayTest, VisibilityTest) {
  ASSERT_FALSE(GetTray()->GetVisible());
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(GetTray()->GetVisible());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetTray()->GetVisible());
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // When there is an window, it'll take an screenshot and
  // switch becomes asynchronous, but the display tablet state is synchronously
  // updated.
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));

  ASSERT_FALSE(GetTray()->GetVisible());
  TabletMode::Waiter waiter(/*enable=*/true);
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());

  waiter.Wait();

  EXPECT_TRUE(GetTray()->GetVisible());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());
}

// Tests that activating this control brings up window selection mode.
TEST_F(OverviewButtonTrayTest, PerformAction) {
  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // Overview Mode only works when there is a window
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  GetTray()->SetVisiblePreferred(true);
  GestureTapOn(GetTray());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Verify tapping on the button again closes overview mode.
  GestureTapOn(GetTray());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // Test in tablet mode.
  TabletMode::Waiter waiter(/*enable=*/true);
  TabletModeControllerTestApi().EnterTabletMode();
  waiter.Wait();

  GestureTapOn(GetTray());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Verify tapping on the button again closes overview mode.
  GestureTapOn(GetTray());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

TEST_F(OverviewButtonTrayTest, PerformDoubleTapAction) {
  TabletModeControllerTestApi().EnterTabletMode();

  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

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
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // Verify that if we double tap on the window selection page, it acts as two
  // taps, and ends up on the window selection page again.
  ASSERT_TRUE(wm::IsActiveWindow(window1.get()));
  GestureTapOn(GetTray());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  PerformDoubleTap();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Verify that if we minimize a window, double tapping the overlay tray button
  // will bring up the window, and it should be the active window.
  GestureTapOn(GetTray());
  ASSERT_TRUE(!Shell::Get()->overview_controller()->InOverviewSession());
  ASSERT_TRUE(wm::IsActiveWindow(window1.get()));
  WindowState::Get(window2.get())->Minimize();
  ASSERT_EQ(window2->layer()->GetTargetOpacity(), 0.0);
  PerformDoubleTap();
  EXPECT_EQ(window2->layer()->GetTargetOpacity(), 1.0);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Verify that if all windows are minimized, double tapping the tray will have
  // no effect.
  ASSERT_TRUE(!Shell::Get()->overview_controller()->InOverviewSession());
  WindowState::Get(window1.get())->Minimize();
  WindowState::Get(window2.get())->Minimize();
  PerformDoubleTap();
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
}

// Tests that tapping on the control will record the user action Tray_Overview.
TEST_F(OverviewButtonTrayTest, TrayOverviewUserAction) {
  TabletModeControllerTestApi().EnterTabletMode();

  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // With one window present, tapping on the control to enter overview mode
  // should record the user action.
  base::UserActionTester user_action_tester;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  GestureTapOn(GetTray());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kTrayOverview));

  // Tapping on the control to exit overview mode should record the
  // user action.
  GestureTapOn(GetTray());
  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(2, user_action_tester.GetActionCount(kTrayOverview));
}

// Tests that a second OverviewButtonTray has been created, and only shows
// when TabletMode has been enabled,  when we are using multiple displays.
// By default the DisplayManger is in extended mode.
TEST_F(OverviewButtonTrayTest, DisplaysOnBothDisplays) {
  UpdateDisplay("500x400,300x200");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetTray()->GetVisible());
  EXPECT_FALSE(GetSecondaryTray()->GetVisible());
  TabletModeControllerTestApi().EnterTabletMode();
  base::RunLoop().RunUntilIdle();
  // DisplayConfigurationObserver enables mirror mode when tablet mode is
  // enabled. Disable mirror mode to test tablet mode with multiple displays.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetTray()->GetVisible());
  EXPECT_TRUE(GetSecondaryTray()->GetVisible());
}

// Tests if Maximize Mode is enabled before a secondary display is attached
// that the second OverviewButtonTray should be created in a visible state.
// TODO(oshima/jonross): This fails with RunIntilIdle after UpdateDisplay,
// so disabling mirror mode after enabling tablet mode does not work.
// https://crbug.com/798857.
TEST_F(OverviewButtonTrayTest, DISABLED_SecondaryTrayCreatedVisible) {
  TabletModeControllerTestApi().EnterTabletMode();
  UpdateDisplay("500x400,300x200");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetSecondaryTray()->GetVisible());
}

// Tests that the tray loses visibility when a user logs out, and that it
// regains visibility when a user logs back in.
TEST_F(OverviewButtonTrayTest, VisibilityChangesForLoginStatus) {
  TabletModeControllerTestApi().EnterTabletMode();
  ClearLogin();
  Shell::Get()->UpdateAfterLoginStatusChange(LoginStatus::NOT_LOGGED_IN);
  EXPECT_FALSE(GetTray()->GetVisible());
  CreateUserSessions(1);
  Shell::Get()->UpdateAfterLoginStatusChange(LoginStatus::USER);
  EXPECT_TRUE(GetTray()->GetVisible());
  SetUserAddingScreenRunning(true);
  NotifySessionStateChanged();
  EXPECT_FALSE(GetTray()->GetVisible());
  SetUserAddingScreenRunning(false);
  NotifySessionStateChanged();
  EXPECT_TRUE(GetTray()->GetVisible());
}

// Tests that the tray only renders as active while selection is ongoing. Any
// dismissal of overview mode clears the active state.
TEST_F(OverviewButtonTrayTest, ActiveStateOnlyDuringOverviewMode) {
  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  ASSERT_FALSE(GetTray()->is_active());

  // Overview Mode only works when there is a window
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));

  EXPECT_TRUE(EnterOverview());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(GetTray()->is_active());

  EXPECT_TRUE(ExitOverview());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(GetTray()->is_active());
}

// Test that a hide animation can complete.
TEST_F(OverviewButtonTrayTest, HideAnimationAlwaysCompletes) {
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetTray()->GetVisible());
  GetTray()->SetVisiblePreferred(false);
  EXPECT_FALSE(GetTray()->GetVisible());
}

// Test that when a hide animation is aborted via deletion, the
// OverviewButton is still hidden.
TEST_F(OverviewButtonTrayTest, HideAnimationAlwaysCompletesOnDelete) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Long duration for hide animation, to allow it to be interrupted.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> hide_duration(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  GetTray()->SetVisiblePreferred(false);

  aura::Window* root_window = Shell::GetRootWindowForDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  // Colone and delete the old layer tree.
  std::unique_ptr<ui::LayerTreeOwner> old_layer_tree_owner =
      ::wm::RecreateLayers(root_window);
  old_layer_tree_owner.reset();

  EXPECT_FALSE(GetTray()->GetVisible());
}

// Tests that the overview button becomes visible when the user enters
// tablet mode with a system modal window open, and that it hides once
// the user exits tablet mode.
TEST_F(OverviewButtonTrayTest, VisibilityChangesForSystemModalWindow) {
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_NORMAL);
  window->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kSystem);
  window->Show();
  ParentWindowInPrimaryRootWindow(window.get());

  ASSERT_TRUE(Shell::IsSystemModalWindowOpen());
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetTray()->GetVisible());
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetTray()->GetVisible());
}

// Verify that quick switch works properly when one of the windows has a
// transient child.
TEST_F(OverviewButtonTrayTest, TransientChildQuickSwitch) {
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_POPUP);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();

  // Add |window2| as a transient child of |window1|, and focus |window1|.
  wm::AddTransientChild(window1.get(), window2.get());
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  // Verify that after double tapping, we have switched to |window3|, even
  // though |window2| is more recently used.
  PerformDoubleTap();
  EXPECT_EQ(window3.get(), window_util::GetActiveWindow());
}

// Verify that quick switch works properly when in split view mode.
TEST_F(OverviewButtonTrayTest, SplitviewModeQuickSwitch) {
  // Splitview is only available in tablet mode.
  TabletModeControllerTestApi().EnterTabletMode();

  // We want the order in the MRU list to be |window2|, |window1|, |window3|.
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  // Enter splitview mode. Snap |window1| to the left, this will be the default
  // splitview window.
  EnterOverview();
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);
  ASSERT_EQ(window1.get(), split_view_controller()->GetDefaultSnappedWindow());

  // Verify that after double tapping, we have switched to |window3|, even
  // though |window1| is more recently used.
  PerformDoubleTap();
  EXPECT_EQ(window3.get(), split_view_controller()->secondary_window());
  EXPECT_EQ(window3.get(), window_util::GetActiveWindow());

  // Focus |window1|. Verify that after double tapping, |window2| is the on the
  // right side for splitview.
  wm::ActivateWindow(window1.get());
  PerformDoubleTap();
  EXPECT_EQ(window2.get(), split_view_controller()->secondary_window());
  EXPECT_EQ(window2.get(), window_util::GetActiveWindow());

  split_view_controller()->EndSplitView();
}

// Tests that the tray remains visible when leaving tablet mode due to external
// mouse being connected.
TEST_F(OverviewButtonTrayTest, LeaveTabletModeBecauseExternalMouse) {
  // Make sure no mouse is attached as that prevents tablet mode.
  TabletModeControllerTestApi().DetachAllMice();

  TabletModeControllerTestApi().OpenLidToAngle(315.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  ASSERT_TRUE(GetTray()->GetVisible());

  TabletModeControllerTestApi().AttachExternalMouse();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(GetTray()->GetVisible());
}

// Using the developers keyboard shortcut to enable tablet mode should force the
// overview tray button visible, even though the events are not blocked.
TEST_F(OverviewButtonTrayTest, ForDevTabletModeForcesTheButtonShown) {
  // Make sure no mouse is attached as that prevents tablet mode.
  TabletModeControllerTestApi().DetachAllMice();

  Shell::Get()->tablet_mode_controller()->SetEnabledForDev(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(TabletModeControllerTestApi().AreEventsBlocked());
  EXPECT_TRUE(GetTray()->GetVisible());

  Shell::Get()->tablet_mode_controller()->SetEnabledForDev(false);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());

  // When there is a window, a screenshot will be taken before shelf enters
  // tablet mode state.
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));

  EXPECT_FALSE(GetTray()->GetVisible());
  TabletMode::Waiter waiter(/*enable=*/true);
  Shell::Get()->tablet_mode_controller()->SetEnabledForDev(true);
  EXPECT_FALSE(GetTray()->GetVisible());

  waiter.Wait();

  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(GetTray()->GetVisible());

  // When disabling tablet mode, shelf state updates synchronously.
  Shell::Get()->tablet_mode_controller()->SetEnabledForDev(false);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());
}

enum class TestAccessibilityFeature {
  kNone,
  kSpokenFeedback,
  kAutoclick,
  kSwitchAccess
};

// Tests overview button tray item behavior when shelf navigation buttons (which
// includes overview button) are hidden.
class OverviewButtonTrayWithShelfControlsHiddenTest
    : public AshTestBase,
      public testing::WithParamInterface<TestAccessibilityFeature> {
 public:
  OverviewButtonTrayWithShelfControlsHiddenTest() {
    scoped_features_.InitAndEnableFeature(
        features::kHideShelfControlsInTabletMode);
  }
  OverviewButtonTrayWithShelfControlsHiddenTest(
      const OverviewButtonTrayWithShelfControlsHiddenTest& other) = delete;
  OverviewButtonTrayWithShelfControlsHiddenTest& operator=(
      const OverviewButtonTrayWithShelfControlsHiddenTest& other) = delete;
  ~OverviewButtonTrayWithShelfControlsHiddenTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kUseFirstDisplayAsInternal);

    AshTestBase::SetUp();

    // State change is asynchronous on the device. Do the same
    // in this unit tests.
    TabletModeController::SetUseScreenshotForTest(true);
  }

  void SetTestA11yFeatureEnabled(bool enabled) {
    switch (GetParam()) {
      case TestAccessibilityFeature::kNone:
        break;
      case TestAccessibilityFeature::kSpokenFeedback:
        Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
            enabled, A11Y_NOTIFICATION_NONE);
        break;
      case TestAccessibilityFeature::kAutoclick:
        Shell::Get()->accessibility_controller()->autoclick().SetEnabled(
            enabled);
        break;
      case TestAccessibilityFeature::kSwitchAccess:
        Shell::Get()->accessibility_controller()->switch_access().SetEnabled(
            enabled);
        Shell::Get()
            ->accessibility_controller()
            ->DisableSwitchAccessDisableConfirmationDialogTesting();
        break;
    }
  }

  bool HasTestingAccessibilityFeature() const {
    return GetParam() != TestAccessibilityFeature::kNone;
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    OverviewButtonTrayWithShelfControlsHiddenTest,
    ::testing::Values(TestAccessibilityFeature::kNone,
                      TestAccessibilityFeature::kSpokenFeedback,
                      TestAccessibilityFeature::kAutoclick,
                      TestAccessibilityFeature::kSwitchAccess));

// Test that tablet mode toggle does not change overview button visibility if
// kHideShelfControlsInTabletMode feature is enabled, and buttons are not forced
// by an accessibility feature.
TEST_P(OverviewButtonTrayWithShelfControlsHiddenTest, VisibilityTest) {
  ASSERT_FALSE(GetTray()->GetVisible());
  SetTestA11yFeatureEnabled(true);

  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_EQ(HasTestingAccessibilityFeature(), GetTray()->GetVisible());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetTray()->GetVisible());
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // When there is an window, it'll take an screenshot and the tablet mode
  // switch becomes asynchronous.
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));

  ASSERT_FALSE(GetTray()->GetVisible());

  TabletMode::Waiter waiter(/*enable=*/true);
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());

  waiter.Wait();

  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_EQ(HasTestingAccessibilityFeature(), GetTray()->GetVisible());

  // Disable the accessibility feature while in tablet mode - the button should
  // be hidden.
  SetTestA11yFeatureEnabled(false);

  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());
}

// Verifies that enabling an accessibility feature that foces the overview
// button visibility while in tablet mode shows the overview button.
TEST_P(OverviewButtonTrayWithShelfControlsHiddenTest,
       AccessibilityFeatureEnabledWhileInTabletMode) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());

  // The button should be shown if the feature gets enabled.
  SetTestA11yFeatureEnabled(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_EQ(HasTestingAccessibilityFeature(), GetTray()->GetVisible());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());
}

TEST_P(OverviewButtonTrayWithShelfControlsHiddenTest,
       AccessibilityFeaturesChangeWhileInOverview) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());

  // Create a window to show in overview.
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  EnterOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(GetTray()->GetVisible());

  // The button should be shown if the feature gets enabled.
  SetTestA11yFeatureEnabled(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_EQ(HasTestingAccessibilityFeature(), GetTray()->GetVisible());

  // The button should be hidden if the feature gets disabled.
  SetTestA11yFeatureEnabled(false);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(GetTray()->GetVisible());
}

}  // namespace ash
