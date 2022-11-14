// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_status_ui_handler.h"

#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "chromeos/ash/services/bluetooth_config/fake_bluetooth_device_status_notifier.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;
using ::testing::NiceMock;

class MockBluetoothDeviceStatusUiHandler
    : public BluetoothDeviceStatusUiHandler {
 public:
  MOCK_METHOD(void, ShowToast, (ash::ToastData toast_data), (override));
};

}  // namespace

class BluetoothDeviceStatusUiHandlerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    device_status_ui_handler_ =
        std::make_unique<NiceMock<MockBluetoothDeviceStatusUiHandler>>();
    base::RunLoop().RunUntilIdle();
  }

  MockBluetoothDeviceStatusUiHandler& device_status_ui_handler() {
    return *device_status_ui_handler_;
  }

  void SetPairedDevice(PairedBluetoothDevicePropertiesPtr paired_device) {
    fake_device_status_notifier()->SetNewlyPairedDevice(
        std::move(paired_device));
    base::RunLoop().RunUntilIdle();
  }

  void SetConnectedDevice(PairedBluetoothDevicePropertiesPtr paired_device) {
    fake_device_status_notifier()->SetConnectedDevice(std::move(paired_device));
    base::RunLoop().RunUntilIdle();
  }

  void SetDisconnectedDevice(PairedBluetoothDevicePropertiesPtr paired_device) {
    fake_device_status_notifier()->SetDisconnectedDevice(
        std::move(paired_device));
    base::RunLoop().RunUntilIdle();
  }

  PairedBluetoothDevicePropertiesPtr GetPairedDevice() {
    auto paired_device = PairedBluetoothDeviceProperties::New();
    paired_device->nickname = "Beats X";
    paired_device->device_properties = BluetoothDeviceProperties::New();
    return paired_device;
  }

 private:
  bluetooth_config::FakeBluetoothDeviceStatusNotifier*
  fake_device_status_notifier() {
    return ash_test_helper()
        ->bluetooth_config_test_helper()
        ->fake_bluetooth_device_status_notifier();
  }

  std::unique_ptr<MockBluetoothDeviceStatusUiHandler> device_status_ui_handler_;
};

TEST_F(BluetoothDeviceStatusUiHandlerTest, PairedDevice) {
  EXPECT_CALL(device_status_ui_handler(), ShowToast);
  SetPairedDevice(GetPairedDevice());
}

TEST_F(BluetoothDeviceStatusUiHandlerTest, ConnectedDevice) {
  EXPECT_CALL(device_status_ui_handler(), ShowToast);
  SetConnectedDevice(GetPairedDevice());
}

TEST_F(BluetoothDeviceStatusUiHandlerTest, DisconnectedDevice) {
  EXPECT_CALL(device_status_ui_handler(), ShowToast);
  SetDisconnectedDevice(GetPairedDevice());
}

}  // namespace ash
