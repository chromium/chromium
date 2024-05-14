// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chromeos/ash/components/hid_detection/bluetooth_hid_detector.h"
#include "chromeos/ash/components/hid_detection/fake_hid_detection_manager.h"
#include "chromeos/ash/components/hid_detection/hid_detection_manager.h"
#include "chromeos/ash/components/hid_detection/hid_detection_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "services/device/public/cpp/hid/fake_input_service_linux.h"
#include "services/device/public/mojom/input_service.mojom.h"

namespace ash {

namespace {

using ::testing::_;
using HidType = hid_detection::HidType;
using HidsMissing = hid_detection::HidsMissing;
using InputState = hid_detection::HidDetectionManager::InputState;
using HidDetectionBluetoothPairingResult =
    hid_detection::HidDetectionBluetoothPairingResult;

const char kTestPointerName[] = "pointer";
const char kTestKeyboardName[] = "keyboard";

const test::UIPath kHidContinueButton = {"hid-detection",
                                         "hid-continue-button"};
const test::UIPath kHidTouchscreenEntry = {"hid-detection",
                                           "hid-touchscreen-entry"};
const test::UIPath kHidMouseTick = {"hid-detection", "mouse-tick"};
const test::UIPath kHidKeyboardTick = {"hid-detection", "keyboard-tick"};

InputState GetHidInputState(
    device::mojom::InputDeviceType connected_hid_device_type) {
  switch (connected_hid_device_type) {
    case device::mojom::InputDeviceType::TYPE_BLUETOOTH:
      return InputState::kPairedViaBluetooth;
    case device::mojom::InputDeviceType::TYPE_USB:
      return InputState::kConnectedViaUsb;
    case device::mojom::InputDeviceType::TYPE_SERIO:
      [[fallthrough]];
    case device::mojom::InputDeviceType::TYPE_UNKNOWN:
      return InputState::kConnected;
  }
}

}  // namespace

// TODO(crbug.com/40167270): use INSTANTIATE_TEST_SUITE_P to test this for
// chromebox, chromebase, chromebit
class HIDDetectionScreenChromeboxTest : public OobeBaseTest {
 public:
  HIDDetectionScreenChromeboxTest() {
      auto fake_hid_detection_manager =
          std::make_unique<hid_detection::FakeHidDetectionManager>();
      fake_hid_detection_manager_ = fake_hid_detection_manager.get();
      HIDDetectionScreen::OverrideHidDetectionManagerForTesting(
          std::move(fake_hid_detection_manager));
  }

  HIDDetectionScreenChromeboxTest(const HIDDetectionScreenChromeboxTest&) =
      delete;
  HIDDetectionScreenChromeboxTest& operator=(
      const HIDDetectionScreenChromeboxTest&) = delete;

  ~HIDDetectionScreenChromeboxTest() override = default;

  void SetUpOnMainThread() override {
    if (HIDDetectionScreen::CanShowScreen()) {
      ASSERT_TRUE(WizardController::default_controller());

      hid_detection_screen_ = static_cast<HIDDetectionScreen*>(
          WizardController::default_controller()->GetScreen(
              HIDDetectionView::kScreenId));
      ASSERT_TRUE(hid_detection_screen_);
      ASSERT_TRUE(hid_detection_screen_->view_);
    }
    OobeBaseTest::SetUpOnMainThread();
  }

  HIDDetectionScreen* hid_detection_screen() { return hid_detection_screen_; }
  HIDDetectionScreenHandler* handler() {
    return LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetHandler<HIDDetectionScreenHandler>();
  }

  void ContinueToWelcomeScreen() {
    // Simulate the user's click on "Continue" button.
    test::OobeJS().CreateVisibilityWaiter(true, kHidContinueButton)->Wait();
    test::OobeJS().TapOnPath(kHidContinueButton);
    test::WaitForWelcomeScreen();
  }

 protected:
  const std::optional<HIDDetectionScreen::Result>& GetExitResult() {
    return WizardController::default_controller()
        ->GetScreen<HIDDetectionScreen>()
        ->get_exit_result_for_testing();
  }

  void SimulatePointerHidConnected(device::mojom::InputDeviceType device_type) {
      fake_hid_detection_manager_->SetHidStatusPointerMetadata(
          {GetHidInputState(device_type), kTestPointerName});
  }

