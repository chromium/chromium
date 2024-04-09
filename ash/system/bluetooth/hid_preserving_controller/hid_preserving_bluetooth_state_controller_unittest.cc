// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_state_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/system/bluetooth/hid_preserving_controller/fake_disable_bluetooth_dialog_controller.h"
#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_metrics.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

namespace {
using bluetooth_config::ScopedBluetoothConfigTestHelper;
using bluetooth_config::mojom::BluetoothSystemState;

// Logitech Vendor ID
const uint16_t kLogitechVID = 0x046d;

// Logitech MX Master 3S Product ID (Bluetooth)
const uint16_t KMousePID = 0xb034;

// Logitech MX Keys Product ID (Bluetooth)
const uint16_t KKeyboardPID = 0xb35b;

const std::string kSampleMouseBluetooth = "kSampleMouseBluetooth";
const std::string kSampleKeyboardBluetooth = "kSampleKeyboardBluetooth";

const ui::KeyboardDevice GetSampleKeyboardBluetooth() {
  return {10,
          ui::INPUT_DEVICE_BLUETOOTH,
          kSampleKeyboardBluetooth,
          /* phys= */ "",
          base::FilePath(),
          kLogitechVID,
          KKeyboardPID,
          /* version= */ 0};
}

const ui::InputDevice GetSampleMouseBluetoothDuplicate() {
  return {110,
          ui::INPUT_DEVICE_BLUETOOTH,
          "kSampleMouseBluetoothDuplicate",
          /* phys= */ "",
          base::FilePath(),
          kLogitechVID,
          KKeyboardPID,
          /* version= */ 0};
}

const ui::KeyboardDevice GetSampleKeyboardUsb() {
  return {15, ui::INPUT_DEVICE_USB, "kSampleKeyboardUsb"};
}

const ui::InputDevice GetSampleMouseUsb() {
  return {20, ui::INPUT_DEVICE_USB, "kSampleMouseUsb"};
}

const ui::InputDevice GetSampleMouseBluetooth() {
  return {25,
          ui::INPUT_DEVICE_BLUETOOTH,
          kSampleMouseBluetooth,
          /* phys= */ "",
          base::FilePath(),
          kLogitechVID,
          KMousePID,
          /* version= */ 0};
}

const ui::KeyboardDevice GetSampleKeyboardBluetoothDuplicate() {
  return {100,
          ui::INPUT_DEVICE_BLUETOOTH,
          "kSampleKeyboardBluetoothDuplicate",
          /* phys= */ "",
          base::FilePath(),
          kLogitechVID,
          KMousePID,
          /* version= */ 0};
}

const ui::KeyboardDevice GetSampleMouseInternal() {
  return {30, ui::INPUT_DEVICE_INTERNAL, "kSampleMouseInternal"};
}

const ui::TouchpadDevice GetSampleTouchpadInternal() {
  return {35, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
          "kSampleTouchpadInternal"};
}

const ui::TouchpadDevice GetSampleTouchpadBluetooth() {
  return {40, ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
          "kSampleTouchpadBluetooth"};
}

const ui::TouchscreenDevice GetSampleTouchscreenBluetooth() {
  return {45, ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
          "kSampleTouchscreenBluetooth", gfx::Size(123, 456), 1};
}

const ui::TouchscreenDevice GetSampleTouchscreenInternal() {
  return {50, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
          "kSampleTouchscreenInternal", gfx::Size(123, 456), 1};
}

const ui::InputDevice GetSamplePointingStickBluetooth() {
  return {55, ui::INPUT_DEVICE_BLUETOOTH, "kSamplePointingStickBluetooth"};
}

const ui::InputDevice GetSamplePointingStickInternal() {
  return {60, ui::INPUT_DEVICE_INTERNAL, "kSamplePointingStickInternal"};
}

const ui::InputDevice GetSampleGraphicsTabletBluetooth() {
  return {65, ui::INPUT_DEVICE_BLUETOOTH, "kSampleGraphicsTabletBluetooth"};
}

const ui::InputDevice GetSampleGraphicsTabletInternal() {
  return {70, ui::INPUT_DEVICE_INTERNAL, "kSampleGraphicsTabletInternal"};
}

}  // namespace

class HidPreservingBluetoothStateControllerTest : public AshTestBase {
 public:
  struct ExpectedHistogramState {
    size_t disabled_bluetooth_dialog_shown_count = 0u;
    size_t disabled_bluetooth_dialog_not_shown_count = 0u;
    size_t user_action_keep_on_count = 0u;
    size_t user_action_turn_off_count = 0u;
    size_t disabled_bluetooth_dialog_source_os_settings_count = 0u;
    size_t disabled_bluetooth_dialog_source_quick_settings_count = 0u;
  };

