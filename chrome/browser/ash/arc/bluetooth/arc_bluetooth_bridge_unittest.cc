// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/bluetooth/arc_bluetooth_bridge.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_bluetooth_instance.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/arc/bluetooth/arc_bluez_bridge.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
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

// This unittest defaults to testing BlueZ. For Floss, use |ArcFlossBridgeTest|.
class ArcBluetoothBridgeTest : public testing::Test {
 protected:
  void StartDiscovery() {
    base::RunLoop run_loop;
    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(),
        base::BindLambdaForTesting(
            [this, &run_loop](std::unique_ptr<device::BluetoothDiscoverySession>
                                  discovery_session) {
              arc_bluetooth_bridge_->discovery_session_ =
                  std::move(discovery_session);
              run_loop.Quit();
            }),
        base::DoNothing());
    run_loop.Run();
  }

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

  void ChangeTestDeviceConnected(bool connected) {
    bluez::BluezDBusManager* dbus_manager = bluez::BluezDBusManager::Get();
    auto* fake_bluetooth_device_client =
        static_cast<bluez::FakeBluetoothDeviceClient*>(
            dbus_manager->GetBluetoothDeviceClient());
    bluez::FakeBluetoothDeviceClient::Properties* properties =
        fake_bluetooth_device_client->GetProperties(
            dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
    properties->connected.ReplaceValue(connected);
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
    auto fake_bluetooth_adapter_client =
        std::make_unique<bluez::FakeBluetoothAdapterClient>();
    fake_bluetooth_adapter_client->SetDiscoverySimulation(false);
    fake_bluetooth_adapter_client->SetSimulationIntervalMs(0);
    dbus_setter->SetBluetoothAdapterClient(
        std::move(fake_bluetooth_adapter_client));
    dbus_setter->SetBluetoothGattServiceClient(
        std::make_unique<bluez::FakeBluetoothGattServiceClient>());
    dbus_setter->SetBluetoothGattCharacteristicClient(
        std::make_unique<bluez::FakeBluetoothGattCharacteristicClient>());
    dbus_setter->SetBluetoothGattDescriptorClient(
        std::make_unique<bluez::FakeBluetoothGattDescriptorClient>());

    arc_bridge_service_ = std::make_unique<ArcBridgeService>();
    // TODO(hidehiko): Use Singleton instance tied to BrowserContext.
    arc_bluetooth_bridge_ =
        std::make_unique<ArcBluezBridge>(nullptr, arc_bridge_service_.get());
    fake_bluetooth_instance_ = std::make_unique<FakeBluetoothInstance>();
    arc_bridge_service_->bluetooth()->SetInstance(
        fake_bluetooth_instance_.get(), 20);
    WaitForInstanceReady(arc_bridge_service_->bluetooth());

    device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
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
  EXPECT_EQ(0u,
            fake_bluetooth_instance_->device_properties_changed_data().size());
  StartDiscovery();
  AddTestDevice();
  // Only the first one should invoke a device_found callback. The following
  // device change events should invode the remote_device_properties callback.
  EXPECT_EQ(1u, fake_bluetooth_instance_->device_found_data().size());
  EXPECT_EQ(4u,
            fake_bluetooth_instance_->device_properties_changed_data().size());
  const std::vector<mojom::BluetoothPropertyPtr>& prop =
      fake_bluetooth_instance_->device_properties_changed_data().back();

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
  EXPECT_EQ(1u, fake_bluetooth_instance_->device_found_data().size());
  EXPECT_EQ(5u,
            fake_bluetooth_instance_->device_properties_changed_data().size());
  const std::vector<mojom::BluetoothPropertyPtr>& prop2 =
      fake_bluetooth_instance_->device_properties_changed_data().back();
  EXPECT_EQ(7u, prop2.size());
  EXPECT_TRUE(prop2[6]->is_remote_rssi());
  EXPECT_EQ(kTestRssi2, prop2[6]->get_remote_rssi());
}

// Invoke OnDiscoveryStarted to send cached device to BT instance,
// and check correctness of the Advertising data sent via arc bridge.
TEST_F(ArcBluetoothBridgeTest, LEDeviceFound) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_ARC_ANDROID_SDK_VERSION=28", base::Time::Now());

  EXPECT_EQ(0u, fake_bluetooth_instance_->le_device_found_data().size());
  StartDiscovery();
  AddTestDevice();
  EXPECT_EQ(3u, fake_bluetooth_instance_->le_device_found_data().size());

  const auto& le_device_found_data =
      fake_bluetooth_instance_->le_device_found_data().back();
  const mojom::BluetoothAddressPtr& addr = le_device_found_data->addr();
  const std::vector<uint8_t>& eir = le_device_found_data->eir();

  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            addr->To<std::string>());
  EXPECT_EQ(kEIR, eir);
  EXPECT_EQ(kTestRssi, le_device_found_data->rssi());

  ChangeTestDeviceRssi(kTestRssi2);
  EXPECT_EQ(4u, fake_bluetooth_instance_->le_device_found_data().size());
  EXPECT_EQ(kTestRssi2,
            fake_bluetooth_instance_->le_device_found_data().back()->rssi());
}