  void SimulateKeyboardHidConnected(
      device::mojom::InputDeviceType device_type) {
      fake_hid_detection_manager_->SetHidStatusKeyboardMetadata(
          {GetHidInputState(device_type), kTestKeyboardName});
  }

  void SimulatePointerHidRemoved() {
      fake_hid_detection_manager_->SetHidStatusPointerMetadata(
          {InputState::kSearching, /*detected_hid_name=*/""});
  }

  void SimulateKeyboardHidRemoved() {
      fake_hid_detection_manager_->SetHidStatusKeyboardMetadata(
          {InputState::kSearching, /*detected_hid_name=*/""});
  }

  void SimulateBluetoothDeviceDiscovered(
      const device::BluetoothDeviceType device_type) {
      if (device_type == device::BluetoothDeviceType::MOUSE) {
        fake_hid_detection_manager_->SetHidStatusPointerMetadata(
            {InputState::kPairingViaBluetooth, kTestPointerName});
      } else if (device_type == device::BluetoothDeviceType::KEYBOARD) {
        fake_hid_detection_manager_->SetHidStatusKeyboardMetadata(
            {InputState::kPairingViaBluetooth, kTestKeyboardName});
      } else {
        NOTREACHED_IN_MIGRATION()
            << "SimulateBluetoothDeviceDiscovered() should only be "
            << "called with cases MOUSE or KEYBOARD.";
        return;
      }
  }

  // HID detection must be stopped before HidDetectionManager is destroyed. This
  // should be called in tests that start HID detection but don't continue to
  // the next screen.
  void ForceStopHidDetection() {
    test::OobeJS().CreateVisibilityWaiter(true, kHidContinueButton)->Wait();
    test::OobeJS().TapOnPath(kHidContinueButton);
  }

  size_t num_devices_created_ = 0u;

  device::BluetoothDevice::ConnectCallback connect_callback_;

 private:
  raw_ptr<HIDDetectionScreen, DanglingUntriaged> hid_detection_screen_;

  raw_ptr<hid_detection::FakeHidDetectionManager, DanglingUntriaged>
      fake_hid_detection_manager_;

  // HID detection screen only appears for Chromebases, Chromebits, and
  // Chromeboxes.
  base::test::ScopedChromeOSVersionInfo version_{"DEVICETYPE=CHROMEBOX",
                                                 base::Time::Now()};

  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest, NoDevicesConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  EXPECT_FALSE(GetExitResult().has_value());
  ForceStopHidDetection();
}

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest, MouseKeyboardStates) {
  // NOTE: State strings match those in hid_detection_screen.cc.
  // No devices added yet
  EXPECT_EQ("searching", handler()->mouse_state_for_test());
  EXPECT_EQ("searching", handler()->keyboard_state_for_test());
  EXPECT_TRUE(handler()->mouse_device_name_for_test().empty());
  EXPECT_TRUE(handler()->keyboard_device_name_for_test().empty());
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  // Generic connection types. Unlike the pointing device, which may be a tablet
  // or touchscreen, the keyboard only reports usb and bluetooth states.
  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_SERIO);
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_SERIO);
  EXPECT_EQ("connected", handler()->mouse_state_for_test());
  EXPECT_EQ("usb", handler()->keyboard_state_for_test());
  EXPECT_EQ(kTestPointerName, handler()->mouse_device_name_for_test());
  EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  // Remove generic devices, add usb devices.
  SimulatePointerHidRemoved();
  SimulateKeyboardHidRemoved();
  EXPECT_EQ("searching", handler()->mouse_state_for_test());
  EXPECT_EQ("searching", handler()->keyboard_state_for_test());

  // The device names are reset.
  EXPECT_TRUE(handler()->mouse_device_name_for_test().empty());
  EXPECT_TRUE(handler()->keyboard_device_name_for_test().empty());
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  // TODO(crbug.com/40167270): use screen or JS state instead of handler()
  EXPECT_EQ("usb", handler()->mouse_state_for_test());
  EXPECT_EQ("usb", handler()->keyboard_state_for_test());
  EXPECT_EQ(kTestPointerName, handler()->mouse_device_name_for_test());
  EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  // Remove usb devices, add bluetooth devices.
  SimulatePointerHidRemoved();
  SimulateKeyboardHidRemoved();

  // The device names are reset.
  EXPECT_TRUE(handler()->mouse_device_name_for_test().empty());
  EXPECT_TRUE(handler()->keyboard_device_name_for_test().empty());
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  // The device states and names are set during pairing.
  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::MOUSE);
  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::KEYBOARD);
  EXPECT_EQ("pairing", handler()->mouse_state_for_test());
  EXPECT_EQ("pairing", handler()->keyboard_state_for_test());
  EXPECT_EQ(kTestPointerName, handler()->mouse_device_name_for_test());
  EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());

  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  EXPECT_EQ("paired", handler()->mouse_state_for_test());
  EXPECT_EQ("paired", handler()->keyboard_state_for_test());
  EXPECT_EQ(kTestPointerName, handler()->mouse_device_name_for_test());
  EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  ForceStopHidDetection();
}

