// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/gatt_battery_percentage_fetcher.h"

#include "base/bind.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothDevice;
using device::BluetoothRemoteGattCharacteristic;
using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;

namespace {

constexpr char kServiceID[] = "service id";
constexpr char kCharacteristicID[] = "characteristic id";
constexpr char kDeviceAddress[] = "AA:BB:CC:DD:EE:FF";
constexpr char kBatteryServiceUUID[] = "180F";
constexpr char kBatteryLevelUUID[] = "2A19";
const uint8_t kBatteryPercentage = 100;

ACTION_TEMPLATE(MoveArg,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  *pointer = std::move(::std::get<k>(args));
}

const device::BluetoothUUID& GetBatteryServiceUUID() {
  static const device::BluetoothUUID battery_service_uuid(kBatteryServiceUUID);
  return battery_service_uuid;
}

const device::BluetoothUUID& GetBatteryLevelUUID() {
  static const device::BluetoothUUID battery_level_uuid(kBatteryLevelUUID);
  return battery_level_uuid;
}

}  // namespace

namespace ash {

class GattBatteryPercentageFetcherTest : public testing::Test {
 protected:
  GattBatteryPercentageFetcherTest() = default;

  ~GattBatteryPercentageFetcherTest() override = default;

  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();

    // By default, |mock_device_| is paired, connected, with Gatt Services
    // Discovery completed and returns |mock_service_| when requested for
    // for available services. These behaviors can be overridden in tests.
    mock_device_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), 0 /* bluetooth_class */, "device_name",
        kDeviceAddress, true /* paired */, true /* connected */);
    ON_CALL(*mock_adapter_, GetDevice(kDeviceAddress))
        .WillByDefault(Return(mock_device_.get()));
    ON_CALL(*mock_device_, IsGattServicesDiscoveryComplete())
        .WillByDefault(Return(true));
    ASSERT_FALSE(mock_device_->battery_percentage());

    // By default, |mock_service_| returns a vector containing
    // |mock_characteristic_| when requested for the battery level
    // characteristic. This behavior can be overridden in tests.
    mock_service_ =
        std::make_unique<NiceMock<device::MockBluetoothGattService>>(
            mock_device_.get(), kServiceID, GetBatteryServiceUUID(),
            true /* is_primary */, false /* is_local */);
    std::vector<device::BluetoothRemoteGattService*> services = {
        mock_service_.get()};
    ON_CALL(*mock_device_, GetGattServices()).WillByDefault(Return(services));

    mock_characteristic_ =
        std::make_unique<NiceMock<device::MockBluetoothGattCharacteristic>>(
            mock_service_.get(), kCharacteristicID, GetBatteryLevelUUID(),
            false /* is_local */,
            BluetoothRemoteGattCharacteristic::PROPERTY_READ,
            BluetoothRemoteGattCharacteristic::PERMISSION_READ);
    std::vector<BluetoothRemoteGattCharacteristic*> characteristics = {
        mock_characteristic_.get()};
    ON_CALL(*mock_service_, GetCharacteristicsByUUID(GetBatteryLevelUUID()))
        .WillByDefault(Return(characteristics));

    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    // Create a GattBatteryPercentageFetcher.
    ExpectGattConnection();
    fetcher_ = GattBatteryPercentageFetcher::Factory::NewInstance(
        mock_adapter_, kDeviceAddress,
        base::BindOnce(
            &GattBatteryPercentageFetcherTest::OnBatteryPercentageFetched,
            base::Unretained(this)));
  }

  void ExpectGattConnection() {
    EXPECT_CALL(*mock_device_, CreateGattConnection(_, _))
        .WillOnce(DoAll(SaveArg<0>(&create_gatt_connection_success_callback_),
                        SaveArg<1>(&create_gatt_connection_error_callback_)));
  }

  void ExpectReadCharacteristic() {
    EXPECT_CALL(*mock_characteristic_, ReadRemoteCharacteristic_(_, _))
        .WillOnce(
            DoAll(MoveArg<0>(&read_remote_characteristic_callback_),
                  MoveArg<1>(&read_remote_characteristic_error_callback_)));
  }

  void OnBatteryPercentageFetched(base::Optional<uint8_t> battery_percentage) {
    battery_percentage_ = battery_percentage;
    battery_percentage_callback_called_ = true;
  }

  void VerifyFetchResult(base::Optional<uint8_t> expected_result) {
    EXPECT_TRUE(battery_percentage_callback_called_);
    EXPECT_EQ(expected_result, battery_percentage_);
  }

  std::unique_ptr<GattBatteryPercentageFetcher> fetcher_;

  base::Optional<uint8_t> battery_percentage_;
  bool battery_percentage_callback_called_ = false;

  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_;

  std::unique_ptr<device::MockBluetoothGattService> mock_service_;
  std::unique_ptr<device::MockBluetoothGattCharacteristic> mock_characteristic_;

  BluetoothDevice::GattConnectionCallback
      create_gatt_connection_success_callback_;
  BluetoothDevice::ConnectErrorCallback create_gatt_connection_error_callback_;
  BluetoothRemoteGattCharacteristic::ValueCallback
      read_remote_characteristic_callback_;
  BluetoothRemoteGattCharacteristic::ErrorCallback
      read_remote_characteristic_error_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GattBatteryPercentageFetcherTest);
};