  void SetUp() override {
    AshTestBase::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        features::kBluetoothDisconnectWarning);

    hid_preserving_bluetooth_state_controller_ =
        std::make_unique<HidPreservingBluetoothStateController>();

    std::unique_ptr<FakeDisableBluetoothDialogController>
        disable_bluetooth_dialog_controller =
            std::make_unique<FakeDisableBluetoothDialogController>();

    hid_preserving_bluetooth_state_controller_
        ->SetDisableBluetoothDialogControllerForTest(
            std::move(disable_bluetooth_dialog_controller));
  }

  void SetBluetoothAdapterState(BluetoothSystemState system_state) {
    bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->SetSystemState(system_state);
    base::RunLoop().RunUntilIdle();
  }

  void TryToSetBluetoothEnabledState(bool enabled,
                                     mojom::HidWarningDialogSource source) {
    hid_preserving_bluetooth_state_controller_->TryToSetBluetoothEnabledState(
        enabled, source);
    base::RunLoop().RunUntilIdle();
  }

  void CompleteShowDialog(size_t called_count, bool show_dialog_result) {
    EXPECT_EQ(GetDisabledBluetoothDialogController()->show_dialog_call_count(),
              called_count);
    GetDisabledBluetoothDialogController()->CompleteShowDialogCallback(
        show_dialog_result);
  }

  FakeDisableBluetoothDialogController* GetDisabledBluetoothDialogController() {
    return static_cast<FakeDisableBluetoothDialogController*>(
        hid_preserving_bluetooth_state_controller_
            ->GetDisabledBluetoothDialogForTesting());
  }

  BluetoothSystemState GetBluetoothAdapterState() {
    return bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->GetAdapterState();
  }

  DisableBluetoothDialogController::DeviceNamesList* GetShownDevicesList() {
    return hid_preserving_bluetooth_state_controller_
        ->device_names_for_testing();
  }

  void CheckHistogramState(const ExpectedHistogramState& state) {
    histogram_tester_.ExpectBucketCount(
        bluetooth::kPoweredDisableDialogBehavior,
        bluetooth::DisabledBehavior::kWarningDialogShown,
        state.disabled_bluetooth_dialog_shown_count);
    histogram_tester_.ExpectBucketCount(
        bluetooth::kPoweredDisableDialogBehavior,
        bluetooth::DisabledBehavior::kWarningDialogNotShown,
        state.disabled_bluetooth_dialog_not_shown_count);
    histogram_tester_.ExpectBucketCount(bluetooth::kUserAction,
                                        bluetooth::UserAction::kKeepOn,
                                        state.user_action_keep_on_count);
    histogram_tester_.ExpectBucketCount(bluetooth::kUserAction,
                                        bluetooth::UserAction::kTurnOff,
                                        state.user_action_turn_off_count);
    histogram_tester_.ExpectBucketCount(
        bluetooth::kDialogSource, bluetooth::DialogSource::kQuickSettings,
        state.disabled_bluetooth_dialog_source_quick_settings_count);
    histogram_tester_.ExpectBucketCount(
        bluetooth::kDialogSource, bluetooth::DialogSource::kOsSettings,
        state.disabled_bluetooth_dialog_source_os_settings_count);
  }

 private:
  ScopedBluetoothConfigTestHelper* bluetooth_config_test_helper() {
    return ash_test_helper()->bluetooth_config_test_helper();
  }

  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<HidPreservingBluetoothStateController>
      hid_preserving_bluetooth_state_controller_;
};

