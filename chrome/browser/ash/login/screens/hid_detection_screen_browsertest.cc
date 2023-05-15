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
#include "chrome/browser/ash/login/test/hid_controller_mixin.h"
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

const uint32_t kTestBluetoothClass = 1337u;
const char kTestBluetoothName[] = "testName";

const char kTestPointerName[] = "pointer";
const char kTestKeyboardName[] = "keyboard";

const char kTestPinCode[] = "pincode";

const test::UIPath kHidContinueButton = {"hid-detection",
                                         "hid-continue-button"};
const test::UIPath kHidTouchscreenEntry = {"hid-detection",
                                           "hid-touchscreen-entry"};
const test::UIPath kHidMouseTick = {"hid-detection", "mouse-tick"};
const test::UIPath kHidKeyboardTick = {"hid-detection", "keyboard-tick"};
const test::UIPath kHidPairingDialog = {"hid-detection", "hid-pin-popup"};
const test::UIPath kHidPairingDialogEnterCodePage = {"hid-detection",
                                                     "hid-pairing-enter-code"};

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

// TODO(crbug/1173782): use INSTANTIATE_TEST_SUITE_P to test this for
// chromebox, chromebase, chromebit
class HIDDetectionScreenChromeboxTest
    : public OobeBaseTest,
      public testing::WithParamInterface<bool> {
 public:
  HIDDetectionScreenChromeboxTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kOobeHidDetectionRevamp);

      auto fake_hid_detection_manager =
          std::make_unique<hid_detection::FakeHidDetectionManager>();
      fake_hid_detection_manager_ = fake_hid_detection_manager.get();
      HIDDetectionScreen::OverrideHidDetectionManagerForTesting(
          std::move(fake_hid_detection_manager));
      return;
    }

    scoped_feature_list_.InitAndDisableFeature(
        features::kOobeHidDetectionRevamp);
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

      hid_detection_screen()->SetAdapterInitialPoweredForTesting(false);
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
  const absl::optional<HIDDetectionScreen::Result>& GetExitResult() {
    return WizardController::default_controller()
        ->GetScreen<HIDDetectionScreen>()
        ->get_exit_result_for_testing();
  }

  void AssertHidConnectedCount(HidType hid_type, int count) {
    // This is not applicable after the revamp.
    if (GetParam())
      return;

    histogram_tester_.ExpectBucketCount("OOBE.HidDetectionScreen.HidConnected",
                                        hid_type, count);
  }

  void AssertBluetoothPairingAttemptsCount(int count) {
    histogram_tester_.ExpectBucketCount(
        "OOBE.HidDetectionScreen.BluetoothPairingAttempts", count, 1);
  }

  void AssertBluetoothPairingAttemptsMetricCount(int count) {
    histogram_tester_.ExpectTotalCount(
        "OOBE.HidDetectionScreen.BluetoothPairingAttempts", count);
  }

  void AssertBluetoothPairingResult(bool success, int count) {
    histogram_tester_.ExpectTotalCount(
        base::StrCat({"OOBE.HidDetectionScreen.BluetoothPairing.Duration.",
                      success ? "Success" : "Failure"}),
        count);
    histogram_tester_.ExpectBucketCount(
        "OOBE.HidDetectionScreen.BluetoothPairing.Result",
        success ? HidDetectionBluetoothPairingResult::kPaired
                : HidDetectionBluetoothPairingResult::kNotPaired,
        count);
  }

  void AssertInitialHidsMissingCount(HidsMissing hids_missing, int count) {
    // This is not applicable after the revamp.
    if (GetParam())
      return;

    histogram_tester_.ExpectBucketCount(
        "OOBE.HidDetectionScreen.InitialHidsMissing", hids_missing, count);
  }

  bool HasPendingConnectCallback() const {
    return !connect_callback_.is_null();
  }

  void InvokePendingConnectCallback(bool success) {
    if (success) {
      std::move(connect_callback_).Run(absl::nullopt);
    } else {
      std::move(connect_callback_)
          .Run(device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    }
  }

  void SimulatePointerHidConnected(device::mojom::InputDeviceType device_type) {
    if (GetParam()) {
      fake_hid_detection_manager_->SetHidStatusPointerMetadata(
          {GetHidInputState(device_type), kTestPointerName});
      return;
    }
    hid_controller_.AddMouse(device_type);
  }

  void SimulateKeyboardHidConnected(
      device::mojom::InputDeviceType device_type) {
    if (GetParam()) {
      fake_hid_detection_manager_->SetHidStatusKeyboardMetadata(
          {GetHidInputState(device_type), kTestKeyboardName});
      return;
    }
    hid_controller_.AddKeyboard(device_type);
  }

  void SimulatePointerHidRemoved() {
    if (GetParam()) {
      fake_hid_detection_manager_->SetHidStatusPointerMetadata(
          {InputState::kSearching, /*detected_hid_name=*/""});
      return;
    }
    hid_controller_.RemoveMouse();
  }

  void SimulateKeyboardHidRemoved() {
    if (GetParam()) {
      fake_hid_detection_manager_->SetHidStatusKeyboardMetadata(
          {InputState::kSearching, /*detected_hid_name=*/""});
      return;
    }
    hid_controller_.RemoveKeyboard();
  }

  void SimulateBluetoothDeviceDiscovered(
      const device::BluetoothDeviceType device_type) {
    if (GetParam()) {
      if (device_type == device::BluetoothDeviceType::MOUSE) {
        fake_hid_detection_manager_->SetHidStatusPointerMetadata(
            {InputState::kPairingViaBluetooth, kTestPointerName});
      } else if (device_type == device::BluetoothDeviceType::KEYBOARD) {
        fake_hid_detection_manager_->SetHidStatusKeyboardMetadata(
            {InputState::kPairingViaBluetooth, kTestKeyboardName});
      } else {
        NOTREACHED() << "SimulateBluetoothDeviceDiscovered() should only be "
                     << "called with cases MOUSE or KEYBOARD.";
        return;
      }
    }

    // We use the number of devices created in this test as the address.
    std::string address = base::NumberToString(num_devices_created_);
    ++num_devices_created_;

    auto mock_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            /*adapter=*/nullptr, kTestBluetoothClass, kTestBluetoothName,
            address, /*paired=*/false, /*connected=*/false);
    ON_CALL(*mock_device, Connect(testing::_, testing::_))
        .WillByDefault(MoveArg<1>(&connect_callback_));
    ON_CALL(*mock_device, GetDeviceType())
        .WillByDefault(testing::Return(device_type));

    hid_detection_screen_->DeviceAdded(/*adapter=*/nullptr, mock_device.get());
  }

  void SimulatePairingCodeRequired() {
    // This is not applicable to the legacy screen.
    if (!GetParam())
      return;

    fake_hid_detection_manager_->SetPairingState(
        hid_detection::BluetoothHidPairingState{
            kTestPinCode, static_cast<uint8_t>(std::strlen(kTestPinCode))});
  }

  void SimulatePairingCodeNotRequired() {
    // This is not applicable to the legacy screen.
    if (!GetParam())
      return;

    fake_hid_detection_manager_->SetPairingState(
        /*pairing_state=*/absl::nullopt);
  }

  // HID detection must be stopped before HidDetectionManager is destroyed. This
  // should be called in tests that start HID detection but don't continue to
  // the next screen.
  void ForceStopHidDetectionIfRevamp() {
    if (GetParam()) {
      test::OobeJS().CreateVisibilityWaiter(true, kHidContinueButton)->Wait();
      test::OobeJS().TapOnPath(kHidContinueButton);
    }
  }

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetMockBluetoothAdapter() {
    return hid_controller_.mock_bluetooth_adapter();
  }

  void SetWaitUntilIdleAfterDeviceUpdate(bool wait) {
    if (!GetParam())
      hid_controller_.set_wait_until_idle_after_device_update(wait);
  }

  void AssertHidDisconnectedCount(HidType hid_type, int count) {
    // This is not applicable after the revamp.
    if (!GetParam()) {
      histogram_tester_.ExpectBucketCount(
          "OOBE.HidDetectionScreen.HidDisconnected", hid_type, count);
    }
  }

  size_t num_devices_created_ = 0u;

  device::BluetoothDevice::ConnectCallback connect_callback_;

 private:
  raw_ptr<HIDDetectionScreen, ExperimentalAsh> hid_detection_screen_;

  test::HIDControllerMixin hid_controller_{&mixin_host_};
  raw_ptr<hid_detection::FakeHidDetectionManager, ExperimentalAsh>
      fake_hid_detection_manager_;

  // HID detection screen only appears for Chromebases, Chromebits, and
  // Chromeboxes.
  base::test::ScopedChromeOSVersionInfo version_{"DEVICETYPE=CHROMEBOX",
                                                 base::Time::Now()};

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(All, HIDDetectionScreenChromeboxTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest, NoDevicesConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);
  AssertInitialHidsMissingCount(HidsMissing::kPointerAndKeyboard, /*count=*/1);

  EXPECT_FALSE(GetExitResult().has_value());
  ForceStopHidDetectionIfRevamp();
}

IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest,
                       BluetoothPairingAttemptsSimultaneous) {
  // This test is not applicable after the revamp.
  if (GetParam()) {
    ForceStopHidDetectionIfRevamp();
    return;
  }

  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();

  // Two simultaneous pairing attempts of the same type.
  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::MOUSE);
  ASSERT_TRUE(HasPendingConnectCallback());
  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::MOUSE);
  ASSERT_TRUE(HasPendingConnectCallback());
  // Invoke the first device's connect callback since the second device will
  // never be attempted to be connected with.
  InvokePendingConnectCallback(/*success=*/false);

  // Two simultaneous pairing attempts of different types.
  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::KEYBOARD);
  ASSERT_TRUE(HasPendingConnectCallback());
  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::MOUSE);
  ASSERT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(/*success=*/false);

  // Bluetooth pairing attempt counts should only emit after the welcome screen.
  AssertBluetoothPairingAttemptsMetricCount(/*count=*/0);

  ContinueToWelcomeScreen();
  AssertBluetoothPairingAttemptsCount(/*count=*/3);
}

IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest,
                       BluetoothPairingAttemptsSequential) {
  // This test is not applicable after the revamp.
  if (GetParam()) {
    ForceStopHidDetectionIfRevamp();
    return;
  }

  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();

  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::MOUSE);
  ASSERT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(/*success=*/true);
  AssertBluetoothPairingResult(/*success=*/true, /*count=*/1);

  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::MOUSE);
  ASSERT_FALSE(HasPendingConnectCallback());

  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::KEYBOARD);
  ASSERT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(/*success=*/false);
  AssertBluetoothPairingResult(/*success=*/false, /*count=*/1);

  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::KEYBOARD);
  ASSERT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(/*success=*/false);
  AssertBluetoothPairingResult(/*success=*/false, /*count=*/2);

  ContinueToWelcomeScreen();
  AssertBluetoothPairingAttemptsCount(/*count=*/3);
}

IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest, MouseKeyboardStates) {
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
  AssertHidConnectedCount(HidType::kSerialPointer, /*count=*/1);

  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_SERIO);
  EXPECT_EQ("connected", handler()->mouse_state_for_test());
  EXPECT_EQ("usb", handler()->keyboard_state_for_test());
  EXPECT_EQ(kTestPointerName, handler()->mouse_device_name_for_test());
  EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  AssertHidConnectedCount(HidType::kSerialKeyboard, /*count=*/1);

  // Remove generic devices, add usb devices.
  SimulatePointerHidRemoved();
  SimulateKeyboardHidRemoved();
  EXPECT_EQ("searching", handler()->mouse_state_for_test());
  EXPECT_EQ("searching", handler()->keyboard_state_for_test());

  // The device names are reset in the revamped code.
  if (GetParam()) {
    EXPECT_TRUE(handler()->mouse_device_name_for_test().empty());
    EXPECT_TRUE(handler()->keyboard_device_name_for_test().empty());
  }
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  AssertHidDisconnectedCount(HidType::kSerialKeyboard, /*count=*/1);
  AssertHidDisconnectedCount(HidType::kSerialPointer, /*count=*/1);
  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  // TODO(crbug/1173782): use screen or JS state instead of handler()
  EXPECT_EQ("usb", handler()->mouse_state_for_test());
  EXPECT_EQ("usb", handler()->keyboard_state_for_test());
  EXPECT_EQ(kTestPointerName, handler()->mouse_device_name_for_test());
  EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  AssertHidConnectedCount(HidType::kUsbKeyboard, /*count=*/1);
  AssertHidConnectedCount(HidType::kUsbPointer, /*count=*/1);

  // Remove usb devices, add bluetooth devices.
  SimulatePointerHidRemoved();
  SimulateKeyboardHidRemoved();

  // The device names are reset in the revamped code.
  if (GetParam()) {
    EXPECT_TRUE(handler()->mouse_device_name_for_test().empty());
    EXPECT_TRUE(handler()->keyboard_device_name_for_test().empty());
  }
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  AssertHidDisconnectedCount(HidType::kUsbKeyboard, /*count=*/1);
  AssertHidDisconnectedCount(HidType::kUsbPointer, /*count=*/1);

  // The device states and names are set during pairing in the revamped
  // code.
  if (GetParam()) {
    SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::MOUSE);
    SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::KEYBOARD);
    EXPECT_EQ("pairing", handler()->mouse_state_for_test());
    EXPECT_EQ("pairing", handler()->keyboard_state_for_test());
    EXPECT_EQ(kTestPointerName, handler()->mouse_device_name_for_test());
    EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());
  }

  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  EXPECT_EQ("paired", handler()->mouse_state_for_test());
  EXPECT_EQ("paired", handler()->keyboard_state_for_test());
  EXPECT_EQ(kTestPointerName, handler()->mouse_device_name_for_test());
  EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  AssertHidConnectedCount(HidType::kBluetoothKeyboard, /*count=*/1);
  AssertHidConnectedCount(HidType::kBluetoothPointer, /*count=*/1);

  // Not applicable after the revamp.
  if (!GetParam()) {
    // Remove bluetooth devices.
    SimulatePointerHidRemoved();
    SimulateKeyboardHidRemoved();
    AssertHidDisconnectedCount(HidType::kBluetoothKeyboard, /*count=*/1);
    AssertHidDisconnectedCount(HidType::kBluetoothPointer, /*count=*/1);
  }

  ForceStopHidDetectionIfRevamp();
}

IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest,
                       BluetoothPairingDialog) {
  // This test is not applicable to the legacy screen.
  if (!GetParam())
    return;

  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();

  SimulateBluetoothDeviceDiscovered(device::BluetoothDeviceType::KEYBOARD);
  EXPECT_EQ("pairing", handler()->keyboard_state_for_test());
  EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());
  EXPECT_FALSE(handler()->num_keys_entered_expected_for_test());
  EXPECT_EQ(0, handler()->num_keys_entered_pin_code_for_test());
  EXPECT_TRUE(handler()->keyboard_pin_code_for_test().empty());
  test::OobeJS().ExpectDialogClosed(kHidPairingDialog);

  SimulatePairingCodeRequired();
  EXPECT_EQ("pairing", handler()->keyboard_state_for_test());
  EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());
  EXPECT_TRUE(handler()->num_keys_entered_expected_for_test());
  EXPECT_EQ(static_cast<uint8_t>(std::strlen(kTestPinCode)),
            handler()->num_keys_entered_pin_code_for_test());
  EXPECT_EQ(kTestPinCode, handler()->keyboard_pin_code_for_test());
  test::OobeJS().ExpectDialogOpen(kHidPairingDialog);
  EXPECT_EQ(kTestPinCode, test::OobeJS().GetAttributeString(
                              "code", kHidPairingDialogEnterCodePage));
  EXPECT_EQ(kTestKeyboardName,
            test::OobeJS().GetAttributeString("deviceName",
                                              kHidPairingDialogEnterCodePage));
  EXPECT_EQ(static_cast<int>(strlen(kTestPinCode)),
            test::OobeJS().GetAttributeInt("numKeysEntered",
                                           kHidPairingDialogEnterCodePage));

  SimulatePairingCodeNotRequired();
  EXPECT_EQ("pairing", handler()->keyboard_state_for_test());
  EXPECT_EQ(kTestKeyboardName, handler()->keyboard_device_name_for_test());
  EXPECT_FALSE(handler()->num_keys_entered_expected_for_test());
  test::OobeJS().ExpectDialogClosed(kHidPairingDialog);

  ForceStopHidDetectionIfRevamp();
}

IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest,
                       AddRemoveDevicesAfterScreen) {
  // This test is not applicable after the revamp.
  if (GetParam()) {
    ForceStopHidDetectionIfRevamp();
    return;
  }

  // NOTE: State strings match those in hid_detection_screen.cc.
  // No devices added yet
  EXPECT_EQ("searching", handler()->mouse_state_for_test());
  EXPECT_EQ("searching", handler()->keyboard_state_for_test());
  test::OobeJS().ExpectDisabledPath(kHidContinueButton);

  // Generic connection types. Unlike the pointing device, which may be a tablet
  // or touchscreen, the keyboard only reports usb and bluetooth states.
  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_SERIO);
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  AssertHidConnectedCount(HidType::kSerialPointer, /*count=*/1);

  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_SERIO);
  EXPECT_EQ("connected", handler()->mouse_state_for_test());
  EXPECT_EQ("usb", handler()->keyboard_state_for_test());
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  AssertHidConnectedCount(HidType::kSerialKeyboard, /*count=*/1);

  ContinueToWelcomeScreen();

  SimulatePointerHidRemoved();
  SimulateKeyboardHidRemoved();

  AssertHidDisconnectedCount(HidType::kSerialKeyboard, /*count=*/0);
  AssertHidDisconnectedCount(HidType::kSerialPointer, /*count=*/0);

  // Re-add the generic keyboard/mouse and make sure the count doesn't increase.
  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_SERIO);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_SERIO);

  AssertHidConnectedCount(HidType::kSerialPointer, /*count=*/1);
  AssertHidConnectedCount(HidType::kSerialKeyboard, /*count=*/1);

  SimulatePointerHidRemoved();
  SimulateKeyboardHidRemoved();

  // Make sure a not yet added device type also doesn't increment the count.
  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);

  AssertHidConnectedCount(HidType::kUsbKeyboard, /*count=*/0);
  AssertHidConnectedCount(HidType::kUsbPointer, /*count=*/0);
}

// Test that there isn't a crash when a device is removed that was never added.
// This is a regression test for b/235083051.
IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest,
                       BluetoothDeviceRemoveNeverAdded) {
  // This test is not applicable after the revamp.
  if (GetParam()) {
    ForceStopHidDetectionIfRevamp();
    return;
  }

  SimulatePointerHidRemoved();
  AssertHidDisconnectedCount(HidType::kUsbPointer, /*count=*/0);
  AssertHidDisconnectedCount(HidType::kSerialPointer, /*count=*/0);
}

// Test that if there is any Bluetooth device connected on HID screen, the
// Bluetooth adapter should not be disabled after advancing to the next screen.
IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest,
                       BluetoothDeviceConnected) {
  // This test is not applicable after the revamp.
  if (GetParam()) {
    ForceStopHidDetectionIfRevamp();
    return;
  }

  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();

  // Add a pair of USB mouse/keyboard so that `pointing_device_type_`
  // and `keyboard_type_` are
  // device::mojom::InputDeviceType::TYPE_USB.
  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);

  // Add another pair of Bluetooth mouse/keyboard.
  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_BLUETOOTH);

  EXPECT_CALL(*GetMockBluetoothAdapter(), SetPowered(false, _, _)).Times(0);
  ContinueToWelcomeScreen();
  testing::Mock::VerifyAndClear(&*GetMockBluetoothAdapter());
}

// Test that if there is no Bluetooth device connected on HID screen, the
// Bluetooth adapter should be disabled after advancing to the next screen.
IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest,
                       NoBluetoothDeviceConnected) {
  // This test is not applicable after the revamp.
  if (GetParam()) {
    ForceStopHidDetectionIfRevamp();
    return;
  }

  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();

  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);

  // The adapter should be powered off at this moment.
  EXPECT_CALL(*GetMockBluetoothAdapter(), SetPowered(false, _, _)).Times(1);
  ContinueToWelcomeScreen();
  testing::Mock::VerifyAndClear(&*GetMockBluetoothAdapter());
}

