// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_status_ui_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/toast_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/bluetooth_config/fake_bluetooth_device_status_notifier.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

using chromeos::bluetooth_config::mojom::BatteryProperties;
using chromeos::bluetooth_config::mojom::BluetoothDeviceProperties;
using chromeos::bluetooth_config::mojom::DeviceBatteryInfo;
using chromeos::bluetooth_config::mojom::DeviceBatteryInfoPtr;
using chromeos::bluetooth_config::mojom::DeviceConnectionState;
using chromeos::bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;
using testing::NiceMock;

namespace ash {

namespace {

class MockBluetoothDeviceStatusUiHandler
    : public BluetoothDeviceStatusUiHandler {
 public:
  MOCK_METHOD(void, ShowToast, (const ash::ToastData& toast_data), (override));
};

}  // namespace

class BluetoothDeviceStatusUiHandlerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    feature_list_.InitAndEnableFeature(features::kBluetoothRevamp);
    device_status_ui_handler_ =
        std::make_unique<NiceMock<MockBluetoothDeviceStatusUiHandler>>();
    base::RunLoop().RunUntilIdle();
  }

  MockBluetoothDeviceStatusUiHandler& device_status_ui_handler() {
    return *device_status_ui_handler_;
  }

  void SetPairedDevices(
      std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices) {
    fake_device_status_notifier()->SetNewlyPairedDevices(
        std::move(paired_devices));
    base::RunLoop().RunUntilIdle();
  }

  void SetConnectedDevices(
      std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices) {
    fake_device_status_notifier()->SetConnectedDevices(
        std::move(paired_devices));
    base::RunLoop().RunUntilIdle();
  }

  void SetDisconnectedDevices(
      std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices) {
    fake_device_status_notifier()->SetDisconnectedDevices(
        std::move(paired_devices));
    base::RunLoop().RunUntilIdle();
  }

  std::vector<PairedBluetoothDevicePropertiesPtr> GetPairedDevices() {
    auto paired_device = PairedBluetoothDeviceProperties::New();
    paired_device->nickname = "Beats X";
    paired_device->device_properties = BluetoothDeviceProperties::New();

    std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices;
    paired_devices.push_back(std::move(paired_device));
    return paired_devices;
  }

 private:
  chromeos::bluetooth_config::FakeBluetoothDeviceStatusNotifier*
  fake_device_status_notifier() {
    return scoped_bluetooth_config_test_helper_
        .fake_bluetooth_device_status_notifier();
  }

  std::unique_ptr<MockBluetoothDeviceStatusUiHandler> device_status_ui_handler_;
  base::test::ScopedFeatureList feature_list_;
  chromeos::bluetooth_config::ScopedBluetoothConfigTestHelper
      scoped_bluetooth_config_test_helper_;
};

TEST_F(BluetoothDeviceStatusUiHandlerTest, PairedDevice) {
  EXPECT_CALL(device_status_ui_handler(), ShowToast);
  SetPairedDevices(GetPairedDevices());
}

TEST_F(BluetoothDeviceStatusUiHandlerTest, ConnectedDevice) {
  EXPECT_CALL(device_status_ui_handler(), ShowToast);
  SetConnectedDevices(GetPairedDevices());
}

TEST_F(BluetoothDeviceStatusUiHandlerTest, DisconnectedDevice) {
  EXPECT_CALL(device_status_ui_handler(), ShowToast);
  SetDisconnectedDevices(GetPairedDevices());
}

}  // namespace ash