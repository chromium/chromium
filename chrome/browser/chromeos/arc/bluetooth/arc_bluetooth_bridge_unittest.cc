// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/bluetooth/arc_bluetooth_bridge.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "components/arc/bluetooth/bluetooth_type_converters.h"
#include "components/arc/mojom/bluetooth.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_bluetooth_instance.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertising_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr int16_t kTestRssi = -50;
constexpr int16_t kTestRssi2 = -70;
constexpr char kTestServiceUUID[] = "00001357-0000-1000-8000-00805f9b34fb";
const std::vector<uint8_t> kEIR = {0x00, 0x01, 0x02};
}  // namespace

namespace arc {

constexpr int kFailureAdvHandle = -1;

class ArcBluetoothBridgeTest : public testing::Test {
 protected:
  void AddTestDevice() {
    bluez::BluezDBusManager* dbus_manager = bluez::BluezDBusManager::Get();
    auto* fake_bluetooth_device_client =
        static_cast<bluez::FakeBluetoothDeviceClient*>(
            dbus_manager->GetBluetoothDeviceClient());
    auto* fake_bluetooth_gatt_service_client =
        static_cast<bluez::FakeBluetoothGattServiceClient*>(
            dbus_manager->GetBluetoothGattServiceClient());
    auto* fake_bluetooth_gatt_characteristic_client =
        static_cast<bluez::FakeBluetoothGattCharacteristicClient*>(
            dbus_manager->GetBluetoothGattCharacteristicClient());

    fake_bluetooth_device_client->CreateDevice(
        dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
        dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
    fake_bluetooth_device_client->UpdateServiceAndManufacturerData(
        dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath),
        {kTestServiceUUID}, /* service_data = */ {},
        /* manufacture_data = */ {});
    fake_bluetooth_device_client->UpdateEIR(
        dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath),
        kEIR);
    fake_bluetooth_gatt_service_client->ExposeHeartRateService(
        dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
    fake_bluetooth_gatt_characteristic_client->ExposeHeartRateCharacteristics(
        fake_bluetooth_gatt_service_client->GetHeartRateServicePath());

    ChangeTestDeviceRssi(kTestRssi);
  }

