// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_id_map.h"

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
constexpr char kTestBLEDeviceId[] = "test_ble_device_id";
constexpr char kTestClassicAddress[] = "test_classic_address";
constexpr char kTestClassicDeviceId[] = "test_classic_device_id";

}  // namespace

namespace ash {
namespace quick_pair {

// For convenience.
using ::testing::Return;

class DeviceIdMapTest : public AshTestBase {
 public:
  DeviceIdMapTest()
      : adapter_(new testing::NiceMock<device::MockBluetoothAdapter>),
        ble_bluetooth_device_(adapter_.get(),
                              0,
                              "Test ble name",
                              kTestBLEAddress,
                              false,
                              true),
        classic_bluetooth_device_(adapter_.get(),
                                  0,
                                  "Test classic name",
                                  kTestClassicAddress,
                                  false,
                                  true) {
    ON_CALL(ble_bluetooth_device_, GetIdentifier)
        .WillByDefault(Return(kTestBLEDeviceId));
    ON_CALL(classic_bluetooth_device_, GetIdentifier)
        .WillByDefault(Return(kTestClassicDeviceId));
    ON_CALL(*adapter_, GetDevice(kTestBLEAddress))
        .WillByDefault(Return(&ble_bluetooth_device_));
    ON_CALL(*adapter_, GetDevice(kTestClassicAddress))
        .WillByDefault(Return(&classic_bluetooth_device_));
  }

  void SetUp() override {
    AshTestBase::SetUp();

    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    device_ = base::MakeRefCounted<Device>(kTestModelId, kTestBLEAddress,
                                           Protocol::kFastPairInitial);
    device_->set_classic_address(kTestClassicAddress);
    device_id_map_ = std::make_unique<DeviceIdMap>(adapter_);
  }

 protected:
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  testing::NiceMock<device::MockBluetoothDevice> ble_bluetooth_device_;
  testing::NiceMock<device::MockBluetoothDevice> classic_bluetooth_device_;
  scoped_refptr<Device> device_;
  std::unique_ptr<DeviceIdMap> device_id_map_;
};

TEST_F(DeviceIdMapTest, SaveModelIdForDeviceValid) {
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  absl::optional<const std::string> ble_model_id =
      device_id_map_->GetModelIdForDeviceId(kTestBLEDeviceId);
  EXPECT_TRUE(ble_model_id);
  EXPECT_EQ(ble_model_id.value(), kTestModelId);
  absl::optional<const std::string> classic_model_id =
      device_id_map_->GetModelIdForDeviceId(kTestClassicDeviceId);
  EXPECT_TRUE(classic_model_id);
  EXPECT_EQ(classic_model_id.value(), kTestModelId);
}

TEST_F(DeviceIdMapTest, SaveModelIdForDeviceValidOnlyClassicAddress) {
  // Pretend adapter can't find the device for the BLE address.
  // A record should still be saved for the valid address.
  EXPECT_CALL(*adapter_, GetDevice(kTestBLEAddress)).WillOnce(Return(nullptr));
  EXPECT_CALL(*adapter_, GetDevice(kTestClassicAddress))
      .WillOnce(Return(&classic_bluetooth_device_));
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  absl::optional<const std::string> ble_model_id =
      device_id_map_->GetModelIdForDeviceId(kTestBLEDeviceId);
  EXPECT_FALSE(ble_model_id);
  absl::optional<const std::string> classic_model_id =
      device_id_map_->GetModelIdForDeviceId(kTestClassicDeviceId);
  EXPECT_TRUE(classic_model_id);
  EXPECT_EQ(classic_model_id.value(), kTestModelId);
}

TEST_F(DeviceIdMapTest, SaveModelIdForDeviceValidOnlyBLEAddress) {
  // Pretend adapter can't find the device for the classic address.
  // A record should still be saved for the valid address.
  EXPECT_CALL(*adapter_, GetDevice(kTestBLEAddress))
      .WillOnce(Return(&ble_bluetooth_device_));
  EXPECT_CALL(*adapter_, GetDevice(kTestClassicAddress))
      .WillOnce(Return(nullptr));
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  absl::optional<const std::string> ble_model_id =
      device_id_map_->GetModelIdForDeviceId(kTestBLEDeviceId);
  EXPECT_TRUE(ble_model_id);
  EXPECT_EQ(ble_model_id.value(), kTestModelId);
  absl::optional<const std::string> classic_model_id =
      device_id_map_->GetModelIdForDeviceId(kTestClassicDeviceId);
  EXPECT_FALSE(classic_model_id);
}

TEST_F(DeviceIdMapTest, SaveModelIdForDeviceInvalidDeviceNotFound) {
  // Pretend adapter can't find the device.
  EXPECT_CALL(*adapter_, GetDevice(kTestBLEAddress)).WillOnce(Return(nullptr));
  EXPECT_CALL(*adapter_, GetDevice(kTestClassicAddress))
      .WillOnce(Return(nullptr));
  EXPECT_FALSE(device_id_map_->SaveModelIdForDevice(device_));
  absl::optional<const std::string> ble_model_id =
      device_id_map_->GetModelIdForDeviceId(kTestBLEDeviceId);
  EXPECT_FALSE(ble_model_id);
  absl::optional<const std::string> classic_model_id =
      device_id_map_->GetModelIdForDeviceId(kTestClassicDeviceId);
  EXPECT_FALSE(classic_model_id);
}

TEST_F(DeviceIdMapTest, PersistRecordsForDeviceValid) {
  // First, save the device ID records to memory.
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));

