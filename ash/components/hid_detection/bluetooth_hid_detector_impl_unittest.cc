// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/bluetooth_hid_detector_impl.h"

#include "base/test/task_environment.h"
#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/fake_bluetooth_power_controller.h"
#include "chromeos/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/services/bluetooth_config/fake_device_pairing_handler.h"
#include "chromeos/services/bluetooth_config/fake_discovered_devices_provider.h"
#include "chromeos/services/bluetooth_config/fake_discovery_session_manager.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::bluetooth_config::FakeDevicePairingHandler;
using chromeos::bluetooth_config::mojom::BluetoothDeviceProperties;
using chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr;
using chromeos::bluetooth_config::mojom::BluetoothSystemState;
using chromeos::bluetooth_config::mojom::DeviceType;
using chromeos::bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

namespace ash {
namespace hid_detection {

class BluetoothHidDetectorImplTest : public testing::Test {
 protected:
  BluetoothHidDetectorImplTest() = default;
  BluetoothHidDetectorImplTest(const BluetoothHidDetectorImplTest&) = delete;
  BluetoothHidDetectorImplTest& operator=(const BluetoothHidDetectorImplTest&) =
      delete;
  ~BluetoothHidDetectorImplTest() override = default;

  // testing::Test:
  void SetUp() override {}

  void TearDown() override {
    // HID detection must be stopped before BluetoothHidDetectorImpl is
    // destroyed.
    if (IsDiscoverySessionActive())
      StopBluetoothHidDetection();
  }

  void StartBluetoothHidDetection(bool pointer_is_missing = true,
                                  bool keyboard_is_missing = true) {
    bluetooth_hid_detector_.StartBluetoothHidDetection(
        {.pointer_is_missing = pointer_is_missing,
         .keyboard_is_missing = keyboard_is_missing});
    base::RunLoop().RunUntilIdle();
  }

  void StopBluetoothHidDetection() {
    bluetooth_hid_detector_.StopBluetoothHidDetection();
    base::RunLoop().RunUntilIdle();
  }

  // Simulates Bluetooth being toggled by a UI surface. This sets the state of
  // the Bluetooth adapter and persists that state which is restored when HID
  // detection finishes.
  void SimulateBluetoothToggledByUi(bool enabled) {
    scoped_bluetooth_config_test_helper_.fake_bluetooth_power_controller()
        ->SetBluetoothEnabledState(enabled);
  }

  // Sets the state of the Bluetooth adapter without any persistence. This
  // simulates the adapter changing without any user interaction.
  void SetAdapterState(BluetoothSystemState system_state) {
    scoped_bluetooth_config_test_helper_.fake_adapter_state_controller()
        ->SetSystemState(system_state);
  }

  BluetoothSystemState GetAdapterState() {
    return scoped_bluetooth_config_test_helper_.fake_adapter_state_controller()
        ->GetAdapterState();
  }

  bool IsDiscoverySessionActive() {
    return scoped_bluetooth_config_test_helper_
        .fake_discovery_session_manager()
        ->IsDiscoverySessionActive();
  }

  // Updates a Bluetooth system property in order to trigger the
  // OnPropertiesUpdated() observer method in BluetoothHidDetectorImpl.
  void TriggerOnPropertiesUpdatedCall() {
    auto paired_device = PairedBluetoothDeviceProperties::New();
    paired_device->device_properties = BluetoothDeviceProperties::New();
    std::vector<PairedBluetoothDevicePropertiesPtr> paired_devices;
    paired_devices.push_back(mojo::Clone(paired_device));
    scoped_bluetooth_config_test_helper_.fake_device_cache()->SetPairedDevices(
        std::move(paired_devices));
    base::RunLoop().RunUntilIdle();
  }

  void AddUnpairedDevice(std::string* id_out, DeviceType device_type) {
    // We use the number of devices created in this test as the id.
    *id_out = base::NumberToString(num_devices_created_);
    ++num_devices_created_;

    auto device = BluetoothDeviceProperties::New();
    device->id = *id_out;
    device->device_type = device_type;
    unpaired_devices_.push_back(device.Clone());

    UpdateDiscoveredDevicesProviderDevices();
    base::RunLoop().RunUntilIdle();
  }

