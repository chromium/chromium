// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_address_map.h"

#include <optional>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"

namespace {

constexpr char kTestModelId[] = "test_model_id";
constexpr char kTestClassicAddress[] = "test_classic_address";
constexpr char kTestClassicAddress2[] = "test_classic_address_2";

}  // namespace

namespace ash::quick_pair {

// For convenience.
using ::testing::Return;

class DeviceAddressMapTest : public AshTestBase {
 public:
  DeviceAddressMapTest() {}

  void SetUp() override {
    AshTestBase::SetUp();

    // Initialize device_ with no classic address.
    device_ = base::MakeRefCounted<Device>(kTestModelId, "address",
                                           Protocol::kFastPairInitial);

    device_address_map_ = std::make_unique<DeviceAddressMap>();
  }

 protected:
  scoped_refptr<Device> device_;
  std::unique_ptr<DeviceAddressMap> device_address_map_;
};

TEST_F(DeviceAddressMapTest, SaveModelIdForDeviceValid) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  std::optional<const std::string> model_id =
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress);
  EXPECT_TRUE(model_id);
  EXPECT_EQ(model_id.value(), kTestModelId);
}

TEST_F(DeviceAddressMapTest, SaveModelIdForDeviceInvalidDeviceNotFound) {
  // Device has no classic address set.
  EXPECT_FALSE(device_address_map_->SaveModelIdForDevice(device_));
  std::optional<const std::string> model_id =
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress);
  EXPECT_FALSE(model_id);
}

TEST_F(DeviceAddressMapTest, PersistRecordsForDeviceValid) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  // First, save the mac address to model ID records to memory.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));

  // Validate that the ID records are persisted to prefs.
  PrefService* local_state = Shell::Get()->local_state();
  const base::Value::Dict& device_address_map_dict =
      local_state->GetDict(DeviceAddressMap::kDeviceAddressMapPref);
  const std::string* model_id =
      device_address_map_dict.FindString(kTestClassicAddress);
  EXPECT_TRUE(model_id);
  EXPECT_EQ(*model_id, kTestModelId);
}

TEST_F(DeviceAddressMapTest, PersistRecordsForDeviceValidDoublePersist) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  // First, save the mac address records to memory.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));

  // When persisting a second time, should overwrite record and
  // still return true.
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
}

TEST_F(DeviceAddressMapTest, PersistRecordsForDeviceInvalidNotSaved) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  // Don't save the mac address record to memory.
  EXPECT_FALSE(device_address_map_->PersistRecordsForDevice(device_));
}

TEST_F(DeviceAddressMapTest, EvictMacAddressRecordValid) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  // First, persist the mac address record to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
  EXPECT_TRUE(device_address_map_->EvictMacAddressRecord(kTestClassicAddress));

  // Validate that the ID records are evicted from prefs.
  PrefService* local_state = Shell::Get()->local_state();
  const base::Value::Dict& device_address_map_dict =
      local_state->GetDict(DeviceAddressMap::kDeviceAddressMapPref);
  const std::string* model_id =
      device_address_map_dict.FindString(kTestClassicAddress);
  EXPECT_FALSE(model_id);
}

TEST_F(DeviceAddressMapTest, EvictMacAddressRecordInvalidDeviceAddress) {
  // Don't save the mac address records to disk.
  EXPECT_FALSE(device_address_map_->EvictMacAddressRecord(kTestClassicAddress));
}

TEST_F(DeviceAddressMapTest, EvictMacAddressRecordInvalidDoubleFree) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  // First, persist the mac address records to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
  EXPECT_TRUE(device_address_map_->EvictMacAddressRecord(kTestClassicAddress));

  // The second evict should fail.
  EXPECT_FALSE(device_address_map_->EvictMacAddressRecord(kTestClassicAddress));
}

TEST_F(DeviceAddressMapTest, GetModelIdForMacAddressValid) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));

  std::optional<const std::string> model_id =
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress);
  EXPECT_TRUE(model_id);
  EXPECT_EQ(model_id.value(), kTestModelId);
}

TEST_F(DeviceAddressMapTest, GetModelIdForMacAddressInvalidUninitialized) {
  // Don't initialize the dictionary with any results.
  std::optional<const std::string> model_id =
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress);
  EXPECT_FALSE(model_id);
}

TEST_F(DeviceAddressMapTest, GetModelIdForMacAddressInvalidNotAdded) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));

  std::optional<const std::string> model_id =
      device_address_map_->GetModelIdForMacAddress("not found id");
  EXPECT_FALSE(model_id);
}

TEST_F(DeviceAddressMapTest, HasPersistedRecordsForModelIdTrueAfterPersist) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  // First, persist the mac address records to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));
  EXPECT_TRUE(device_address_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceAddressMapTest,
       HasPersistedRecordsForModelIdTrueAfterOneEviction) {
  // Set a valid classic address and persist the mac address records to disk.
  device_->set_classic_address(kTestClassicAddress);
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));

  // Set a different classic address, as if a second device with the samel model
  // ID was paired, and persist the mac address records to disk.
  device_->set_classic_address(kTestClassicAddress2);
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));

  // Evict one of the records that points to this model ID.
  EXPECT_TRUE(device_address_map_->EvictMacAddressRecord(kTestClassicAddress));

  // There is still one mac address to model ID for record for this model ID.
  EXPECT_TRUE(device_address_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceAddressMapTest,
       HasPersistedRecordsForModelIdFalseAfterAllEvictions) {
  // Set a valid classic address and persist the mac address records to disk.
  device_->set_classic_address(kTestClassicAddress);
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));

  // Set a different classic address, as if a second device with the samel model
  // ID was paired, and persist the mac address records to disk.
  device_->set_classic_address(kTestClassicAddress2);
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));

  // Evict both of the records that point to this model ID.
  EXPECT_TRUE(device_address_map_->EvictMacAddressRecord(kTestClassicAddress));
  EXPECT_TRUE(device_address_map_->EvictMacAddressRecord(kTestClassicAddress2));

  // There are no more mac address to model ID records for this model ID.
  EXPECT_FALSE(
      device_address_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceAddressMapTest, HasPersistedRecordsForModelIdFalseNoPersist) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  // Don't persist the mac address records to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_FALSE(
      device_address_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceAddressMapTest, LoadPersistedIdRecordFromPrefs) {
  // Set a valid classic address.
  device_->set_classic_address(kTestClassicAddress);

  // First, persist the mac address records to disk.
  EXPECT_TRUE(device_address_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_address_map_->PersistRecordsForDevice(device_));

  // A new/restarted DeviceAddressMap instance should load persisted ID records
  // from prefs.
  DeviceAddressMap new_device_address_map = DeviceAddressMap();
  std::optional<const std::string> model_id =
      new_device_address_map.GetModelIdForMacAddress(kTestClassicAddress);
  EXPECT_TRUE(model_id);
  EXPECT_EQ(model_id.value(), kTestModelId);
}

}  // namespace ash::quick_pair