  // Validate that the ID records are persisted to prefs.
  PrefService* local_state = Shell::Get()->local_state();
  const base::Value::Dict& device_id_map_dict =
      local_state->GetDict(DeviceIdMap::kDeviceIdMapPref);
  const std::string* ble_model_id =
      device_id_map_dict.FindString(kTestBLEDeviceId);
  EXPECT_TRUE(ble_model_id);
  EXPECT_EQ(*ble_model_id, kTestModelId);
  const std::string* classic_model_id =
      device_id_map_dict.FindString(kTestClassicDeviceId);
  EXPECT_TRUE(classic_model_id);
  EXPECT_EQ(*classic_model_id, kTestModelId);
}

TEST_F(DeviceIdMapTest, PersistRecordsForDeviceValidOnlyClassicAddress) {
  // Pretend adapter can't find the device for one of the addresses.
  // A record should still be saved for the valid address.
  EXPECT_CALL(*adapter_, GetDevice(kTestBLEAddress))
      .Times(2)
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*adapter_, GetDevice(kTestClassicAddress))
      .Times(2)
      .WillRepeatedly(Return(&classic_bluetooth_device_));
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));
}

TEST_F(DeviceIdMapTest, PersistRecordsForDeviceValidOnlyBLEAddress) {
  // Pretend adapter can't find the device for the BLE address.
  // A record should still be saved for the valid address.
  EXPECT_CALL(*adapter_, GetDevice(kTestBLEAddress))
      .Times(2)
      .WillRepeatedly(Return(&ble_bluetooth_device_));
  EXPECT_CALL(*adapter_, GetDevice(kTestClassicAddress))
      .Times(2)
      .WillRepeatedly(Return(nullptr));
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));
}

TEST_F(DeviceIdMapTest, PersistRecordsForDeviceValidDoublePersist) {
  // First, save the device ID records to memory.
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));

  // When persisting a second time, should overwrite record and
  // still return true.
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));
}

TEST_F(DeviceIdMapTest, PersistRecordsForDeviceInvalidNotSaved) {
  // Don't save the device ID record to memory.
  EXPECT_FALSE(device_id_map_->PersistRecordsForDevice(device_));
}

