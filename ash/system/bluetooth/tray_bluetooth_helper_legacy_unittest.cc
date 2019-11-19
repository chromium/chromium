// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth_helper_legacy.h"

#include <string>
#include <vector>

#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_switches.h"
#include "dbus/object_path.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

using bluez::BluezDBusManager;
using bluez::FakeBluetoothAdapterClient;
using bluez::FakeBluetoothDeviceClient;
using device::mojom::BluetoothSystem;

namespace ash {
namespace {

// FakeBluetoothDeviceClient::kDisplayPinCodeAddress but in a BluetoothAddress.
constexpr BluetoothAddress kDisplayPinCodeAddress = {0x28, 0x37, 0x37,
                                                     0x00, 0x00, 0x00};
// FakeBluetoothDeviceClient::kLowEnergyAddress but in a BluetoothAddress.
constexpr BluetoothAddress kLowEnergyAddress = {0x00, 0x1A, 0x11,
                                                0x00, 0x15, 0x30};

// Returns true if device with |address| exists in the filtered device list.
// Returns false otherwise.
bool ExistInFilteredDevices(const BluetoothAddress& address,
                            const BluetoothDeviceList& filtered_devices) {
  for (const auto& device : filtered_devices) {
    if (device->address == address)
      return true;
  }
  return false;
}

// Test observer that counts the number of times methods are called and what the
// state was then the OnBluetoothSystemChanged method is called.
class TestTrayBluetoothHelperObserver : public TrayBluetoothHelper::Observer {
 public:
  TestTrayBluetoothHelperObserver(TrayBluetoothHelper* helper)
      : helper_(helper) {}
  ~TestTrayBluetoothHelperObserver() override = default;

  void Reset() {
    system_state_changed_count_ = 0;
    system_states_.clear();
    scan_state_changed_count_ = 0;
    device_list_changed_count_ = 0;
  }

  void OnBluetoothSystemStateChanged() override {
    ++system_state_changed_count_;
    system_states_.push_back(helper_->GetBluetoothState());
  }

  void OnBluetoothScanStateChanged() override { ++scan_state_changed_count_; }

  void OnBluetoothDeviceListChanged() override { ++device_list_changed_count_; }

  TrayBluetoothHelper* helper_;

  size_t system_state_changed_count_ = 0;

  std::vector<BluetoothSystem::State> system_states_;

  size_t scan_state_changed_count_ = 0;
  size_t device_list_changed_count_ = 0;
};

using TrayBluetoothHelperLegacyTest = AshTestBase;

// Tests basic functionality.
TEST_F(TrayBluetoothHelperLegacyTest, Basics) {
  // Set Bluetooth discovery simulation delay to 0 so the test doesn't have to
  // wait or use timers.
  FakeBluetoothAdapterClient* adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          BluezDBusManager::Get()->GetBluetoothAdapterClient());
  adapter_client->SetSimulationIntervalMs(0);
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.ReplaceValue(true);

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
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(device::mojom::BluetoothSystem::State::kPoweredOn,
            helper.GetBluetoothState());
  EXPECT_FALSE(helper.HasBluetoothDiscoverySession());

  const BluetoothDeviceList& devices = helper.GetAvailableBluetoothDevices();
  // The devices are fake in tests, so don't assume any particular number.
  EXPECT_FALSE(devices.empty());
  EXPECT_TRUE(ExistInFilteredDevices(kDisplayPinCodeAddress, devices));
  EXPECT_FALSE(ExistInFilteredDevices(kLowEnergyAddress, devices));

  helper.StartBluetoothDiscovering();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper.HasBluetoothDiscoverySession());

  helper.StopBluetoothDiscovering();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper.HasBluetoothDiscoverySession());
}

// Tests GetBluetoothState() returns the right value based on the adapter state.
TEST_F(TrayBluetoothHelperLegacyTest, GetBluetoothState) {
  TrayBluetoothHelperLegacy helper;
  // Purposely don't call TrayBluetoothHelperLegacy::Initialize() to simulate
  // that the BluetoothAdapter object hasn't been retrieved yet.
  EXPECT_EQ(BluetoothSystem::State::kUnavailable, helper.GetBluetoothState());

  FakeBluetoothAdapterClient* adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          BluezDBusManager::Get()->GetBluetoothAdapterClient());

  // Mark all adapters as not-visible to simulate no adapters.
  adapter_client->SetVisible(false);
  adapter_client->SetSecondVisible(false);
  helper.Initialize();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BluetoothSystem::State::kUnavailable, helper.GetBluetoothState());

  // Make adapter visible but turn it off.
  adapter_client->SetVisible(true);
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.Set(false, base::DoNothing());

  EXPECT_EQ(BluetoothSystem::State::kPoweredOff, helper.GetBluetoothState());

  // Turn adapter on.
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.Set(true, base::DoNothing());

  EXPECT_EQ(BluetoothSystem::State::kPoweredOn, helper.GetBluetoothState());
}