// If ARC starts the connection by LEConnect(), OnLEConnectionStateChange()
// should be invoked when the connection is up/down. OnConnectionStateChanged()
// should always be invoked when the physical link state changed.
TEST_F(ArcBluetoothBridgeTest, DeviceConnectStateChangedAfterLEConnectReuqest) {
  StartDiscovery();
  AddTestDevice();
  arc_bluetooth_bridge_->ConnectLEDevice(mojom::BluetoothAddress::From(
      std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress)));

  // OnConnectionStateChanged() should be invoked.
  ASSERT_EQ(1u,
            fake_bluetooth_instance_->connection_state_changed_data().size());
  const auto& connected_data =
      fake_bluetooth_instance_->connection_state_changed_data().back();
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            connected_data->addr()->To<std::string>());
  EXPECT_EQ(device::BLUETOOTH_TRANSPORT_LE, connected_data->device_type());
  EXPECT_EQ(true, connected_data->connected());

  // OnLEConnectionStateChange() should be invoked.
  ASSERT_EQ(1u,
            fake_bluetooth_instance_->le_connection_state_change_data().size());
  const auto& le_connected_data =
      fake_bluetooth_instance_->le_connection_state_change_data().back();
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            le_connected_data->addr()->To<std::string>());
  EXPECT_EQ(true, le_connected_data->connected());

  // Device is disconnected.
  ChangeTestDeviceConnected(false);

  // OnConnectionStateChanged() should be invoked.
  ASSERT_EQ(2u,
            fake_bluetooth_instance_->connection_state_changed_data().size());
  const auto& disconnected_data =
      fake_bluetooth_instance_->connection_state_changed_data().back();
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            disconnected_data->addr()->To<std::string>());
  EXPECT_EQ(device::BLUETOOTH_TRANSPORT_LE, disconnected_data->device_type());
  EXPECT_EQ(false, disconnected_data->connected());

  // OnLEConnectionStateChange() should be invoked.
  ASSERT_EQ(2u,
            fake_bluetooth_instance_->le_connection_state_change_data().size());
  const auto& le_disconnected_data =
      fake_bluetooth_instance_->le_connection_state_change_data().back();
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            le_disconnected_data->addr()->To<std::string>());
  EXPECT_EQ(false, le_disconnected_data->connected());
}

