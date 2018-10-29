// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth_helper_legacy.h"

#include <string>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "dbus/object_path.h"
#include "device/base/features.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"

using bluez::BluezDBusManager;
using bluez::FakeBluetoothAdapterClient;
using bluez::FakeBluetoothDeviceClient;

namespace ash {
namespace {

// Returns true if device with |address| exists in the filtered device list.
// Returns false otherwise.
bool ExistInFilteredDevices(const std::string& address,
                            BluetoothDeviceList filtered_devices) {
  for (const auto& device : filtered_devices) {
    if (device.address == address)
      return true;
  }
  return false;
}

using TrayBluetoothHelperLegacyTest = AshTestBase;

// Tests basic functionality.
TEST_F(TrayBluetoothHelperLegacyTest, Basics) {
  // Set Bluetooth discovery simulation delay to 0 so the test doesn't have to
  // wait or use timers.
  FakeBluetoothAdapterClient* adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          BluezDBusManager::Get()->GetBluetoothAdapterClient());
  adapter_client->SetSimulationIntervalMs(0);

  FakeBluetoothDeviceClient* device_client =
      static_cast<FakeBluetoothDeviceClient*>(
          BluezDBusManager::Get()->GetBluetoothDeviceClient());
  // A classic bluetooth keyboard device shouldn't be filtered out.
  device_client->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kDisplayPinCodePath));
  // A low energy bluetooth heart rate monitor should be filtered out.
  device_client->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));

  TrayBluetoothHelperLegacy helper;
  helper.Initialize();
  RunAllPendingInMessageLoop();
  EXPECT_EQ(device::mojom::BluetoothSystem::State::kPoweredOff,
            helper.GetBluetoothState());
  EXPECT_FALSE(helper.HasBluetoothDiscoverySession());

  BluetoothDeviceList devices = helper.GetAvailableBluetoothDevices();
  // The devices are fake in tests, so don't assume any particular number.
  EXPECT_FALSE(devices.empty());
  EXPECT_TRUE(ExistInFilteredDevices(
      FakeBluetoothDeviceClient::kDisplayPinCodeAddress, devices));
  EXPECT_FALSE(ExistInFilteredDevices(
      FakeBluetoothDeviceClient::kLowEnergyAddress, devices));

  helper.StartBluetoothDiscovering();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(helper.HasBluetoothDiscoverySession());

  helper.StopBluetoothDiscovering();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(helper.HasBluetoothDiscoverySession());
}

// Tests GetBluetoothState() returns the right value based on the adapter state.
TEST_F(TrayBluetoothHelperLegacyTest, GetBluetoothState) {
  TrayBluetoothHelperLegacy helper;
  // Purposely don't call TrayBluetoothHelperLegacy::Initialize() to simulate
  // that the BluetoothAdapter object hasn't been retrieved yet.
  EXPECT_EQ(device::mojom::BluetoothSystem::State::kUnavailable,
            helper.GetBluetoothState());

  FakeBluetoothAdapterClient* adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          BluezDBusManager::Get()->GetBluetoothAdapterClient());

  // Mark all adapters as not-visible to simulate no adapters.
  adapter_client->SetVisible(false);
  adapter_client->SetSecondVisible(false);
  helper.Initialize();
  RunAllPendingInMessageLoop();

  EXPECT_EQ(device::mojom::BluetoothSystem::State::kUnavailable,
            helper.GetBluetoothState());

  // Make adapter visible but turn it off.
  adapter_client->SetVisible(true);
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.Set(false, base::DoNothing());

  EXPECT_EQ(device::mojom::BluetoothSystem::State::kPoweredOff,
            helper.GetBluetoothState());

  // Turn adapter on.
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.Set(true, base::DoNothing());

  EXPECT_EQ(device::mojom::BluetoothSystem::State::kPoweredOn,
            helper.GetBluetoothState());
}

// Tests the Bluetooth device list when UnfilteredBluetoothDevices feature is
// enabled.
TEST_F(TrayBluetoothHelperLegacyTest, UnfilteredBluetoothDevices) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine(device::kUnfilteredBluetoothDevices.name,
                                   "");

  // Set Bluetooth discovery simulation delay to 0 so the test doesn't have to
  // wait or use timers.
  FakeBluetoothAdapterClient* adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          BluezDBusManager::Get()->GetBluetoothAdapterClient());
  adapter_client->SetSimulationIntervalMs(0);

  FakeBluetoothDeviceClient* device_client =
      static_cast<FakeBluetoothDeviceClient*>(
          BluezDBusManager::Get()->GetBluetoothDeviceClient());
  // All devices should be shown (unfiltered).
  device_client->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kDisplayPinCodePath));
  device_client->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));

  TrayBluetoothHelperLegacy helper;
  helper.Initialize();
  base::RunLoop().RunUntilIdle();

  BluetoothDeviceList devices = helper.GetAvailableBluetoothDevices();
  // The devices are fake in tests, so don't assume any particular number.
  EXPECT_TRUE(ExistInFilteredDevices(
      FakeBluetoothDeviceClient::kDisplayPinCodeAddress, devices));
  EXPECT_TRUE(ExistInFilteredDevices(
      FakeBluetoothDeviceClient::kLowEnergyAddress, devices));
}

}  // namespace
}  // namespace ash