TEST_F(DeviceIdMapTest, EvictDeviceIdRecordValid) {
  // First, persist the device ID record to disk.
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));
  EXPECT_TRUE(device_id_map_->EvictDeviceIdRecord(kTestBLEDeviceId));

  // Validate that the ID records are evicted from prefs.
  PrefService* local_state = Shell::Get()->local_state();
  const base::Value::Dict& device_id_map_dict =
      local_state->GetDict(DeviceIdMap::kDeviceIdMapPref);
  const std::string* model_id = device_id_map_dict.FindString(kTestBLEDeviceId);
  EXPECT_FALSE(model_id);
}

TEST_F(DeviceIdMapTest, EvictDeviceIdRecordInvalidDeviceId) {
  // Don't save the device ID records to disk.
  EXPECT_FALSE(device_id_map_->EvictDeviceIdRecord(kTestBLEDeviceId));
}

TEST_F(DeviceIdMapTest, EvictDeviceIdRecordInvalidDoubleFree) {
  // First, persist the device ID records to disk.
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));
  EXPECT_TRUE(device_id_map_->EvictDeviceIdRecord(kTestBLEDeviceId));

  // The second evict should fail.
  EXPECT_FALSE(device_id_map_->EvictDeviceIdRecord(kTestBLEDeviceId));
}

TEST_F(DeviceIdMapTest, GetModelIdForDeviceIdValid) {
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));

  absl::optional<const std::string> model_id =
      device_id_map_->GetModelIdForDeviceId(kTestBLEDeviceId);
  EXPECT_TRUE(model_id);
  EXPECT_EQ(model_id.value(), kTestModelId);
}

TEST_F(DeviceIdMapTest, GetModelIdForDeviceIdInvalidUninitialized) {
  // Don't initialize the dictionary with any results.
  absl::optional<const std::string> model_id =
      device_id_map_->GetModelIdForDeviceId(kTestBLEDeviceId);
  EXPECT_FALSE(model_id);
}

TEST_F(DeviceIdMapTest, GetModelIdForDeviceIdInvalidNotAdded) {
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));

  absl::optional<const std::string> model_id =
      device_id_map_->GetModelIdForDeviceId("not found id");
  EXPECT_FALSE(model_id);
}

TEST_F(DeviceIdMapTest, HasPersistedRecordsForModelIdTrueAfterPersist) {
  // First, persist the device ID records to disk.
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));
  EXPECT_TRUE(device_id_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceIdMapTest, HasPersistedRecordsForModelIdTrueAfterOneEviction) {
  // First, persist the device ID records to disk.
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));
  // Evict one of the records that points to this model ID.
  EXPECT_TRUE(device_id_map_->EvictDeviceIdRecord(kTestClassicDeviceId));
  EXPECT_TRUE(device_id_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceIdMapTest, HasPersistedRecordsForModelIdFalseAfterAllEvictions) {
  // First, persist the device ID records to disk.
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));
  // Evict all of the records that points to this model ID.
  EXPECT_TRUE(device_id_map_->EvictDeviceIdRecord(kTestClassicDeviceId));
  EXPECT_TRUE(device_id_map_->EvictDeviceIdRecord(kTestBLEDeviceId));
  EXPECT_FALSE(device_id_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceIdMapTest, HasPersistedRecordsForModelIdFalseNoPersist) {
  // Don't persist the device ID records to disk.
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_FALSE(device_id_map_->HasPersistedRecordsForModelId(kTestModelId));
}

TEST_F(DeviceIdMapTest, LoadPersistedIdRecordFromPrefs) {
  // First, persist the device ID records to disk.
  EXPECT_TRUE(device_id_map_->SaveModelIdForDevice(device_));
  EXPECT_TRUE(device_id_map_->PersistRecordsForDevice(device_));

  // A new/restarted DeviceIdMap instance should load persisted ID records
  // from prefs.
  DeviceIdMap new_device_id_map(adapter_);
  absl::optional<const std::string> model_id =
      new_device_id_map.GetModelIdForDeviceId(kTestBLEDeviceId);
  EXPECT_TRUE(model_id);
  EXPECT_EQ(model_id.value(), kTestModelId);
}

}  // namespace quick_pair
}  // namespace ash