// If the connection is up/down and is not requested by ARC (e.g., in the case
// that another app in CrOS starts the connection, or it is an incoming
// connection), OnLEConnectionStateChange() should not be invoked.
// OnConnectionStateChanged() should always be invoked when the physical link
// state changed.
TEST_F(ArcBluetoothBridgeTest,
       DeviceConnectStateChangedWithoutLEConnectRequest) {
  StartDiscovery();
  AddTestDevice();
  ChangeTestDeviceConnected(true);

  // OnConnectionStateChanged() should be invoked.
  ASSERT_EQ(1u,
            fake_bluetooth_instance_->connection_state_changed_data().size());
  const auto& connected_data =
      fake_bluetooth_instance_->connection_state_changed_data().back();
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            connected_data->addr()->To<std::string>());
  EXPECT_EQ(device::BLUETOOTH_TRANSPORT_LE, connected_data->device_type());
  EXPECT_EQ(true, connected_data->connected());

  // OnLEConnectionStateChange() should not be invoked.
  ASSERT_EQ(0u,
            fake_bluetooth_instance_->le_connection_state_change_data().size());

  // Device is disconnected.
  ChangeTestDeviceConnected(false);

  // OnConnectionStateChanged() should be invoked.
  ASSERT_EQ(2u,
            fake_bluetooth_instance_->connection_state_changed_data().size());
  const auto& disconnected_data =
      fake_bluetooth_instance_->connection_state_changed_data().back();
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            disconnected_data->addr()->To<std::string>());
  EXPECT_EQ(device::BLUETOOTH_TRANSPORT_LE, disconnected_data->device_type());
  EXPECT_EQ(false, disconnected_data->connected());

  // OnLEConnectionStateChange() should not be invoked.
  ASSERT_EQ(0u,
            fake_bluetooth_instance_->le_connection_state_change_data().size());
}