TEST_F(GattBatteryPercentageFetcherTest,
       ReadBattery_GattServicesDiscoveredOnGattConnection) {
  ExpectReadCharacteristic();
  create_gatt_connection_success_callback_.Run(
      std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
          mock_adapter_, kDeviceAddress));

  std::move(read_remote_characteristic_callback_).Run({kBatteryPercentage});
  VerifyFetchResult(kBatteryPercentage);
}

TEST_F(GattBatteryPercentageFetcherTest,
       ReadBattery_GattServicesDiscoveredAfterGattConnection) {
  ExpectReadCharacteristic();
  ON_CALL(*mock_device_, IsGattServicesDiscoveryComplete())
      .WillByDefault(Return(false));

  // GattServicesDiscoveryComplete() should not have run yet.
  EXPECT_TRUE(read_remote_characteristic_error_callback_.is_null());
  EXPECT_TRUE(read_remote_characteristic_callback_.is_null());

  create_gatt_connection_success_callback_.Run(
      std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
          mock_adapter_, kDeviceAddress));

  mock_adapter_->NotifyGattServicesDiscovered(mock_device_.get());

  // GattServicesDiscoveryComplete() should have run.
  std::move(read_remote_characteristic_callback_).Run({kBatteryPercentage});
  VerifyFetchResult(kBatteryPercentage);
}

TEST_F(GattBatteryPercentageFetcherTest,
       IgnoreGattServicesDiscoveredBeforeGattConnection) {
  // Case where the GATT connection has not been established yet, but the
  // services are done with discovery. Should not try reading the battery level.
  EXPECT_CALL(*mock_characteristic_, ReadRemoteCharacteristic_(_, _)).Times(0);
  mock_adapter_->NotifyGattServicesDiscovered(mock_device_.get());
}

TEST_F(GattBatteryPercentageFetcherTest, ErrorOpeningGattConnection) {
  create_gatt_connection_error_callback_.Run(
      BluetoothDevice::ERROR_AUTH_TIMEOUT);
  VerifyFetchResult(base::nullopt /* expected_result */);
}

TEST_F(GattBatteryPercentageFetcherTest, BatteryServiceUnavailable) {
  ON_CALL(*mock_device_, GetGattServices())
      .WillByDefault(
          Return(std::vector<device::BluetoothRemoteGattService*>()));

  create_gatt_connection_success_callback_.Run(
      std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
          mock_adapter_, kDeviceAddress));
  VerifyFetchResult(base::nullopt /* expected_result */);
}

TEST_F(GattBatteryPercentageFetcherTest, MissingBatteryLevelCharacteristic) {
  ON_CALL(*mock_service_, GetCharacteristicsByUUID(GetBatteryLevelUUID()))
      .WillByDefault(Return(std::vector<BluetoothRemoteGattCharacteristic*>()));
  EXPECT_CALL(*mock_characteristic_, ReadRemoteCharacteristic_(_, _)).Times(0);

  create_gatt_connection_success_callback_.Run(
      std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
          mock_adapter_, kDeviceAddress));
  VerifyFetchResult(base::nullopt /* expected_result */);
}

TEST_F(GattBatteryPercentageFetcherTest, ErrorReadingRemoteCharacteristic) {
  ExpectReadCharacteristic();
  create_gatt_connection_success_callback_.Run(
      std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
          mock_adapter_, kDeviceAddress));

  std::move(read_remote_characteristic_error_callback_)
      .Run(device::BluetoothGattService::GATT_ERROR_UNKNOWN);
  VerifyFetchResult(base::nullopt /* expected_result */);
}

TEST_F(GattBatteryPercentageFetcherTest,
       BadFormatForBatteryLevelValue_MadeOfMultipleBytes) {
  ExpectReadCharacteristic();
  create_gatt_connection_success_callback_.Run(
      std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
          mock_adapter_, kDeviceAddress));

  // Battery value made of a multibyte vector.
  std::move(read_remote_characteristic_callback_)
      .Run({kBatteryPercentage, kBatteryPercentage});
  VerifyFetchResult(base::nullopt /* expected_result */);
}

TEST_F(GattBatteryPercentageFetcherTest,
       BadFormatForBatteryLevelValue_ValueAbove100Percent) {
  ExpectReadCharacteristic();
  create_gatt_connection_success_callback_.Run(
      std::make_unique<NiceMock<device::MockBluetoothGattConnection>>(
          mock_adapter_, kDeviceAddress));

  uint8_t new_battery_percentage = 101;
  std::move(read_remote_characteristic_callback_).Run({new_battery_percentage});
  VerifyFetchResult(base::nullopt /* expected_result */);
}

}  // namespace ash