  void RemoveUnpairedDevice(const std::string& device_id) {
    unpaired_devices_.erase(
        std::remove_if(unpaired_devices_.begin(), unpaired_devices_.end(),
                       [device_id](BluetoothDevicePropertiesPtr const& device) {
                         return device->id == device_id;
                       }),
        unpaired_devices_.end());

    UpdateDiscoveredDevicesProviderDevices();
    base::RunLoop().RunUntilIdle();
  }

  std::vector<FakeDevicePairingHandler*> GetDevicePairingHandlers() {
    return scoped_bluetooth_config_test_helper_
        .fake_discovery_session_manager()
        ->device_pairing_handlers();
  }

 private:
  void UpdateDiscoveredDevicesProviderDevices() {
    std::vector<BluetoothDevicePropertiesPtr> unpaired_devices;
    for (auto& device : unpaired_devices_) {
      unpaired_devices.push_back(device.Clone());
    }
    scoped_bluetooth_config_test_helper_.fake_discovered_devices_provider()
        ->SetDiscoveredDevices(std::move(unpaired_devices));
  }

  base::test::TaskEnvironment task_environment_;

  std::vector<BluetoothDevicePropertiesPtr> unpaired_devices_;
  size_t num_devices_created_ = 0u;

  chromeos::bluetooth_config::ScopedBluetoothConfigTestHelper
      scoped_bluetooth_config_test_helper_;

  BluetoothHidDetectorImpl bluetooth_hid_detector_;
};

TEST_F(BluetoothHidDetectorImplTest, StartStopStartDetection_BluetoothEnabled) {
  // Start with Bluetooth enabled.
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Trigger an OnPropertiesUpdated() call. Nothing should happen.
  TriggerOnPropertiesUpdatedCall();
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Begin HID detection. Discovery should have started and Bluetooth still
  // enabled.
  StartBluetoothHidDetection();
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetAdapterState());

  // Trigger an OnPropertiesUpdated() call. Nothing should happen.
  TriggerOnPropertiesUpdatedCall();
  EXPECT_TRUE(IsDiscoverySessionActive());

  // Stop HID detection. Discovery should have stopped but Bluetooth still
  // enabled.
  StopBluetoothHidDetection();
  EXPECT_FALSE(IsDiscoverySessionActive());
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetAdapterState());

  // Trigger an OnPropertiesUpdated() call. Nothing should happen.
  TriggerOnPropertiesUpdatedCall();
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Begin HID detection again. Discovery should have started and Bluetooth
  // still enabled.
  StartBluetoothHidDetection();
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetAdapterState());
}

TEST_F(BluetoothHidDetectorImplTest, StartStopDetection_BluetoothDisabled) {
  // Initiate disabling Bluetooth.
  SimulateBluetoothToggledByUi(/*enabled=*/false);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetAdapterState());

  // Complete adapter disabling.
  SetAdapterState(BluetoothSystemState::kDisabled);
  EXPECT_EQ(BluetoothSystemState::kDisabled, GetAdapterState());

  // Begin HID detection. The adapter state should switch to enabling.
  StartBluetoothHidDetection();
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetAdapterState());
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Mock the adapter becoming unavailable.
  SetAdapterState(BluetoothSystemState::kUnavailable);
  EXPECT_EQ(BluetoothSystemState::kUnavailable, GetAdapterState());
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Mock the adapter becoming available again.
  SetAdapterState(BluetoothSystemState::kEnabling);
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetAdapterState());
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Mock the adapter enabling. Discovery should have started.
  SetAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_TRUE(IsDiscoverySessionActive());

  // Trigger an OnPropertiesUpdated() call. Nothing should happen.
  TriggerOnPropertiesUpdatedCall();

  // Stop HID detection. Discovery should have stopped and Bluetooth disabled.
  StopBluetoothHidDetection();
  EXPECT_FALSE(IsDiscoverySessionActive());
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetAdapterState());
}

TEST_F(BluetoothHidDetectorImplTest, StartDetection_BluetoothUnavailable) {
  // Set Bluetooth to unavailable.
  SetAdapterState(BluetoothSystemState::kUnavailable);
  EXPECT_EQ(BluetoothSystemState::kUnavailable, GetAdapterState());

  // Begin HID detection.
  StartBluetoothHidDetection();
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Set Bluetooth to enabling.
  SetAdapterState(BluetoothSystemState::kEnabling);
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetAdapterState());

  // Complete adapter enabling. Discovery should have started.
  SetAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_TRUE(IsDiscoverySessionActive());
}

