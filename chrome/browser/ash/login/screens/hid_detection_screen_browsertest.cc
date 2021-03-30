// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/hid_controller_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "services/device/public/cpp/hid/fake_input_service_linux.h"
#include "services/device/public/mojom/input_service.mojom.h"

using ::testing::_;

namespace chromeos {

namespace {

const test::UIPath kHidContinueButton = {"hid-detection",
                                         "hid-continue-button"};
const test::UIPath kHidTouchscreenEntry = {"hid-detection",
                                           "hid-touchscreen-entry"};
const test::UIPath kHidMouseTick = {"hid-detection", "mouse-tick"};
const test::UIPath kHidKeyboardTick = {"hid-detection", "keyboard-tick"};

}  // namespace

// TODO(crbug/1173782): use INSTANTIATE_TEST_SUITE_P to test this for
// chromebox, chromebase, chromebit
class HIDDetectionScreenChromeboxTest : public OobeBaseTest {
 public:
  HIDDetectionScreenChromeboxTest() {
    // HID detection screen only appears for Chromebases, Chromebits, and
    // Chromeboxes.
    base::SysInfo::SetChromeOSVersionInfoForTest("DEVICETYPE=CHROMEBOX",
                                                 base::Time::Now());
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(WizardController::default_controller());

    hid_detection_screen_ = static_cast<HIDDetectionScreen*>(
        WizardController::default_controller()->GetScreen(
            HIDDetectionView::kScreenId));
    ASSERT_TRUE(hid_detection_screen_);
    ASSERT_TRUE(hid_detection_screen_->view_);

    hid_detection_screen()->SetAdapterInitialPoweredForTesting(false);
    OobeBaseTest::SetUpOnMainThread();
  }

  HIDDetectionScreen* hid_detection_screen() { return hid_detection_screen_; }
  HIDDetectionScreenHandler* handler() {
    return static_cast<HIDDetectionScreenHandler*>(
        hid_detection_screen()->view_);
  }

  void ContinueToWelcomeScreen() {
    // Simulate the user's click on "Continue" button.
    test::OobeJS().CreateVisibilityWaiter(true, kHidContinueButton)->Wait();
    test::OobeJS().TapOnPath(kHidContinueButton);
    OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  }

 protected:
  const base::Optional<HIDDetectionScreen::Result>& GetExitResult() {
    return WizardController::default_controller()
        ->GetScreen<HIDDetectionScreen>()
        ->get_exit_result_for_testing();
  }

  test::HIDControllerMixin hid_controller_{&mixin_host_};

 private:
  HIDDetectionScreen* hid_detection_screen_;

  DISALLOW_COPY_AND_ASSIGN(HIDDetectionScreenChromeboxTest);
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest, NoDevicesConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);
  EXPECT_FALSE(GetExitResult().has_value());
}

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest, MouseKeyboardStates) {
  // NOTE: State strings match those in hid_detection_screen.cc.
  // No devices added yet
  EXPECT_EQ("searching", handler()->mouse_state_for_test());
  EXPECT_EQ("searching", handler()->keyboard_state_for_test());
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  // Generic connection types. Unlike the pointing device, which may be a tablet
  // or touchscreen, the keyboard only reports usb and bluetooth states.
  hid_controller_.AddMouse(device::mojom::InputDeviceType::TYPE_SERIO);
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  hid_controller_.AddKeyboard(device::mojom::InputDeviceType::TYPE_SERIO);
  EXPECT_EQ("connected", handler()->mouse_state_for_test());
  EXPECT_EQ("usb", handler()->keyboard_state_for_test());
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  // Remove generic devices, add usb devices.
  hid_controller_.RemoveDevices();
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  hid_controller_.ConnectUSBDevices();
  // TODO(crbug/1173782): use screen or JS state instead of handler()
  EXPECT_EQ("usb", handler()->mouse_state_for_test());
  EXPECT_EQ("usb", handler()->keyboard_state_for_test());
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  // Remove usb devices, add bluetooth devices.
  hid_controller_.RemoveDevices();
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  hid_controller_.ConnectBTDevices();
  EXPECT_EQ("paired", handler()->mouse_state_for_test());
  EXPECT_EQ("paired", handler()->keyboard_state_for_test());
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
}

// Test that if there is any Bluetooth device connected on HID screen, the
// Bluetooth adapter should not be disabled after advancing to the next screen.
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest,
                       BluetoothDeviceConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();

  // Add a pair of USB mouse/keyboard so that `pointing_device_type_`
  // and `keyboard_type_` are
  // device::mojom::InputDeviceType::TYPE_USB.
  hid_controller_.ConnectUSBDevices();

  // Add another pair of Bluetooth mouse/keyboard.
  hid_controller_.ConnectBTDevices();

  EXPECT_CALL(*hid_controller_.mock_bluetooth_adapter(),
              SetPowered(false, _, _))
      .Times(0);
  ContinueToWelcomeScreen();
  testing::Mock::VerifyAndClear(&*hid_controller_.mock_bluetooth_adapter());
}

// Test that if there is no Bluetooth device connected on HID screen, the
// Bluetooth adapter should be disabled after advancing to the next screen.
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest,
                       NoBluetoothDeviceConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();

  hid_controller_.ConnectUSBDevices();

  // The adapter should be powered off at this moment.
  EXPECT_CALL(*hid_controller_.mock_bluetooth_adapter(),
              SetPowered(false, _, _))
      .Times(1);
  ContinueToWelcomeScreen();
  testing::Mock::VerifyAndClear(&*hid_controller_.mock_bluetooth_adapter());
}