  void ChangeTestDeviceRssi(uint16_t rssi) {
    bluez::BluezDBusManager* dbus_manager = bluez::BluezDBusManager::Get();
    auto* fake_bluetooth_device_client =
        static_cast<bluez::FakeBluetoothDeviceClient*>(
            dbus_manager->GetBluetoothDeviceClient());
    fake_bluetooth_device_client->UpdateDeviceRSSI(
        dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath),
        rssi);
  }

  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter) {
    adapter_ = adapter;
    get_adapter_run_loop_.Quit();
  }

  void SetUp() override {
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();
    auto fake_bluetooth_device_client =
        std::make_unique<bluez::FakeBluetoothDeviceClient>();
    fake_bluetooth_device_client->RemoveAllDevices();
    dbus_setter->SetBluetoothDeviceClient(
        std::move(fake_bluetooth_device_client));
    dbus_setter->SetBluetoothGattServiceClient(
        std::make_unique<bluez::FakeBluetoothGattServiceClient>());
    dbus_setter->SetBluetoothGattCharacteristicClient(
        std::make_unique<bluez::FakeBluetoothGattCharacteristicClient>());
    dbus_setter->SetBluetoothGattDescriptorClient(
        std::make_unique<bluez::FakeBluetoothGattDescriptorClient>());

    arc_bridge_service_ = std::make_unique<ArcBridgeService>();
    // TODO(hidehiko): Use Singleton instance tied to BrowserContext.
    arc_bluetooth_bridge_ = std::make_unique<ArcBluetoothBridge>(
        nullptr, arc_bridge_service_.get());
    fake_bluetooth_instance_ = std::make_unique<FakeBluetoothInstance>();
    arc_bridge_service_->bluetooth()->SetInstance(
        fake_bluetooth_instance_.get(), 13);
    base::SysInfo::SetChromeOSVersionInfoForTest(
        "CHROMEOS_ARC_ANDROID_SDK_VERSION=28", base::Time::Now());
    WaitForInstanceReady(arc_bridge_service_->bluetooth());

    device::BluetoothAdapterFactory::GetAdapter(base::BindOnce(
        &ArcBluetoothBridgeTest::OnAdapterInitialized, base::Unretained(this)));
    // We will quit the loop once we get the adapter.
    get_adapter_run_loop_.Run();
  }

  void TearDown() override {
    arc_bridge_service_->bluetooth()->CloseInstance(
        fake_bluetooth_instance_.get());
    fake_bluetooth_instance_.reset();
    arc_bluetooth_bridge_.reset();
    arc_bridge_service_.reset();
  }

  // Helper methods for multi advertisement tests.
  int32_t ReserveAdvertisementHandle() {
    constexpr int kSentinelHandle = -2;
    last_adv_handle_ = kSentinelHandle;
    arc_bluetooth_bridge_->ReserveAdvertisementHandle(base::BindOnce(
        &ArcBluetoothBridgeTest::ReserveAdvertisementHandleCallback,
        base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
    // Make sure the callback was called.
    EXPECT_NE(kSentinelHandle, last_adv_handle_);
    return last_adv_handle_;
  }

  mojom::BluetoothGattStatus EnableAdvertisement(
      int adv_handle,
      std::unique_ptr<device::BluetoothAdvertisement::Data> data) {
    last_status_ = mojom::BluetoothGattStatus::GATT_REQUEST_NOT_SUPPORTED;
    arc_bluetooth_bridge_->EnableAdvertisement(
        adv_handle, std::move(data),
        base::BindOnce(&ArcBluetoothBridgeTest::StatusSetterCallback,
                       base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
    EXPECT_NE(mojom::BluetoothGattStatus::GATT_REQUEST_NOT_SUPPORTED,
              last_status_);
    return last_status_;
  }

  mojom::BluetoothGattStatus DisableAdvertisement(int adv_handle) {
    last_status_ = mojom::BluetoothGattStatus::GATT_REQUEST_NOT_SUPPORTED;
    arc_bluetooth_bridge_->DisableAdvertisement(
        adv_handle,
        base::BindOnce(&ArcBluetoothBridgeTest::StatusSetterCallback,
                       base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
    EXPECT_NE(mojom::BluetoothGattStatus::GATT_REQUEST_NOT_SUPPORTED,
              last_status_);
    return last_status_;
  }

  mojom::BluetoothGattStatus ReleaseAdvertisementHandle(int adv_handle) {
    last_status_ = mojom::BluetoothGattStatus::GATT_REQUEST_NOT_SUPPORTED;
    arc_bluetooth_bridge_->ReleaseAdvertisementHandle(
        adv_handle,
        base::BindOnce(&ArcBluetoothBridgeTest::StatusSetterCallback,
                       base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
    EXPECT_NE(mojom::BluetoothGattStatus::GATT_REQUEST_NOT_SUPPORTED,
              last_status_);
    return last_status_;
  }

  void ReserveAdvertisementHandleCallback(mojom::BluetoothGattStatus status,
                                          int32_t adv_handle) {
    if (status == mojom::BluetoothGattStatus::GATT_FAILURE)
      last_adv_handle_ = kFailureAdvHandle;
    else
      last_adv_handle_ = adv_handle;
  }

  void StatusSetterCallback(mojom::BluetoothGattStatus status) {
    last_status_ = status;
  }

  int NumActiveAdvertisements() {
    bluez::FakeBluetoothLEAdvertisingManagerClient* adv_client =
        static_cast<bluez::FakeBluetoothLEAdvertisingManagerClient*>(
            bluez::BluezDBusManager::Get()
                ->GetBluetoothLEAdvertisingManagerClient());
    return adv_client->currently_registered();
  }

  int last_adv_handle_;
  mojom::BluetoothGattStatus last_status_;

  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<FakeBluetoothInstance> fake_bluetooth_instance_;
  std::unique_ptr<ArcBluetoothBridge> arc_bluetooth_bridge_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop get_adapter_run_loop_;
};

// When we add device to bluez::FakeBluetoothDeviceClient, ArcBluetoothBridge
// should send new device data to Android. This test will then check
// the correctness of the device properties sent via arc bridge.
TEST_F(ArcBluetoothBridgeTest, DeviceFound) {
  EXPECT_EQ(0u, fake_bluetooth_instance_->device_found_data().size());
  AddTestDevice();
  EXPECT_EQ(5u, fake_bluetooth_instance_->device_found_data().size());
  const std::vector<mojom::BluetoothPropertyPtr>& prop =
      fake_bluetooth_instance_->device_found_data().back();

  EXPECT_EQ(7u, prop.size());
  EXPECT_TRUE(prop[0]->is_bdname());
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyName),
            prop[0]->get_bdname());
  EXPECT_TRUE(prop[1]->is_bdaddr());
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            prop[1]->get_bdaddr()->To<std::string>());
  EXPECT_TRUE(prop[2]->is_uuids());
  EXPECT_THAT(
      prop[2]->get_uuids(),
      testing::UnorderedElementsAre(
          device::BluetoothUUID(
              bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
          device::BluetoothUUID(kTestServiceUUID)));
  EXPECT_TRUE(prop[3]->is_device_class());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kLowEnergyClass,
            prop[3]->get_device_class());
  EXPECT_TRUE(prop[4]->is_device_type());
  // bluez::FakeBluetoothDeviceClient does not return proper device type.
  EXPECT_TRUE(prop[5]->is_remote_friendly_name());
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyName),
            prop[5]->get_remote_friendly_name());
  EXPECT_TRUE(prop[6]->is_remote_rssi());
  EXPECT_EQ(kTestRssi, prop[6]->get_remote_rssi());

  ChangeTestDeviceRssi(kTestRssi2);
  EXPECT_EQ(6u, fake_bluetooth_instance_->device_found_data().size());
  const std::vector<mojom::BluetoothPropertyPtr>& prop2 =
      fake_bluetooth_instance_->device_found_data().back();
  EXPECT_EQ(7u, prop2.size());
  EXPECT_TRUE(prop2[6]->is_remote_rssi());
  EXPECT_EQ(kTestRssi2, prop2[6]->get_remote_rssi());
}

// Invoke OnDiscoveryStarted to send cached device to BT instance,
// and check correctness of the Advertising data sent via arc bridge.
TEST_F(ArcBluetoothBridgeTest, LEDeviceFound) {
  EXPECT_EQ(0u, fake_bluetooth_instance_->le_device_found_data().size());
  AddTestDevice();
  EXPECT_EQ(1u, fake_bluetooth_instance_->le_device_found_data().size());

  const auto& le_device_found_data =
      fake_bluetooth_instance_->le_device_found_data().back();
  const mojom::BluetoothAddressPtr& addr = le_device_found_data->addr();
  const std::vector<uint8_t>& eir = le_device_found_data->eir();

  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            addr->To<std::string>());
  EXPECT_EQ(kEIR, eir);
  EXPECT_EQ(kTestRssi, le_device_found_data->rssi());

  ChangeTestDeviceRssi(kTestRssi2);
  EXPECT_EQ(2u, fake_bluetooth_instance_->le_device_found_data().size());
  EXPECT_EQ(kTestRssi2,
            fake_bluetooth_instance_->le_device_found_data().back()->rssi());
}

TEST_F(ArcBluetoothBridgeTest, LEDeviceFoundForN) {
  base::SysInfo::SetChromeOSVersionInfoForTest(
      "CHROMEOS_ARC_ANDROID_SDK_VERSION=27", base::Time::Now());
  EXPECT_EQ(0u, fake_bluetooth_instance_->le_device_found_data().size());
  AddTestDevice();
  EXPECT_EQ(1u, fake_bluetooth_instance_->le_device_found_data().size());

  const auto& le_device_found_data =
      fake_bluetooth_instance_->le_device_found_data().back();
  const mojom::BluetoothAddressPtr& addr = le_device_found_data->addr();
  const std::vector<mojom::BluetoothAdvertisingDataPtr>& adv_data =
      le_device_found_data->adv_data();

  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            addr->To<std::string>());
  EXPECT_EQ(2u, adv_data.size());

  EXPECT_TRUE(adv_data[0]->is_local_name());
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyName),
            adv_data[0]->get_local_name());

  EXPECT_TRUE(adv_data[1]->is_service_uuids());
  EXPECT_EQ(1u, adv_data[1]->get_service_uuids().size());
  EXPECT_EQ(kTestServiceUUID,
            adv_data[1]->get_service_uuids()[0].canonical_value());

  EXPECT_EQ(kTestRssi, le_device_found_data->rssi());

  ChangeTestDeviceRssi(kTestRssi2);
  EXPECT_EQ(2u, fake_bluetooth_instance_->le_device_found_data().size());
  EXPECT_EQ(kTestRssi2,
            fake_bluetooth_instance_->le_device_found_data().back()->rssi());
}