TEST_F(BluetoothHidDetectorImplTest,
       StartDetection_BluetoothDisabledEnabledExternally) {
  // Start with Bluetooth enabled.
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetAdapterState());

  // Begin HID detection. Discovery should have started and Bluetooth still
  // enabled.
  StartBluetoothHidDetection();
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetAdapterState());

  // Mock another client disabling Bluetooth.
  SimulateBluetoothToggledByUi(/*enabled=*/false);
  EXPECT_EQ(BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Finish the adapter disabling.
  SetAdapterState(BluetoothSystemState::kDisabled);
  EXPECT_EQ(BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Mock another client re-enabling Bluetooth. This should cause
  // BluetoothHidDetector to start discovery again.
  SimulateBluetoothToggledByUi(/*enabled=*/true);
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetAdapterState());
  EXPECT_FALSE(IsDiscoverySessionActive());
  SetAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_TRUE(IsDiscoverySessionActive());
}

TEST_F(BluetoothHidDetectorImplTest, AddDevices_TypeNotHid) {
  std::string device_id1;
  AddUnpairedDevice(&device_id1, DeviceType::kHeadset);

  std::string device_id2;
  AddUnpairedDevice(&device_id2, DeviceType::kKeyboard);

  // Begin HID detection. |device_id1| should not be attempted to be paired
  // with.
  StartBluetoothHidDetection();
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(1u, GetDevicePairingHandlers().size());
  EXPECT_EQ(device_id2,
            GetDevicePairingHandlers()[0]->current_pairing_device_id());
}

TEST_F(BluetoothHidDetectorImplTest, AddDevices_TypeNotMissing) {
  std::string device_id1;
  AddUnpairedDevice(&device_id1, DeviceType::kMouse);

  std::string device_id2;
  AddUnpairedDevice(&device_id2, DeviceType::kKeyboard);

  // Begin HID detection. |device_id1| should not be attempted to be paired
  // with.
  StartBluetoothHidDetection(/*is_pointer_missing=*/false);
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(1u, GetDevicePairingHandlers().size());
  EXPECT_EQ(device_id2,
            GetDevicePairingHandlers()[0]->current_pairing_device_id());
}

TEST_F(BluetoothHidDetectorImplTest, AddDevices_BeforeStartingDetection) {
  std::string device_id1;
  AddUnpairedDevice(&device_id1, DeviceType::kMouse);

  std::string device_id2;
  AddUnpairedDevice(&device_id2, DeviceType::kKeyboardMouseCombo);

  // Begin HID detection. |device_id1| should be attempted to be paired with.
  StartBluetoothHidDetection();
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(1u, GetDevicePairingHandlers().size());
  EXPECT_EQ(device_id1,
            GetDevicePairingHandlers()[0]->current_pairing_device_id());

  // Mock |device_id1| being paired. |device_id2| should be attempted to be
  // paired with.
  RemoveUnpairedDevice(device_id1);
  GetDevicePairingHandlers()[0]->SimulatePairDeviceFinished(
      /*failure_reason=*/absl::nullopt);
  EXPECT_EQ(device_id2,
            GetDevicePairingHandlers()[0]->current_pairing_device_id());

  // Mock |device_id2| being paired.
  RemoveUnpairedDevice(device_id2);
  GetDevicePairingHandlers()[0]->SimulatePairDeviceFinished(
      /*failure_reason=*/absl::nullopt);
  EXPECT_TRUE(
      GetDevicePairingHandlers()[0]->current_pairing_device_id().empty());
}