TEST_F(HidPreservingBluetoothStateControllerTest, BluetoothEnabled) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kDisabled);
  EXPECT_EQ(BluetoothSystemState::kDisabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  TryToSetBluetoothEnabledState(/*enabled=*/true,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithTouchScreenDevice) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());

  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(screens);

  TryToSetBluetoothEnabledState(/*enabled=*/false,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_not_shown_count++;
  CheckHistogramState(expected_state);
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithMouseDevices) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  ui::DeviceDataManagerTestApi().SetMouseDevices({GetSampleMouseUsb(),
                                                  GetSampleMouseBluetooth(),
                                                  GetSampleMouseInternal()});
  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(/*enabled=*/false,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_not_shown_count++;
  CheckHistogramState(expected_state);
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithDuplicateMouseDevices) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  ui::DeviceDataManagerTestApi().SetMouseDevices({GetSampleMouseBluetooth()});
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {GetSampleKeyboardBluetoothDuplicate()});

  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(/*enabled=*/false,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_source_quick_settings_count++;
  expected_state.disabled_bluetooth_dialog_shown_count++;
  CheckHistogramState(expected_state);

  DisableBluetoothDialogController::DeviceNamesList* device_names =
      GetShownDevicesList();
  EXPECT_EQ(1u, device_names->size());
  EXPECT_EQ(device_names->front(), kSampleMouseBluetooth);
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithDuplicateKeyboardDevices) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {GetSampleMouseBluetoothDuplicate()});
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {GetSampleKeyboardBluetooth()});

  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(/*enabled=*/false,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_source_quick_settings_count++;
  expected_state.disabled_bluetooth_dialog_shown_count++;
  CheckHistogramState(expected_state);

  DisableBluetoothDialogController::DeviceNamesList* device_names =
      GetShownDevicesList();
  EXPECT_EQ(1u, device_names->size());
  EXPECT_EQ(device_names->front(), kSampleKeyboardBluetooth);
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithTouchPadDevices) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  ui::DeviceDataManagerTestApi().SetTouchpadDevices(
      {GetSampleTouchpadInternal(), GetSampleTouchpadBluetooth()});
  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(/*enabled=*/false,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_not_shown_count++;
  CheckHistogramState(expected_state);
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithKeyboardDevices) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {GetSampleKeyboardBluetooth(), GetSampleKeyboardUsb()});
  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(/*enabled=*/false,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_not_shown_count++;
  CheckHistogramState(expected_state);
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithTouchscreenDevices) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {GetSampleTouchscreenBluetooth(), GetSampleTouchscreenInternal()});
  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(/*enabled=*/false,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_not_shown_count++;
  CheckHistogramState(expected_state);
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithPointingStickDevices) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  ui::DeviceDataManagerTestApi().SetPointingStickDevices(
      {GetSamplePointingStickBluetooth(), GetSamplePointingStickInternal()});
  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(/*enabled=*/false,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_not_shown_count++;
  CheckHistogramState(expected_state);
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothNoBluetoothDevices) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);
  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  TryToSetBluetoothEnabledState(false,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_not_shown_count++;
  CheckHistogramState(expected_state);

  TryToSetBluetoothEnabledState(true,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  TryToSetBluetoothEnabledState(false,
                                mojom::HidWarningDialogSource::kOsSettings);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_not_shown_count++;
  CheckHistogramState(expected_state);
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithOnlyBluetoothDevices_AllResults) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  ui::DeviceDataManagerTestApi().SetMouseDevices({GetSampleMouseBluetooth()});
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {GetSampleKeyboardBluetooth()});
  ui::DeviceDataManagerTestApi().SetTouchpadDevices(
      {GetSampleTouchpadBluetooth()});
  ui::DeviceDataManagerTestApi().SetPointingStickDevices(
      {GetSamplePointingStickBluetooth()});
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {GetSampleTouchscreenBluetooth()});
  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
      {GetSampleGraphicsTabletBluetooth()});
  base::RunLoop().RunUntilIdle();

  size_t called_count = 0u;
  // Try to set Bluetooth enabled state with different sources and results.
  for (const auto& [source, result] :
       std::vector<std::pair<mojom::HidWarningDialogSource, bool>>{
           {mojom::HidWarningDialogSource::kQuickSettings, true},
           {mojom::HidWarningDialogSource::kQuickSettings, false},
           {mojom::HidWarningDialogSource::kOsSettings, true},
           {mojom::HidWarningDialogSource::kOsSettings, false}}) {
    SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
    base::RunLoop().RunUntilIdle();
    TryToSetBluetoothEnabledState(false, source);
    expected_state.disabled_bluetooth_dialog_shown_count++;
    if (source == mojom::HidWarningDialogSource::kQuickSettings) {
      expected_state.disabled_bluetooth_dialog_source_quick_settings_count++;
    } else {
      expected_state.disabled_bluetooth_dialog_source_os_settings_count++;
    }
    CheckHistogramState(expected_state);

    CompleteShowDialog(++called_count, /*show_dialog_result=*/result);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(result ? BluetoothSystemState::kDisabling
                     : BluetoothSystemState::kEnabled,
              GetBluetoothAdapterState());

    expected_state.user_action_turn_off_count += result;
    expected_state.user_action_keep_on_count += !result;
    CheckHistogramState(expected_state);
  }
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithGraphicsTabletDevices) {
  ExpectedHistogramState expected_state;
  CheckHistogramState(expected_state);

  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  CheckHistogramState(expected_state);

  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
      {GetSampleGraphicsTabletBluetooth(), GetSampleGraphicsTabletInternal()});
  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(/*enabled=*/false,
                                mojom::HidWarningDialogSource::kQuickSettings);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
  expected_state.disabled_bluetooth_dialog_not_shown_count++;
  CheckHistogramState(expected_state);
}

}  // namespace ash