// Invoke GetGattDB and check correctness of the GattDB sent via arc bridge.
TEST_F(ArcBluetoothBridgeTest, GetGattDB) {
  AddTestDevice();

  arc_bluetooth_bridge_->GetGattDB(mojom::BluetoothAddress::From(
      std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress)));
  EXPECT_EQ(1u, fake_bluetooth_instance_->gatt_db_result().size());

  const mojom::BluetoothAddressPtr& addr =
      fake_bluetooth_instance_->gatt_db_result().back()->remote_addr();
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            addr->To<std::string>());

  // HeartRateService in bluez::FakeBluetoothDeviceClient consists of
  // Service: HeartRateService
  //     Characteristic: HeartRateMeasurement
  //         Descriptor: ClientCharacteristicConfiguration
  //     Characteristic: BodySensorLocation
  //     Characteristic: HeartRateControlPoint
  const std::vector<mojom::BluetoothGattDBElementPtr>& db =
      fake_bluetooth_instance_->gatt_db_result().back()->db();
  EXPECT_EQ(5u, db.size());

  EXPECT_EQ(device::BluetoothUUID(
                bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
            db[0]->uuid);
  EXPECT_EQ(mojom::BluetoothGattDBAttributeType::BTGATT_DB_PRIMARY_SERVICE,
            db[0]->type);

  EXPECT_EQ(device::BluetoothUUID(bluez::FakeBluetoothGattCharacteristicClient::
                                      kHeartRateMeasurementUUID),
            db[1]->uuid);
  EXPECT_EQ(mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
            db[1]->type);
  EXPECT_EQ(device::BluetoothGattCharacteristic::PROPERTY_NOTIFY |
                device::BluetoothGattCharacteristic::PROPERTY_INDICATE,
            db[1]->properties);

  EXPECT_EQ(device::BluetoothUUID(bluez::FakeBluetoothGattDescriptorClient::
                                      kClientCharacteristicConfigurationUUID),
            db[2]->uuid);
  EXPECT_EQ(mojom::BluetoothGattDBAttributeType::BTGATT_DB_DESCRIPTOR,
            db[2]->type);

  EXPECT_EQ(device::BluetoothUUID(bluez::FakeBluetoothGattCharacteristicClient::
                                      kBodySensorLocationUUID),
            db[3]->uuid);
  EXPECT_EQ(mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
            db[3]->type);
  EXPECT_EQ(device::BluetoothGattCharacteristic::PROPERTY_READ,
            db[3]->properties);

  EXPECT_EQ(device::BluetoothUUID(bluez::FakeBluetoothGattCharacteristicClient::
                                      kHeartRateControlPointUUID),
            db[4]->uuid);
  EXPECT_EQ(mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
            db[4]->type);
  EXPECT_EQ(device::BluetoothGattCharacteristic::PROPERTY_WRITE,
            db[4]->properties);
}

// Invoke multi advertisement methods and make sure they are going down to the
// D-Bus clients.
TEST_F(ArcBluetoothBridgeTest, SingleAdvertisement) {
  int32_t handle = ReserveAdvertisementHandle();
  EXPECT_NE(kFailureAdvHandle, handle);
  EXPECT_EQ(0, NumActiveAdvertisements());

  auto adv_data = std::make_unique<device::BluetoothAdvertisement::Data>(
      device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  mojom::BluetoothGattStatus status =
      EnableAdvertisement(handle, std::move(adv_data));
  EXPECT_EQ(mojom::BluetoothGattStatus::GATT_SUCCESS, status);
  EXPECT_EQ(1, NumActiveAdvertisements());

  status = DisableAdvertisement(handle);
  EXPECT_EQ(mojom::BluetoothGattStatus::GATT_SUCCESS, status);
  EXPECT_EQ(0, NumActiveAdvertisements());

  status = ReleaseAdvertisementHandle(handle);
  EXPECT_EQ(mojom::BluetoothGattStatus::GATT_SUCCESS, status);
  EXPECT_EQ(0, NumActiveAdvertisements());
}

}  // namespace arc