TEST_F(BluetoothHidDetectorImplTest,
       AddDevices_SeriallyAfterStartingDetection) {
  StartBluetoothHidDetection();
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(1u, GetDevicePairingHandlers().size());
  EXPECT_TRUE(
      GetDevicePairingHandlers()[0]->current_pairing_device_id().empty());

  std::string device_id1;
  AddUnpairedDevice(&device_id1, DeviceType::kTablet);
  EXPECT_EQ(device_id1,
            GetDevicePairingHandlers()[0]->current_pairing_device_id());

  // Mock |device_id1| being paired.
  RemoveUnpairedDevice(device_id1);
  GetDevicePairingHandlers()[0]->SimulatePairDeviceFinished(
      /*failure_reason=*/absl::nullopt);
  EXPECT_TRUE(
      GetDevicePairingHandlers()[0]->current_pairing_device_id().empty());

  std::string device_id2;
  AddUnpairedDevice(&device_id2, DeviceType::kKeyboard);
  EXPECT_EQ(device_id2,
            GetDevicePairingHandlers()[0]->current_pairing_device_id());

  // Mock |device_id2| being paired.
  RemoveUnpairedDevice(device_id2);
  GetDevicePairingHandlers()[0]->SimulatePairDeviceFinished(
      /*failure_reason=*/absl::nullopt);
  EXPECT_TRUE(
      GetDevicePairingHandlers()[0]->current_pairing_device_id().empty());
}

TEST_F(BluetoothHidDetectorImplTest, AddDevices_BatchAfterStartingDetection) {
  StartBluetoothHidDetection();
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(1u, GetDevicePairingHandlers().size());
  EXPECT_TRUE(
      GetDevicePairingHandlers()[0]->current_pairing_device_id().empty());

  std::string device_id1;
  AddUnpairedDevice(&device_id1, DeviceType::kMouse);
  EXPECT_EQ(device_id1,
            GetDevicePairingHandlers()[0]->current_pairing_device_id());

  std::string device_id2;
  AddUnpairedDevice(&device_id2, DeviceType::kKeyboard);

  // Mock |device_id1| being paired.
  RemoveUnpairedDevice(device_id1);
  GetDevicePairingHandlers()[0]->SimulatePairDeviceFinished(
      /*failure_reason=*/absl::nullopt);
  EXPECT_EQ(device_id2,
            GetDevicePairingHandlers()[0]->current_pairing_device_id());

  // Mock |device_id2| being paired.
  RemoveUnpairedDevice(device_id2);
  GetDevicePairingHandlers()[0]->SimulatePairDeviceFinished(
      /*failure_reason=*/absl::nullopt);
  EXPECT_TRUE(
      GetDevicePairingHandlers()[0]->current_pairing_device_id().empty());
}

TEST_F(BluetoothHidDetectorImplTest, AdapterDisablesDuringPairing) {
  std::string device_id1;
  AddUnpairedDevice(&device_id1, DeviceType::kMouse);

  std::string device_id2;
  AddUnpairedDevice(&device_id2, DeviceType::kKeyboard);

  // Begin HID detection. |device_id1| should be attempted to be paired with.
  StartBluetoothHidDetection();
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(1u, GetDevicePairingHandlers().size());
  EXPECT_EQ(device_id1,
            GetDevicePairingHandlers()[0]->current_pairing_device_id());

  // Mock the adapter disabling.
  SetAdapterState(BluetoothSystemState::kDisabled);
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Mock the adapter re-enabling Bluetooth. This should cause
  // BluetoothHidDetector to start discovery again. The first device should be
  // attempted to be paired with again.
  SetAdapterState(BluetoothSystemState::kEnabled);
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(2u, GetDevicePairingHandlers().size());
  EXPECT_EQ(device_id1,
            GetDevicePairingHandlers()[1]->current_pairing_device_id());
}

TEST_F(BluetoothHidDetectorImplTest, DetectionStopsStartsDuringPairing) {
  std::string device_id1;
  AddUnpairedDevice(&device_id1, DeviceType::kMouse);

  std::string device_id2;
  AddUnpairedDevice(&device_id2, DeviceType::kKeyboard);

  // Begin HID detection. |device_id1| should be attempted to be paired with.
  StartBluetoothHidDetection();
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(1u, GetDevicePairingHandlers().size());
  EXPECT_EQ(device_id1,
            GetDevicePairingHandlers()[0]->current_pairing_device_id());

  // Stop detection.
  StopBluetoothHidDetection();
  EXPECT_FALSE(IsDiscoverySessionActive());

  // Start detection again. The first device should be attempted to be paired
  // with again.
  StartBluetoothHidDetection();
  EXPECT_TRUE(IsDiscoverySessionActive());
  EXPECT_EQ(2u, GetDevicePairingHandlers().size());
  EXPECT_EQ(device_id1,
            GetDevicePairingHandlers()[1]->current_pairing_device_id());
}

}  // namespace hid_detection
}  // namespace ash