// Start without devices, connect them and proceed to the network screen.
// Network screen should be saved in the local state.
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest, PRE_ResumableScreen) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  hid_controller_.ConnectUSBDevices();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  test::OobeJS().TapOnPath(kHidContinueButton);
  EXPECT_EQ(GetExitResult(), HIDDetectionScreen::Result::NEXT);

  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::TapWelcomeNext();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// Start without devices, connect them. Flow should proceed to the saved screen
// (network screen).
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest, ResumableScreen) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  hid_controller_.ConnectUSBDevices();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  test::OobeJS().TapOnPath(kHidContinueButton);
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// Tests that the connected 'ticks' are shown when the devices are connected.
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest, TestTicks) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  // When touch screen is not detected, the whole touchscreen row is hidden
  test::OobeJS().ExpectHiddenPath(kHidTouchscreenEntry);
  test::OobeJS().CreateVisibilityWaiter(false, kHidMouseTick)->Wait();
  test::OobeJS().CreateVisibilityWaiter(false, kHidKeyboardTick)->Wait();

  hid_controller_.ConnectUSBDevices();
  test::OobeJS().CreateVisibilityWaiter(true, kHidMouseTick)->Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kHidKeyboardTick)->Wait();

  ContinueToWelcomeScreen();
}

class HIDDetectionSkipTest : public HIDDetectionScreenChromeboxTest {
 public:
  HIDDetectionSkipTest() {
    hid_controller_.set_wait_until_idle_after_device_update(false);
    hid_controller_.ConnectUSBDevices();
  }
  ~HIDDetectionSkipTest() override = default;
};

IN_PROC_BROWSER_TEST_F(HIDDetectionSkipTest, BothDevicesPreConnected) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  EXPECT_FALSE(GetExitResult().has_value());
}

class HIDDetectionDeviceOwnedTest : public HIDDetectionScreenChromeboxTest {
 private:
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(HIDDetectionDeviceOwnedTest, NoScreen) {
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
}

class HIDDetectionOobeCompletedUnowned
    : public HIDDetectionScreenChromeboxTest {
 private:
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
};

IN_PROC_BROWSER_TEST_F(HIDDetectionOobeCompletedUnowned, ShowScreen) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
}

class HIDDetectionScreenDisabledAfterRestartTest
    : public HIDDetectionScreenChromeboxTest {
 public:
  HIDDetectionScreenDisabledAfterRestartTest() = default;
  // HidDetectionTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HIDDetectionScreenChromeboxTest::SetUpCommandLine(command_line);
    // Emulating Chrome restart without the flag.
    if (content::IsPreTest()) {
      command_line->AppendSwitch(
          switches::kDisableHIDDetectionOnOOBEForTesting);
    }
  }
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenDisabledAfterRestartTest,
                       PRE_SkipToUpdate) {
  // Pref should be false by default.
  EXPECT_FALSE(StartupUtils::IsHIDDetectionScreenDisabledForTests());

  WizardController::default_controller()->SkipToUpdateForTesting();
  // SkipToUpdateForTesting should set the pref when
  // switches::kDisableHIDDetectionOnOOBEForTesting is passed.
  EXPECT_TRUE(StartupUtils::IsHIDDetectionScreenDisabledForTests());

  EXPECT_EQ(GetExitResult(), HIDDetectionScreen::Result::SKIPPED_FOR_TESTS);
}

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenDisabledAfterRestartTest,
                       SkipToUpdate) {
  // The pref should persist restart.
  EXPECT_TRUE(StartupUtils::IsHIDDetectionScreenDisabledForTests());

  EXPECT_EQ(GetExitResult(), HIDDetectionScreen::Result::SKIPPED_FOR_TESTS);
}

class HIDDetectionScreenChromebookTest : public OobeBaseTest {
 public:
  HIDDetectionScreenChromebookTest() {
    // Set device type to one that should not invoke HIDDetectionScreen logic.
    base::SysInfo::SetChromeOSVersionInfoForTest("DEVICETYPE=CHROMEBOOK",
                                                 base::Time::Now());
  }
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromebookTest,
                       HIDDetectionScreenNotAllowed) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  ASSERT_TRUE(WizardController::default_controller());

  EXPECT_FALSE(WizardController::default_controller()->HasScreen(
      HIDDetectionView::kScreenId));
}

class HIDDetectionScreenChromebaseTest : public OobeBaseTest {
 public:
  HIDDetectionScreenChromebaseTest() {
    // Set device type to a Chromebase with a touch screen.
    // This should show the HIDDetectionScreen with the continue button
    // always enabled, since the user can complete all of OOBE steps
    // with only a touchscreen.
    base::SysInfo::SetChromeOSVersionInfoForTest("DEVICETYPE=CHROMEBASE",
                                                 base::Time::Now());
    hid_controller_.set_wait_until_idle_after_device_update(false);
    hid_controller_.AddTouchscreen();
  }

 protected:
  test::HIDControllerMixin hid_controller_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromebaseTest, TouchscreenDetected) {
  // Continue button should be enabled at all times if touchscreen is detected
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kHidTouchscreenEntry)->Wait();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  hid_controller_.ConnectUSBDevices();
  hid_controller_.RemoveDevices();

  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
}

}  // namespace chromeos