// Invoke GetGattDB and check correctness of the GattDB sent via arc bridge.
TEST_F(ArcBluetoothBridgeTest, GetGattDB) {
  StartDiscovery();
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

TEST_F(ArcBluetoothBridgeTest, ServiceChanged) {
  // Set up device and service
  StartDiscovery();
  AddTestDevice();

  bluez::BluezDBusManager* dbus_manager = bluez::BluezDBusManager::Get();
  auto* fake_bluetooth_gatt_service_client =
      static_cast<bluez::FakeBluetoothGattServiceClient*>(
      dbus_manager->GetBluetoothGattServiceClient());

  device::BluetoothDevice* device = adapter_->GetDevices()[0];
  device::BluetoothRemoteGattService* service =
      device->GetGattService(fake_bluetooth_gatt_service_client->GetHeartRateServicePath().value());

  // When OnServiceChanged is called, service changed flag will be set
  // true, while reset_service_changed_flag will set this flag to false.
  // Here is to test whether OnServiceChanged is called after GattServiceAdded
  // and GattServiceRemoved is called.
  fake_bluetooth_instance_->reset_service_changed_flag();
  EXPECT_FALSE(fake_bluetooth_instance_->get_service_changed_flag());
  arc_bluetooth_bridge_->GattServiceAdded(adapter_.get(), device, service);
  EXPECT_TRUE(fake_bluetooth_instance_->get_service_changed_flag());

  fake_bluetooth_instance_->reset_service_changed_flag();
  EXPECT_FALSE(fake_bluetooth_instance_->get_service_changed_flag());
  arc_bluetooth_bridge_->GattServiceRemoved(adapter_.get(), device, service);
  EXPECT_TRUE(fake_bluetooth_instance_->get_service_changed_flag());

  fake_bluetooth_instance_->reset_service_changed_flag();
  EXPECT_FALSE(fake_bluetooth_instance_->get_service_changed_flag());
  arc_bluetooth_bridge_->GattServiceChanged(adapter_.get(), service);
  EXPECT_TRUE(fake_bluetooth_instance_->get_service_changed_flag());
}

TEST_F(ArcBluetoothBridgeTest, ReadMissingDescriptorFailsGracefully) {
  base::RunLoop run_loop;

  // Pass clearly invalid values to guarantee that we won't be able to find a
  // valid GATT descriptor.
  arc_bluetooth_bridge_->ReadGattDescriptor(
      /*remote_addr=*/mojom::BluetoothAddress::New(),
      /*service_id=*/mojom::BluetoothGattServiceID::New(),
      /*char_id=*/mojom::BluetoothGattID::New(),
      /*desc_id=*/mojom::BluetoothGattID::New(),
      base::BindOnce(
          [](base::RepeatingClosure quit_closure,
             mojom::BluetoothGattValuePtr value) {
            ASSERT_TRUE(value);
            EXPECT_TRUE(value->value.empty());
            EXPECT_EQ(value->status, mojom::BluetoothGattStatus::GATT_FAILURE);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ArcBluetoothBridgeTest, WritingMissingDescriptorFailsGracefully) {
  base::RunLoop run_loop;

  // Pass clearly invalid values to guarantee that we won't be able to find a
  // valid GATT descriptor.
  arc_bluetooth_bridge_->WriteGattDescriptor(
      /*remote_addr=*/mojom::BluetoothAddress::New(),
      /*service_id=*/mojom::BluetoothGattServiceID::New(),
      /*char_id=*/mojom::BluetoothGattID::New(),
      /*desc_id=*/mojom::BluetoothGattID::New(),
      /*value=*/mojom::BluetoothGattValue::New(),
      base::BindOnce(
          [](base::RepeatingClosure quit_closure,
             mojom::BluetoothGattStatus status) {
            EXPECT_EQ(status, mojom::BluetoothGattStatus::GATT_FAILURE);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ArcBluetoothBridgeTest, ReadMissingCharacteristicFailsGracefully) {
  base::RunLoop run_loop;

  // Pass clearly invalid values to guarantee that we won't be able to find a
  // valid GATT characteristic.
  arc_bluetooth_bridge_->ReadGattCharacteristic(
      /*remote_addr=*/mojom::BluetoothAddress::New(),
      /*service_id=*/mojom::BluetoothGattServiceID::New(),
      /*char_id=*/mojom::BluetoothGattID::New(),
      base::BindOnce(
          [](base::RepeatingClosure quit_closure,
             mojom::BluetoothGattValuePtr value) {
            ASSERT_TRUE(value);
            EXPECT_TRUE(value->value.empty());
            EXPECT_EQ(value->status, mojom::BluetoothGattStatus::GATT_FAILURE);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ArcBluetoothBridgeTest, WriteMissingCharacteristicFailsGracefully) {
  base::RunLoop run_loop;

  // Pass clearly invalid values to guarantee that we won't be able to find a
  // valid GATT characteristic.
  arc_bluetooth_bridge_->WriteGattCharacteristic(
      /*remote_addr=*/mojom::BluetoothAddress::New(),
      /*service_id=*/mojom::BluetoothGattServiceID::New(),
      /*char_id=*/mojom::BluetoothGattID::New(),
      /*value=*/mojom::BluetoothGattValue::New(),
      /*prepare=*/false,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure,
             mojom::BluetoothGattStatus status) {
            EXPECT_EQ(status, mojom::BluetoothGattStatus::GATT_FAILURE);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ArcBluetoothBridgeTest, SetDiscoverabilityAndTimeout) {
  // Setting discoverable without setting the timeout first is not allowed
  arc_bluetooth_bridge_->SetAdapterProperty(
      mojom::BluetoothProperty::NewAdapterScanMode(
          mojom::BluetoothScanMode::CONNECTABLE_DISCOVERABLE));
  ASSERT_FALSE(adapter_->IsDiscoverable());

  // Setting discoverable after setting the timeout is OK
  // Timeout of zero is OK
  arc_bluetooth_bridge_->SetAdapterProperty(
      mojom::BluetoothProperty::NewDiscoveryTimeout(0));
  arc_bluetooth_bridge_->SetAdapterProperty(
      mojom::BluetoothProperty::NewAdapterScanMode(
          mojom::BluetoothScanMode::CONNECTABLE_DISCOVERABLE));
  ASSERT_TRUE(adapter_->IsDiscoverable());
}

// If we are not discovering or scanning, we shouldn't be forwarding
// LEDeviceFound events.
TEST_F(ArcBluetoothBridgeTest, NoLEDeviceFoundIfNotScanning) {
  EXPECT_EQ(0u, fake_bluetooth_instance_->le_device_found_data().size());
  AddTestDevice();
  EXPECT_EQ(0u, fake_bluetooth_instance_->le_device_found_data().size());
}

}  // namespace arc
