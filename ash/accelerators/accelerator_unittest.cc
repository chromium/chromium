// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_impl.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/network/network_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ui_controls_factory_ash.h"
#include "ash/test_screenshot_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/user_action_tester.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/network_handler.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

// A network observer to watch for the toggle wifi events.
class TestNetworkObserver : public NetworkObserver {
 public:
  TestNetworkObserver() = default;
  ~TestNetworkObserver() override = default;

  // ash::NetworkObserver:
  void RequestToggleWifi() override {
    wifi_enabled_status_ = !wifi_enabled_status_;
  }

  bool wifi_enabled_status() const { return wifi_enabled_status_; }

 private:
  bool wifi_enabled_status_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkObserver);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// This is intended to test few samples from each category of accelerators to
// make sure they work properly. The test is done as an interactive ui test
// using ui_controls::Send*() functions.
// This is to catch any future regressions (crbug.com/469235).
class AcceleratorTest : public AshTestBase, public OverviewObserver {
 public:
  AcceleratorTest() : is_in_overview_mode_(false) {}

  void SetUp() override {
    ui_controls::InstallUIControlsAura(test::CreateAshUIControls());

    AshTestBase::SetUp();

    Shell::Get()->overview_controller()->AddObserver(this);
  }

  void TearDown() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);

    AshTestBase::TearDown();

    ui_controls::InstallUIControlsAura(nullptr);
  }

  // Sends a key press event and waits synchronously until it's completely
  // processed.
  void SendKeyPressSync(ui::KeyboardCode key,
                        bool control,
                        bool shift,
                        bool alt) {
    base::RunLoop loop;
    ui_controls::SendKeyPressNotifyWhenDone(Shell::GetPrimaryRootWindow(), key,
                                            control, shift, alt, false,
                                            loop.QuitClosure());
    loop.Run();
  }

  // OverviewObserver:
  void OnOverviewModeStarting() override { is_in_overview_mode_ = true; }
  void OnOverviewModeEnded() override { is_in_overview_mode_ = false; }

 protected:
  bool is_in_overview_mode_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratorTest);
};

////////////////////////////////////////////////////////////////////////////////

// Tests a sample of accelerators.
TEST_F(AcceleratorTest, Basic) {
  // Test VOLUME_MUTE.
  base::UserActionTester user_action_tester;
  EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeMute_F8"));
  SendKeyPressSync(ui::VKEY_VOLUME_MUTE, false, false, false);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeMute_F8"));
  // Test VOLUME_DOWN.
  EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
  SendKeyPressSync(ui::VKEY_VOLUME_DOWN, false, false, false);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeDown_F9"));
  // Test VOLUME_UP.
  EXPECT_EQ(0, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));
  SendKeyPressSync(ui::VKEY_VOLUME_UP, false, false, false);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Accel_VolumeUp_F10"));

  // Test TOGGLE_WIFI.
  TestNetworkObserver network_observer;
  Shell::Get()->system_tray_notifier()->AddNetworkObserver(&network_observer);

  EXPECT_FALSE(network_observer.wifi_enabled_status());
  SendKeyPressSync(ui::VKEY_WLAN, false, false, false);
  EXPECT_TRUE(network_observer.wifi_enabled_status());
  SendKeyPressSync(ui::VKEY_WLAN, false, false, false);
  EXPECT_FALSE(network_observer.wifi_enabled_status());

  Shell::Get()->system_tray_notifier()->RemoveNetworkObserver(
      &network_observer);
}

// Tests full screenshot accelerators.
TEST_F(AcceleratorTest, FullScreenshot) {
  if (features::IsCaptureModeEnabled()) {
    // Capture mode shortcuts and behavior are tested elsewhere.
    return;
  }

  TestScreenshotDelegate* screenshot_delegate = GetScreenshotDelegate();
  screenshot_delegate->set_can_take_screenshot(true);
  EXPECT_EQ(0, screenshot_delegate->handle_take_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_window_screenshot_count());

  // Test TAKE_SCREENSHOT via crtl+overview key.
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, true, false, false);
  EXPECT_EQ(1, screenshot_delegate->handle_take_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_window_screenshot_count());
  screenshot_delegate->reset_all_screenshot_counts();

  // Test TAKE_SCREENSHOT via PrtScn/Snapshot key.
  SendKeyPressSync(ui::VKEY_SNAPSHOT, false, false, false);
  EXPECT_EQ(1, screenshot_delegate->handle_take_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_window_screenshot_count());
  screenshot_delegate->reset_all_screenshot_counts();
}