// Start without devices, connect them and proceed to the network screen.
// Network screen should be saved in the local state.
IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest, PRE_ResumableScreen) {
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
IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest, ResumableScreen) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  test::OobeJS().TapOnPath(kHidContinueButton);
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// Tests that the connected 'ticks' are shown when the devices are connected.
IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromeboxTest, TestTicks) {
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
    SetWaitUntilIdleAfterDeviceUpdate(false);
    SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
    SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  }
  ~HIDDetectionSkipTest() override = default;

 protected:
  base::HistogramTester histogram_tester;
};

INSTANTIATE_TEST_SUITE_P(All, HIDDetectionSkipTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(HIDDetectionSkipTest, BothDevicesPreConnected) {
  test::WaitForWelcomeScreen();
  AssertInitialHidsMissingCount(HidsMissing::kNone, /*count=*/1);
  EXPECT_FALSE(GetExitResult().has_value());
  histogram_tester.ExpectTotalCount("OOBE.HidDetectionScreen.HidConnected", 0);
}

class HIDDetectionDeviceOwnedTest : public HIDDetectionScreenChromeboxTest {
 private:
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

INSTANTIATE_TEST_SUITE_P(All, HIDDetectionDeviceOwnedTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(HIDDetectionDeviceOwnedTest, NoScreen) {
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
}

class HIDDetectionOobeCompletedUnowned
    : public HIDDetectionScreenChromeboxTest {
 private:
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
};

INSTANTIATE_TEST_SUITE_P(All,
                         HIDDetectionOobeCompletedUnowned,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(HIDDetectionOobeCompletedUnowned, ShowScreen) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  ForceStopHidDetectionIfRevamp();
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

INSTANTIATE_TEST_SUITE_P(All,
                         HIDDetectionScreenDisabledAfterRestartTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(HIDDetectionScreenDisabledAfterRestartTest,
                       PRE_SkipToUpdate) {
  test::WaitForWelcomeScreen();

  EXPECT_TRUE(StartupUtils::IsHIDDetectionScreenDisabledForTests());
  EXPECT_FALSE(WizardController::default_controller()->HasScreen(
      HIDDetectionView::kScreenId));
}

IN_PROC_BROWSER_TEST_P(HIDDetectionScreenDisabledAfterRestartTest,
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

class HIDDetectionScreenChromebaseTest
    : public OobeBaseTest,
      public testing::WithParamInterface<bool> {
 public:
  HIDDetectionScreenChromebaseTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kOobeHidDetectionRevamp);

      auto fake_hid_detection_manager =
          std::make_unique<hid_detection::FakeHidDetectionManager>();
      fake_hid_detection_manager_ = fake_hid_detection_manager.get();
      HIDDetectionScreen::OverrideHidDetectionManagerForTesting(
          std::move(fake_hid_detection_manager));

      fake_hid_detection_manager_->SetHidStatusTouchscreenDetected(true);
      return;
    }

    scoped_feature_list_.InitAndDisableFeature(
        features::kOobeHidDetectionRevamp);

    hid_controller_.set_wait_until_idle_after_device_update(false);
    hid_controller_.AddTouchscreen();
  }

 protected:
  void SimulatePointerHidConnected(device::mojom::InputDeviceType device_type) {
    if (GetParam()) {
      fake_hid_detection_manager_->SetHidStatusPointerMetadata(
          {GetHidInputState(device_type), kTestPointerName});
      return;
    }
    hid_controller_.AddMouse(device_type);
  }

  void SimulateKeyboardHidConnected(
      device::mojom::InputDeviceType device_type) {
    if (GetParam()) {
      fake_hid_detection_manager_->SetHidStatusKeyboardMetadata(
          {GetHidInputState(device_type), kTestKeyboardName});
      return;
    }
    hid_controller_.AddKeyboard(device_type);
  }

  void SimulatePointerHidRemoved() {
    if (GetParam()) {
      fake_hid_detection_manager_->SetHidStatusPointerMetadata(
          {InputState::kSearching, /*detected_hid_name=*/""});
      return;
    }
    hid_controller_.RemoveMouse();
  }

  void SimulateKeyboardHidRemoved() {
    if (GetParam()) {
      fake_hid_detection_manager_->SetHidStatusKeyboardMetadata(
          {InputState::kSearching, /*detected_hid_name=*/""});
      return;
    }
    hid_controller_.RemoveKeyboard();
  }

  // HID detection must be stopped before HidDetectionManager is destroyed. This
  // should be called in tests that start HID detection but don't continue to
  // the next screen.
  void ForceStopHidDetectionIfRevamp() {
    if (GetParam()) {
      test::OobeJS().CreateVisibilityWaiter(true, kHidContinueButton)->Wait();
      test::OobeJS().TapOnPath(kHidContinueButton);
    }
  }

 private:
  test::HIDControllerMixin hid_controller_{&mixin_host_};
  raw_ptr<hid_detection::FakeHidDetectionManager, ExperimentalAsh>
      fake_hid_detection_manager_;

  // Set device type to a Chromebase with a touch screen.
  // This should show the HIDDetectionScreen with the continue button
  // always enabled, since the user can complete all of OOBE steps
  // with only a touchscreen.
  base::test::ScopedChromeOSVersionInfo version_{"DEVICETYPE=CHROMEBASE",
                                                 base::Time::Now()};

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HIDDetectionScreenChromebaseTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(HIDDetectionScreenChromebaseTest, TouchscreenDetected) {
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

  ForceStopHidDetectionIfRevamp();
}

class HIDDetectionScreenPreConnectedDeviceTest
    : public HIDDetectionScreenChromeboxTest {
 public:
  HIDDetectionScreenPreConnectedDeviceTest() {
    SetWaitUntilIdleAfterDeviceUpdate(false);
    SimulatePointerHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HIDDetectionScreenPreConnectedDeviceTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(HIDDetectionScreenPreConnectedDeviceTest,
                       MousePreConnected) {
  // Continue button should be enabled if at least one device is connected.
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(kHidTouchscreenEntry);
  test::OobeJS().CreateVisibilityWaiter(true, kHidMouseTick)->Wait();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  AssertInitialHidsMissingCount(HidsMissing::kKeyboard, /*count=*/1);
  AssertHidConnectedCount(HidType::kUsbPointer, /*count=*/0);
  AssertHidConnectedCount(HidType::kUsbKeyboard, /*count=*/0);

  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  test::OobeJS().CreateVisibilityWaiter(true, kHidKeyboardTick)->Wait();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  AssertHidConnectedCount(HidType::kUsbPointer, /*count=*/0);
  AssertHidConnectedCount(HidType::kUsbKeyboard, /*count=*/1);

  SimulateKeyboardHidConnected(device::mojom::InputDeviceType::TYPE_USB);
  test::OobeJS().CreateVisibilityWaiter(true, kHidKeyboardTick)->Wait();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);
  AssertHidConnectedCount(HidType::kUsbPointer, /*count=*/0);
  AssertHidConnectedCount(HidType::kUsbKeyboard, /*count=*/2);

  ForceStopHidDetectionIfRevamp();
}

class HIDDetectionPreconnectedBTTest : public HIDDetectionScreenChromeboxTest {
 public:
  HIDDetectionPreconnectedBTTest() {
    SetWaitUntilIdleAfterDeviceUpdate(false);
    SimulateKeyboardHidConnected(
        device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  }
};

INSTANTIATE_TEST_SUITE_P(All, HIDDetectionPreconnectedBTTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(HIDDetectionPreconnectedBTTest,
                       BTKeyboardDevicePreConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(kHidTouchscreenEntry);
  test::OobeJS().CreateVisibilityWaiter(true, kHidKeyboardTick)->Wait();
  test::OobeJS().ExpectEnabledPath(kHidContinueButton);

  AssertHidConnectedCount(HidType::kUsbPointer, /*count=*/0);
  AssertHidConnectedCount(HidType::kUsbKeyboard, /*count=*/0);
  AssertInitialHidsMissingCount(HidsMissing::kPointer, /*count=*/1);

  EXPECT_FALSE(GetExitResult().has_value());
  ForceStopHidDetectionIfRevamp();
}

}  // namespace ash