// Start without devices, connect them and proceed to the network screen.
// Network screen should be saved in the local state.
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest, PRE_ResumableScreen) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  test::OobeJS().TapOnPath(kHidContinueButton);
  EXPECT_EQ(GetExitResult(), HIDDetectionScreen::Result::NEXT);

  test::WaitForWelcomeScreen();
  test::TapWelcomeNext();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// Start without devices, connect them. Flow should proceed to the saved screen
// (network screen).
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromeboxTest, ResumableScreen) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
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

  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  test::OobeJS().CreateVisibilityWaiter(true, kHidMouseTick)->Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kHidKeyboardTick)->Wait();

  ContinueToWelcomeScreen();
}

class HIDDetectionSkipTest : public HIDDetectionScreenChromeboxTest {
 public:
  HIDDetectionSkipTest() {
    SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
    SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  }
  ~HIDDetectionSkipTest() override = default;

 protected:
  base::HistogramTester histogram_tester;
};

IN_PROC_BROWSER_TEST_F(HIDDetectionSkipTest, BothDevicesPreConnected) {
  test::WaitForWelcomeScreen();
  EXPECT_FALSE(GetExitResult().has_value());
  histogram_tester.ExpectTotalCount("OOBE.HidDetectionScreen.HidConnected", 0);
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
  ForceStopHidDetection();
}

class HIDDetectionScreenDisabledAfterRestartTest
    : public HIDDetectionScreenChromeboxTest,
      public LocalStateMixin::Delegate {
 public:
  HIDDetectionScreenDisabledAfterRestartTest() = default;
  // HIDDetectionScreenChromeboxTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HIDDetectionScreenChromeboxTest::SetUpCommandLine(command_line);
    // Emulating Chrome restart without the flag.
    if (content::IsPreTest()) {
      command_line->AppendSwitch(
          switches::kDisableHIDDetectionOnOOBEForTesting);
    }
  }
  // We need to check local state flag before welcome screen is shown.
  void SetUpLocalState() override {
    if (content::IsPreTest()) {
      // Pref should be false by default.
      EXPECT_FALSE(StartupUtils::IsHIDDetectionScreenDisabledForTests());
    }
  }

 private:
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenDisabledAfterRestartTest,
                       PRE_SkipToUpdate) {
  test::WaitForWelcomeScreen();

  EXPECT_TRUE(StartupUtils::IsHIDDetectionScreenDisabledForTests());
  EXPECT_FALSE(WizardController::default_controller()->HasScreen(
      HIDDetectionView::kScreenId));
}

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenDisabledAfterRestartTest,
                       SkipToUpdate) {
  test::WaitForWelcomeScreen();
  // The pref should persist restart.
  EXPECT_TRUE(StartupUtils::IsHIDDetectionScreenDisabledForTests());
  EXPECT_FALSE(WizardController::default_controller()->HasScreen(
      HIDDetectionView::kScreenId));
}

class HIDDetectionScreenChromebookTest : public OobeBaseTest {
 private:
  // Set device type to one that should not invoke HIDDetectionScreen logic.
  base::test::ScopedChromeOSVersionInfo version_{"DEVICETYPE=CHROMEBOOK",
                                                 base::Time::Now()};
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromebookTest,
                       HIDDetectionScreenNotAllowed) {
  test::WaitForWelcomeScreen();
  ASSERT_TRUE(WizardController::default_controller());

  EXPECT_FALSE(WizardController::default_controller()->HasScreen(
      HIDDetectionView::kScreenId));
}

