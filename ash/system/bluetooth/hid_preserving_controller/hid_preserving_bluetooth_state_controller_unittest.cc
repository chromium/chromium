// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_state_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/system/bluetooth/hid_preserving_controller/fake_disable_bluetooth_dialog_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ash {

namespace {
using bluetooth_config::ScopedBluetoothConfigTestHelper;
using bluetooth_config::mojom::BluetoothSystemState;

const ui::KeyboardDevice GetSampleKeyboardBluetooth() {
  return {10, ui::INPUT_DEVICE_BLUETOOTH, "kSampleKeyboardBluetooth"};
}

const ui::KeyboardDevice GetSampleKeyboardUsb() {
  return {15, ui::INPUT_DEVICE_USB, "kSampleKeyboardUsb"};
}

const ui::KeyboardDevice GetSampleMouseUsb() {
  return {20, ui::INPUT_DEVICE_USB, "kSampleMouseUsb"};
}

const ui::KeyboardDevice GetSampleMouseBluetooth() {
  return {25, ui::INPUT_DEVICE_BLUETOOTH, "kSampleMouseBluetooth"};
}

const ui::KeyboardDevice GetSampleMouseInternal() {
  return {30, ui::INPUT_DEVICE_INTERNAL, "kSampleMouseInternal"};
}

}  // namespace

class HidPreservingBluetoothStateControllerTest : public AshTestBase {
 public:
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

  void TryToSetBluetoothEnabledState(bool enabled) {
    hid_preserving_bluetooth_state_controller_->TryToSetBluetoothEnabledState(
        enabled);
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

 private:
  ScopedBluetoothConfigTestHelper* bluetooth_config_test_helper() {
    return ash_test_helper()->bluetooth_config_test_helper();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<HidPreservingBluetoothStateController>
      hid_preserving_bluetooth_state_controller_;
};

TEST_F(HidPreservingBluetoothStateControllerTest, BluetoothEnabled) {
  SetBluetoothAdapterState(BluetoothSystemState::kDisabled);
  EXPECT_EQ(BluetoothSystemState::kDisabled, GetBluetoothAdapterState());
  TryToSetBluetoothEnabledState(/*enabled=*/true);
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetBluetoothAdapterState());
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithTouchScreenDevice) {
  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());

  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(screens);

  TryToSetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithMouseDevices) {
  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());

  ui::DeviceDataManagerTestApi().SetMouseDevices({GetSampleMouseUsb(),
                                                  GetSampleMouseBluetooth(),
                                                  GetSampleMouseInternal()});
  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithKeyboardDevices) {
  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());

  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {GetSampleKeyboardBluetooth(), GetSampleKeyboardUsb()});
  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithOnlyBluetoothDevices_resultTrue) {
  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());

  ui::DeviceDataManagerTestApi().SetMouseDevices({GetSampleMouseBluetooth()});
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {GetSampleKeyboardBluetooth()});
  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(false);
  CompleteShowDialog(/*called_count=*/1u, /*show_dialog_result=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothWithOnlyBluetoothDevices_resultFalse) {
  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());

  ui::DeviceDataManagerTestApi().SetMouseDevices({GetSampleMouseBluetooth()});
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {GetSampleKeyboardBluetooth()});
  base::RunLoop().RunUntilIdle();

  TryToSetBluetoothEnabledState(false);
  CompleteShowDialog(/*called_count=*/1u, /*show_dialog_result=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
}

TEST_F(HidPreservingBluetoothStateControllerTest,
       DisableBluetoothNoBluetoothDevices) {
  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());

  TryToSetBluetoothEnabledState(false);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetBluetoothAdapterState());
}

}  // namespace ash