// Tests partial screenshot accelerators.
TEST_F(AcceleratorTest, PartialScreenshot) {
  if (features::IsCaptureModeEnabled()) {
    // Capture mode shortcuts and behavior are tested elsewhere.
    return;
  }

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  TestScreenshotDelegate* screenshot_delegate = GetScreenshotDelegate();
  screenshot_delegate->set_can_take_screenshot(true);

  // Test TAKE_PARTIAL_SCREENSHOT via ctrl+shift+overview key then exit with
  // escape key.
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, true, true, false);
  SendKeyPressSync(ui::VKEY_ESCAPE, false, false, false);
  EXPECT_EQ(0, screenshot_delegate->handle_take_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_window_screenshot_count());
  screenshot_delegate->reset_all_screenshot_counts();

  // Test TAKE_PARTIAL_SCREENSHOT via alt+PrtScn key then exit with escape
  // key.
  SendKeyPressSync(ui::VKEY_SNAPSHOT, false, false, true);
  SendKeyPressSync(ui::VKEY_ESCAPE, false, false, false);
  EXPECT_EQ(0, screenshot_delegate->handle_take_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_window_screenshot_count());
  screenshot_delegate->reset_all_screenshot_counts();

  // Test TAKE_PARTIAL_SCREENSHOT via ctrl+shift+overview key then select
  // a region to complete the screenshot.
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, true, true, false);
  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();
  EXPECT_EQ(0, screenshot_delegate->handle_take_partial_screenshot_count());
  generator.MoveMouseTo(150, 150);
  generator.ReleaseLeftButton();
  EXPECT_EQ(0, screenshot_delegate->handle_take_screenshot_count());
  EXPECT_EQ(1, screenshot_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_window_screenshot_count());
  EXPECT_EQ(gfx::Rect(100, 100, 50, 50), screenshot_delegate->last_rect());
  screenshot_delegate->reset_all_screenshot_counts();

  // Test TAKE_PARTIAL_SCREENSHOT via alt+PrtScn key then select a region
  // to complete the screenshot.
  SendKeyPressSync(ui::VKEY_SNAPSHOT, false, false, true);
  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();
  EXPECT_EQ(0, screenshot_delegate->handle_take_partial_screenshot_count());
  generator.MoveMouseTo(150, 150);
  generator.ReleaseLeftButton();
  EXPECT_EQ(0, screenshot_delegate->handle_take_screenshot_count());
  EXPECT_EQ(1, screenshot_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_window_screenshot_count());
  EXPECT_EQ(gfx::Rect(100, 100, 50, 50), screenshot_delegate->last_rect());
  screenshot_delegate->reset_all_screenshot_counts();
}

// Tests window screenshot accelerators.
TEST_F(AcceleratorTest, WindowScreenshot) {
  if (features::IsCaptureModeEnabled()) {
    // Capture mode shortcuts and behavior are tested elsewhere.
    return;
  }

  TestScreenshotDelegate* screenshot_delegate = GetScreenshotDelegate();
  screenshot_delegate->set_can_take_screenshot(true);

  // Test TAKE_WINDOW_SCREENSHOT via ctrl+alt+overview then exit with
  // escape key.
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, true, false, true);
  SendKeyPressSync(ui::VKEY_ESCAPE, false, false, false);
  EXPECT_EQ(0, screenshot_delegate->handle_take_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_window_screenshot_count());
  EXPECT_EQ(nullptr, screenshot_delegate->GetSelectedWindowAndReset());
  screenshot_delegate->reset_all_screenshot_counts();

  // Test TAKE_WINDOW_SCREENSHOT via ctrl+alt+overview without an active
  // window.
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, true, false, true);
  SendKeyPressSync(ui::VKEY_RETURN, false, false, false);
  EXPECT_EQ(0, screenshot_delegate->handle_take_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_window_screenshot_count());
  EXPECT_EQ(nullptr, screenshot_delegate->GetSelectedWindowAndReset());
  screenshot_delegate->reset_all_screenshot_counts();

  // Test TAKE_WINDOW_SCREENSHOT via ctrl+alt+overview key then activate
  // the window by pressing return/enter to complete the screenshot.
  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  wm::ActivateWindow(window);
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, true, false, true);
  SendKeyPressSync(ui::VKEY_RETURN, false, false, false);
  EXPECT_EQ(0, screenshot_delegate->handle_take_screenshot_count());
  EXPECT_EQ(0, screenshot_delegate->handle_take_partial_screenshot_count());
  EXPECT_EQ(1, screenshot_delegate->handle_take_window_screenshot_count());
  EXPECT_EQ(window, screenshot_delegate->GetSelectedWindowAndReset());
  screenshot_delegate->reset_all_screenshot_counts();
}

// Tests a sample of the non-repeatable accelerators that need windows to be
// enabled.
TEST_F(AcceleratorTest, NonRepeatableNeedingWindowActions) {
  // Create a bunch of windows to work with.
  aura::Window* window_1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100));
  aura::Window* window_2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100));
  window_1->Show();
  wm::ActivateWindow(window_1);
  window_2->Show();
  wm::ActivateWindow(window_2);

  // Test TOGGLE_OVERVIEW.
  EXPECT_FALSE(is_in_overview_mode_);
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, false, false, false);
  EXPECT_TRUE(is_in_overview_mode_);
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, false, false, false);
  EXPECT_FALSE(is_in_overview_mode_);

  // Test CYCLE_FORWARD_MRU and CYCLE_BACKWARD_MRU.
  wm::ActivateWindow(window_1);
  EXPECT_TRUE(wm::IsActiveWindow(window_1));
  EXPECT_FALSE(wm::IsActiveWindow(window_2));
  SendKeyPressSync(ui::VKEY_TAB, false, false, true);  // CYCLE_FORWARD_MRU.
  EXPECT_TRUE(wm::IsActiveWindow(window_2));
  EXPECT_FALSE(wm::IsActiveWindow(window_1));
  SendKeyPressSync(ui::VKEY_TAB, false, true, true);  // CYCLE_BACKWARD_MRU.
  EXPECT_TRUE(wm::IsActiveWindow(window_1));
  EXPECT_FALSE(wm::IsActiveWindow(window_2));

  // Test TOGGLE_FULLSCREEN.
  WindowState* active_window_state = WindowState::ForActiveWindow();
  EXPECT_FALSE(active_window_state->IsFullscreen());
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP2, false, false, false);
  EXPECT_TRUE(active_window_state->IsFullscreen());
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP2, false, false, false);
  EXPECT_FALSE(active_window_state->IsFullscreen());
}

// Tests the app list accelerator.
TEST_F(AcceleratorTest, ToggleAppList) {
  GetAppListTestHelper()->CheckVisibility(false);
  SendKeyPressSync(ui::VKEY_LWIN, false, false, false);
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  SendKeyPressSync(ui::VKEY_LWIN, false, false, false);
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

}  // namespace ash
