// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/unified_bluetooth_detailed_view_controller.h"

#include <memory>

#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "ui/views/view.h"

namespace ash {

namespace {

const base::TimeDelta kUpdateFrequencyMs =
    base::TimeDelta::FromMilliseconds(1000);

}  // namespace

class UnifiedBluetoothDetailedViewControllerTest : public AshTestBase {
 public:
  UnifiedBluetoothDetailedViewControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  UnifiedBluetoothDetailedViewControllerTest(
      const UnifiedBluetoothDetailedViewControllerTest&) = delete;
  UnifiedBluetoothDetailedViewControllerTest& operator=(
      const UnifiedBluetoothDetailedViewControllerTest&) = delete;
  ~UnifiedBluetoothDetailedViewControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Set fake adapter client to powered-on and initialize with zero simulation
    // interval.
    adapter_client_ = static_cast<bluez::FakeBluetoothAdapterClient*>(
        bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient());
    adapter_client_->SetSimulationIntervalMs(0);
    SetAdapterPowered(true);

    // Enable fake device client and initialize with zero simulation interval.
    device_client_ = static_cast<bluez::FakeBluetoothDeviceClient*>(
        bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient());
    device_client_->SetSimulationIntervalMs(0);
    task_environment()->RunUntilIdle();

    tray_model_ = std::make_unique<UnifiedSystemTrayModel>(nullptr);
    tray_controller_ =
        std::make_unique<UnifiedSystemTrayController>(tray_model_.get());
    bt_detailed_view_controller_ =
        std::make_unique<UnifiedBluetoothDetailedViewController>(
            tray_controller_.get());
  }

  void TearDown() override {
    bt_detailed_view_controller_.reset();
    tray_controller_.reset();
    tray_model_.reset();
    AshTestBase::TearDown();
  }

  void SetAdapterPowered(bool powered) {
    adapter_client_
        ->GetProperties(
            dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
        ->powered.ReplaceValue(powered);
    task_environment()->RunUntilIdle();
  }

  void AddTestDevice() {
    bluez::FakeBluetoothDeviceClient::IncomingDeviceProperties props;
    props.device_path = "/fake/hci0/dev123";
    props.device_address = "00:00:00:00:00:01";
    props.device_name = "Test Device";
    props.device_class = 0x000104;
    device_client_->CreateDeviceWithProperties(
        dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
        props);
  }

  void RemoveAllDevices() {
    dbus::ObjectPath fake_adapter_path(
        bluez::FakeBluetoothAdapterClient::kAdapterPath);
    std::vector<dbus::ObjectPath> device_paths =
        device_client_->GetDevicesForAdapter(fake_adapter_path);
    for (auto& device_path : device_paths) {
      device_client_->RemoveDevice(fake_adapter_path, device_path);
    }
  }

  UnifiedBluetoothDetailedViewController* bt_detailed_view_controller() {
    return bt_detailed_view_controller_.get();
  }

  bluez::FakeBluetoothAdapterClient* adapter_client() {
    return adapter_client_;
  }

 private:
  bluez::FakeBluetoothAdapterClient* adapter_client_;
  bluez::FakeBluetoothDeviceClient* device_client_;
  std::unique_ptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  std::unique_ptr<UnifiedBluetoothDetailedViewController>
      bt_detailed_view_controller_;
};

TEST_F(UnifiedBluetoothDetailedViewControllerTest, UpdateScrollListTest) {
  tray::BluetoothDetailedView* bluetooth_detailed_view =
      static_cast<tray::BluetoothDetailedView*>(
          bt_detailed_view_controller()->CreateView());
  task_environment()->FastForwardBy(kUpdateFrequencyMs);

  // Verify that default devices simulated by FakeBluetoothDeviceClient are
  // displayed.
  const views::View* scroll_content = bluetooth_detailed_view->GetViewByID(
      tray::BluetoothDetailedView::kScrollContentID);
  const size_t scroll_content_size = scroll_content->children().size();
  // Expect at least 1 paired device, 1 unpaired device and 2 headers.
  EXPECT_GE(scroll_content_size, 4u);

  // Fast forward to next bluetooth list sync and verify that child views
  // are re-used after update.
  views::View* scroll_content_child = scroll_content->children()[1];
  task_environment()->FastForwardBy(kUpdateFrequencyMs);
  EXPECT_EQ(scroll_content_child, scroll_content->children()[1]);

  // Verify that newly added devices is displayed.
  AddTestDevice();
  task_environment()->FastForwardBy(kUpdateFrequencyMs);
  EXPECT_EQ(scroll_content_size + 1u, scroll_content->children().size());
}

TEST_F(UnifiedBluetoothDetailedViewControllerTest,
       UpdateScrollListPowerCycleTest) {
  // Disable discovery simulation and remove all existing
  // devices so that only the "Scanning" message is displayed.
  adapter_client()->SetDiscoverySimulation(false);
  RemoveAllDevices();

  tray::BluetoothDetailedView* bluetooth_detailed_view =
      static_cast<tray::BluetoothDetailedView*>(
          bt_detailed_view_controller()->CreateView());
  task_environment()->FastForwardBy(kUpdateFrequencyMs);

  const views::View* scroll_content = bluetooth_detailed_view->GetViewByID(
      tray::BluetoothDetailedView::kScrollContentID);
  // Only the scanning message should be displayed.
  EXPECT_EQ(1u, scroll_content->children().size());

  // Verify that if bluetooth is powered off and back on again
  // the scroll list is cleared and populated back again properly.
  SetAdapterPowered(false);
  EXPECT_EQ(0u, scroll_content->children().size());
  SetAdapterPowered(true);
  EXPECT_EQ(1u, scroll_content->children().size());
}

}  // namespace ash