class HIDDetectionScreenChromebaseTest : public OobeBaseTest {
 public:
  HIDDetectionScreenChromebaseTest() {
      auto fake_hid_detection_manager =
          std::make_unique<hid_detection::FakeHidDetectionManager>();
      fake_hid_detection_manager_ = fake_hid_detection_manager.get();
      HIDDetectionScreen::OverrideHidDetectionManagerForTesting(
          std::move(fake_hid_detection_manager));

      fake_hid_detection_manager_->SetHidStatusTouchscreenDetected(true);
  }

 protected:
  void SimulatePointerHidConnected(device::mojom::InputDeviceType device_type) {
      fake_hid_detection_manager_->SetHidStatusPointerMetadata(
          {GetHidInputState(device_type), kTestPointerName});
  }

  void SimulateKeyboardHidConnected(
      device::mojom::InputDeviceType device_type) {
      fake_hid_detection_manager_->SetHidStatusKeyboardMetadata(
          {GetHidInputState(device_type), kTestKeyboardName});
  }

  void SimulatePointerHidRemoved() {
      fake_hid_detection_manager_->SetHidStatusPointerMetadata(
          {InputState::kSearching, /*detected_hid_name=*/""});
  }

  void SimulateKeyboardHidRemoved() {
      fake_hid_detection_manager_->SetHidStatusKeyboardMetadata(
          {InputState::kSearching, /*detected_hid_name=*/""});
  }

  // HID detection must be stopped before HidDetectionManager is destroyed. This
  // should be called in tests that start HID detection but don't continue to
  // the next screen.
  void ForceStopHidDetection() {
    test::OobeJS().CreateVisibilityWaiter(true, kHidContinueButton)->Wait();
    test::OobeJS().TapOnPath(kHidContinueButton);
  }

 private:
  raw_ptr<hid_detection::FakeHidDetectionManager, DanglingUntriaged>
      fake_hid_detection_manager_;

  // Set device type to a Chromebase with a touch screen.
  // This should show the HIDDetectionScreen with the continue button
  // always enabled, since the user can complete all of OOBE steps
  // with only a touchscreen.
  base::test::ScopedChromeOSVersionInfo version_{"DEVICETYPE=CHROMEBASE",
                                                 base::Time::Now()};
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenChromebaseTest, TouchscreenDetected) {
  // Continue button should be enabled at all times if touchscreen is detected
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kHidTouchscreenEntry)->Wait();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  test::OobeJS().CreateVisibilityWaiter(true, kHidMouseTick)->Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kHidKeyboardTick)->Wait();

  SimulatePointerHidRemoved();
  SimulateKeyboardHidRemoved();
  test::OobeJS().CreateVisibilityWaiter(false, kHidMouseTick)->Wait();
  test::OobeJS().CreateVisibilityWaiter(false, kHidKeyboardTick)->Wait();

  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  ForceStopHidDetection();
}

class HIDDetectionScreenPreConnectedDeviceTest
    : public HIDDetectionScreenChromeboxTest {
 public:
  HIDDetectionScreenPreConnectedDeviceTest() {
    SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  }
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenPreConnectedDeviceTest,
                       MousePreConnected) {
  // Continue button should be enabled if at least one device is connected.
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(kHidTouchscreenEntry);
  test::OobeJS().CreateVisibilityWaiter(true, kHidMouseTick)->Wait();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  test::OobeJS().CreateVisibilityWaiter(true, kHidKeyboardTick)->Wait();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  test::OobeJS().CreateVisibilityWaiter(true, kHidKeyboardTick)->Wait();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  ForceStopHidDetection();
}

class HIDDetectionPreconnectedBTTest : public HIDDetectionScreenChromeboxTest {
 public:
  HIDDetectionPreconnectedBTTest() {
    SimulateKeyboardHidConnected(
        device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  }
};

IN_PROC_BROWSER_TEST_F(HIDDetectionPreconnectedBTTest,
                       BTKeyboardDevicePreConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(kHidTouchscreenEntry);
  test::OobeJS().CreateVisibilityWaiter(true, kHidKeyboardTick)->Wait();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  EXPECT_FALSE(GetExitResult().has_value());
  ForceStopHidDetection();
}

}  // namespace ash