// Tests OnBluetoothSystemStateChanged() gets called whenever the state changes.
TEST_F(TrayBluetoothHelperLegacyTest, OnBluetoothSystemStateChanged) {
  TrayBluetoothHelperLegacy helper;
  TestTrayBluetoothHelperObserver observer(&helper);
  helper.AddObserver(&observer);

  // Purposely don't call TrayBluetoothHelperLegacy::Initialize() to simulate
  // that the BluetoothAdapter object hasn't been retrieved yet.
  EXPECT_EQ(BluetoothSystem::State::kUnavailable, helper.GetBluetoothState());
  EXPECT_EQ(0u, observer.system_state_changed_count_);

  FakeBluetoothAdapterClient* adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          BluezDBusManager::Get()->GetBluetoothAdapterClient());

  // Mark all adapters as not-visible to simulate no adapters.
  adapter_client->SetVisible(false);
  adapter_client->SetSecondVisible(false);
  helper.Initialize();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BluetoothSystem::State::kUnavailable, helper.GetBluetoothState());
  EXPECT_EQ(0u, observer.system_state_changed_count_);

  // Turn off the adapter and make it visible to simulate a powered off adapter
  // being added.
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.ReplaceValue(false);
  adapter_client->SetVisible(true);

  EXPECT_EQ(BluetoothSystem::State::kPoweredOff, helper.GetBluetoothState());
  EXPECT_EQ(1u, observer.system_state_changed_count_);
  EXPECT_EQ(std::vector<BluetoothSystem::State>(
                {BluetoothSystem::State::kPoweredOff}),
            observer.system_states_);
  observer.Reset();

  // Turn adapter on.
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.ReplaceValue(true);

  EXPECT_EQ(BluetoothSystem::State::kPoweredOn, helper.GetBluetoothState());
  EXPECT_EQ(1u, observer.system_state_changed_count_);
  EXPECT_EQ(
      std::vector<BluetoothSystem::State>({BluetoothSystem::State::kPoweredOn}),
      observer.system_states_);
  observer.Reset();

  // Remove the adapter.
  adapter_client->SetVisible(false);
  EXPECT_EQ(BluetoothSystem::State::kUnavailable, helper.GetBluetoothState());
  EXPECT_EQ(1u, observer.system_state_changed_count_);
  EXPECT_EQ(std::vector<BluetoothSystem::State>(
                {BluetoothSystem::State::kUnavailable}),
            observer.system_states_);
  observer.Reset();

  // Turn on the adapter and make it visible to simulate a powered on adapter
  // being added.
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.ReplaceValue(true);
  adapter_client->SetVisible(true);

  EXPECT_EQ(BluetoothSystem::State::kPoweredOn, helper.GetBluetoothState());
  EXPECT_EQ(1u, observer.system_state_changed_count_);
  EXPECT_EQ(
      std::vector<BluetoothSystem::State>({BluetoothSystem::State::kPoweredOn}),
      observer.system_states_);
  observer.Reset();

  // Turn off the adapter.
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.ReplaceValue(false);

  EXPECT_EQ(BluetoothSystem::State::kPoweredOff, helper.GetBluetoothState());
  EXPECT_EQ(1u, observer.system_state_changed_count_);
  EXPECT_EQ(std::vector<BluetoothSystem::State>(
                {BluetoothSystem::State::kPoweredOff}),
            observer.system_states_);
  observer.Reset();
}

// Tests the Bluetooth device list when UnfilteredBluetoothDevices feature is
// enabled.
TEST_F(TrayBluetoothHelperLegacyTest, UnfilteredBluetoothDevices) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch(chromeos::switches::kUnfilteredBluetoothDevices);

  // Set Bluetooth discovery simulation delay to 0 so the test doesn't have to
  // wait or use timers.
  FakeBluetoothAdapterClient* adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          BluezDBusManager::Get()->GetBluetoothAdapterClient());
  adapter_client->SetSimulationIntervalMs(0);
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.ReplaceValue(true);

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

  const BluetoothDeviceList& devices = helper.GetAvailableBluetoothDevices();
  // The devices are fake in tests, so don't assume any particular number.
  EXPECT_TRUE(ExistInFilteredDevices(kDisplayPinCodeAddress, devices));
  EXPECT_TRUE(ExistInFilteredDevices(kLowEnergyAddress, devices));
}

TEST_F(TrayBluetoothHelperLegacyTest, BluetoothAddress) {
  // Set Bluetooth discovery simulation delay to 0 so the test doesn't have to
  // wait or use timers.
  FakeBluetoothAdapterClient* adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          BluezDBusManager::Get()->GetBluetoothAdapterClient());
  adapter_client->SetSimulationIntervalMs(0);
  adapter_client
      ->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath))
      ->powered.ReplaceValue(true);

  FakeBluetoothDeviceClient* device_client =
      static_cast<FakeBluetoothDeviceClient*>(
          BluezDBusManager::Get()->GetBluetoothDeviceClient());
  device_client->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kDisplayPinCodePath));

  TrayBluetoothHelperLegacy helper;
  helper.Initialize();
  base::RunLoop().RunUntilIdle();

  const BluetoothDeviceList& devices = helper.GetAvailableBluetoothDevices();
  ASSERT_EQ(3u, devices.size());
  EXPECT_EQ(base::UTF8ToUTF16(
                FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress),
            device::GetBluetoothAddressForDisplay(devices[0]->address));
  EXPECT_EQ(base::UTF8ToUTF16(FakeBluetoothDeviceClient::kPairedDeviceAddress),
            device::GetBluetoothAddressForDisplay(devices[1]->address));
  EXPECT_EQ(
      base::UTF8ToUTF16(FakeBluetoothDeviceClient::kDisplayPinCodeAddress),
      device::GetBluetoothAddressForDisplay(devices[2]->address));
}

}  // namespace
}  // namespace ash
