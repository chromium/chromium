// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/unified_bluetooth_detailed_view_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/bluetooth/bluetooth_detailed_view_legacy.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "ui/views/view.h"

namespace ash {

namespace {

const base::TimeDelta kUpdateFrequencyMs = base::Milliseconds(1000);

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
    // These tests should only be run with the kBluetoothRevamp feature flag is
    // disabled, and so we force it off here and ensure that the local state
    // prefs that would have been registered had the feature flag been off are
    // registered.
    if (ash::features::IsBluetoothRevampEnabled()) {
      feature_list_.InitAndDisableFeature(features::kBluetoothRevamp);
      BluetoothPowerController::RegisterLocalStatePrefs(
          local_state()->registry());
    }

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

    tray_model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
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
    props.device_path = base::StringPrintf("/fake/hci0/dev%02d", next_id_);
    props.device_address = base::StringPrintf("00:00:00:00:00:%02d", next_id_);
    props.device_name = "Test Device";
    props.device_class = 0x01;
    device_client_->CreateDeviceWithProperties(
        dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
        props);
    next_id_++;
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
  base::test::ScopedFeatureList feature_list_;
  bluez::FakeBluetoothAdapterClient* adapter_client_;
  bluez::FakeBluetoothDeviceClient* device_client_;
  scoped_refptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  std::unique_ptr<UnifiedBluetoothDetailedViewController>
      bt_detailed_view_controller_;
  int next_id_ = 1;
};

TEST_F(UnifiedBluetoothDetailedViewControllerTest, UpdateScrollListTest) {
  std::unique_ptr<BluetoothDetailedViewLegacy> bluetooth_detailed_view =
      base::WrapUnique(static_cast<BluetoothDetailedViewLegacy*>(
          bt_detailed_view_controller()->CreateView()));
  AddTestDevice();
  task_environment()->FastForwardBy(kUpdateFrequencyMs);

  // Verify that default devices simulated by FakeBluetoothDeviceClient are
  // displayed.
  const views::View* scroll_content = bluetooth_detailed_view->GetViewByID(
      BluetoothDetailedViewLegacy::kScrollContentID);
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

  std::unique_ptr<BluetoothDetailedViewLegacy> bluetooth_detailed_view =
      base::WrapUnique(static_cast<BluetoothDetailedViewLegacy*>(
          bt_detailed_view_controller()->CreateView()));
  task_environment()->FastForwardBy(kUpdateFrequencyMs);

  const views::View* scroll_content = bluetooth_detailed_view->GetViewByID(
      BluetoothDetailedViewLegacy::kScrollContentID);
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
