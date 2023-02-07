// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_address_map.h"

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kTestModelId[] = "test_model_id";
constexpr char kTestBLEAddress[] = "test_ble_address";
constexpr char kTestClassicAddress[] = "test_classic_address";

}  // namespace

namespace ash::quick_pair {

// For convenience.
using ::testing::Return;

class DeviceAddressMapTest : public AshTestBase {
 public:
  DeviceAddressMapTest() {}

  void SetUp() override {
    AshTestBase::SetUp();

    device_address_map_ = std::make_unique<DeviceAddressMap>();
  }

  void SetupDevice(const std::string& ble_address,
                   const std::string& classic_address) {
    device_ = base::MakeRefCounted<Device>(kTestModelId, ble_address,
                                           Protocol::kFastPairInitial);

    if (!classic_address.empty()) {
      device_->set_classic_address(classic_address);
    }
  }

 protected:
  scoped_refptr<Device> device_;
  std::unique_ptr<DeviceAddressMap> device_address_map_;
};

TEST_F(DeviceAddressMapTest, SaveModelIdForDeviceValid) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  absl::optional<const std::string> ble_model_id =
      device_address_map_->GetModelIdForMacAddress(kTestBLEAddress);
  EXPECT_TRUE(ble_model_id);
  EXPECT_EQ(ble_model_id.value(), kTestModelId);
  absl::optional<const std::string> classic_model_id =
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress);
  EXPECT_TRUE(classic_model_id);
  EXPECT_EQ(classic_model_id.value(), kTestModelId);
}

TEST_F(DeviceAddressMapTest, SaveModelIdForDeviceValidOnlyClassicAddress) {
  // Set the BLE address to empty.
  SetupDevice("", kTestClassicAddress);

  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  absl::optional<const std::string> ble_model_id =
      device_address_map_->GetModelIdForMacAddress(kTestBLEAddress);
  EXPECT_FALSE(ble_model_id);
  absl::optional<const std::string> classic_model_id =
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress);
  EXPECT_TRUE(classic_model_id);
  EXPECT_EQ(classic_model_id.value(), kTestModelId);
}

TEST_F(DeviceAddressMapTest, SaveModelIdForDeviceValidOnlyBLEAddress) {
  // Set the Classic address to empty.
  SetupDevice(kTestBLEAddress, "");

  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  absl::optional<const std::string> ble_model_id =
      device_address_map_->GetModelIdForMacAddress(kTestBLEAddress);
  EXPECT_TRUE(ble_model_id);
  EXPECT_EQ(ble_model_id.value(), kTestModelId);
  absl::optional<const std::string> classic_model_id =
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress);
  EXPECT_FALSE(classic_model_id);
}

TEST_F(DeviceAddressMapTest, SaveModelIdForDeviceInvalidDeviceNotFound) {
  // Set both BLE and Classic address to empty.
  SetupDevice("", "");

  EXPECT_FALSE(device_address_map_->SaveModelIdForDevice(device_));
  absl::optional<const std::string> ble_model_id =
      device_address_map_->GetModelIdForMacAddress(kTestBLEAddress);
  EXPECT_FALSE(ble_model_id);
  absl::optional<const std::string> classic_model_id =
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress);
  EXPECT_FALSE(classic_model_id);
}

TEST_F(DeviceAddressMapTest, PersistRecordsForDeviceValid) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  // First, save the mac address to model ID records to memory.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));

  // Validate that the ID records are persisted to prefs.
  PrefService* local_state = Shell::Get()->local_state();
  const base::Value::Dict& device_address_map_dict =
      local_state->GetDict(DeviceAddressMap::kDeviceAddressMapPref);
  const std::string* ble_model_id =
      device_address_map_dict.FindString(kTestBLEAddress);
  EXPECT_TRUE(ble_model_id);
  EXPECT_EQ(*ble_model_id, kTestModelId);
  const std::string* classic_model_id =
      device_address_map_dict.FindString(kTestClassicAddress);
  EXPECT_TRUE(classic_model_id);
  EXPECT_EQ(*classic_model_id, kTestModelId);
}

TEST_F(DeviceAddressMapTest, PersistRecordsForDeviceValidOnlyClassicAddress) {
  // Set the BLE address to empty. A record should still be saved for the valid
  // address.
  SetupDevice("", kTestClassicAddress);

  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
}

TEST_F(DeviceAddressMapTest, PersistRecordsForDeviceValidOnlyBLEAddress) {
  // Set the Classic address to empty. A record should still be saved for the
  // valid address.
  SetupDevice(kTestBLEAddress, "");

  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
}

TEST_F(DeviceAddressMapTest, PersistRecordsForDeviceValidDoublePersist) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  // First, save the mac address records to memory.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));

  // When persisting a second time, should overwrite record and
  // still return true.
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
}

TEST_F(DeviceAddressMapTest, PersistRecordsForDeviceInvalidNotSaved) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  // Don't save the mac address record to memory.
  EXPECT_FALSE(device_address_map_->PersistRecordsForDevice(device_));
}

TEST_F(DeviceAddressMapTest, EvictMacAddressRecordValid) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  // First, persist the mac address record to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
  EXPECT_TRUE(device_address_map_->EvictMacAddressRecord(kTestBLEAddress));

  // Validate that the ID records are evicted from prefs.
  PrefService* local_state = Shell::Get()->local_state();
  const base::Value::Dict& device_address_map_dict =
      local_state->GetDict(DeviceAddressMap::kDeviceAddressMapPref);
  const std::string* model_id =
      device_address_map_dict.FindString(kTestBLEAddress);
  EXPECT_FALSE(model_id);
}

TEST_F(DeviceAddressMapTest, EvictMacAddressRecordInvalidDeviceAddress) {
  // Don't save the mac address records to disk.
  EXPECT_FALSE(device_address_map_->EvictMacAddressRecord(kTestBLEAddress));
}

TEST_F(DeviceAddressMapTest, EvictMacAddressRecordInvalidDoubleFree) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  // First, persist the mac address records to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
  EXPECT_TRUE(device_address_map_->EvictMacAddressRecord(kTestBLEAddress));

  // The second evict should fail.
  EXPECT_FALSE(device_address_map_->EvictMacAddressRecord(kTestBLEAddress));
}

TEST_F(DeviceAddressMapTest, GetModelIdForMacAddressValid) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));

  absl::optional<const std::string> model_id =
      device_address_map_->GetModelIdForMacAddress(kTestBLEAddress);
  EXPECT_TRUE(model_id);
  EXPECT_EQ(model_id.value(), kTestModelId);
}

TEST_F(DeviceAddressMapTest, GetModelIdForMacAddressInvalidUninitialized) {
  // Don't initialize the dictionary with any results.
  absl::optional<const std::string> model_id =
      device_address_map_->GetModelIdForMacAddress(kTestBLEAddress);
  EXPECT_FALSE(model_id);
}

TEST_F(DeviceAddressMapTest, GetModelIdForMacAddressInvalidNotAdded) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));

  absl::optional<const std::string> model_id =
      device_address_map_->GetModelIdForMacAddress("not found id");
  EXPECT_FALSE(model_id);
}

TEST_F(DeviceAddressMapTest, HasPersistedRecordsForModelIdTrueAfterPersist) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  // First, persist the mac address records to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
  EXPECT_TRUE(device_address_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceAddressMapTest,
       HasPersistedRecordsForModelIdTrueAfterOneEviction) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  // First, persist the mac address records to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
  // Evict one of the records that points to this model ID.
  EXPECT_TRUE(device_address_map_->EvictMacAddressRecord(kTestClassicAddress));
  EXPECT_TRUE(device_address_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceAddressMapTest,
       HasPersistedRecordsForModelIdFalseAfterAllEvictions) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  // First, persist the mac address records to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
  // Evict all of the records that points to this model ID.
  EXPECT_TRUE(device_address_map_->EvictMacAddressRecord(kTestClassicAddress));
  EXPECT_TRUE(device_address_map_->EvictMacAddressRecord(kTestBLEAddress));
  EXPECT_FALSE(
      device_address_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceAddressMapTest, HasPersistedRecordsForModelIdFalseNoPersist) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  // Don't persist the mac address records to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_FALSE(
      device_address_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceAddressMapTest, LoadPersistedIdRecordFromPrefs) {
  // Populate both BLE address and Classic address.
  SetupDevice(kTestBLEAddress, kTestClassicAddress);

  // First, persist the mac address records to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));

  // A new/restarted DeviceAddressMap instance should load persisted ID records
  // from prefs.
  DeviceAddressMap new_device_address_map = DeviceAddressMap();
  absl::optional<const std::string> model_id =
      new_device_address_map.GetModelIdForMacAddress(kTestBLEAddress);
  EXPECT_TRUE(model_id);
  EXPECT_EQ(model_id.value(), kTestModelId);
}

}  // namespace ash::quick_pair
