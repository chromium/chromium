// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/gatt_battery_percentage_fetcher.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

using device::BluetoothDevice;
using device::BluetoothGattService;

namespace ash {

namespace {

// UUIDs for the standardized Battery Service and Characteristic, defined by the
// Bluetooth GATT Specification.
// https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Services/org.bluetooth.service.battery_service.xml
constexpr char kBatteryServiceUUID[] = "180F";
constexpr char kBatteryLevelUUID[] = "2A19";

GattBatteryPercentageFetcher::Factory* g_test_factory_instance_ = nullptr;

const device::BluetoothUUID& GetBatteryServiceUUID() {
  static const device::BluetoothUUID battery_service_uuid(kBatteryServiceUUID);
  return battery_service_uuid;
}
const device::BluetoothUUID& GetBatteryLevelUUID() {
  static const device::BluetoothUUID battery_level_uuid(kBatteryLevelUUID);
  return battery_level_uuid;
}

const char* BluetoothDeviceErrorCodeToString(
    BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case BluetoothDevice::ERROR_AUTH_CANCELED:
      return "ERROR_AUTH_CANCELED";
    case BluetoothDevice::ERROR_AUTH_FAILED:
      return "ERROR_AUTH_FAILED";
    case BluetoothDevice::ERROR_AUTH_REJECTED:
      return "ERROR_AUTH_REJECTED";
    case BluetoothDevice::ERROR_AUTH_TIMEOUT:
      return "ERROR_AUTH_TIMEOUT";
    case BluetoothDevice::ERROR_FAILED:
      return "ERROR_FAILED";
    case BluetoothDevice::ERROR_INPROGRESS:
      return "ERROR_INPROGRESS";
    case BluetoothDevice::ERROR_UNKNOWN:
      return "ERROR_UNKNOWN";
    case BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      return "ERROR_UNSUPPORTED_DEVICE";
    case BluetoothDevice::NUM_CONNECT_ERROR_CODES:
      NOTREACHED();
      return "";
  }
}

const char* GattErrorCodeToString(
    BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case BluetoothGattService::GATT_ERROR_UNKNOWN:
      return "GATT_ERROR_UNKNOWN";
    case BluetoothGattService::GATT_ERROR_FAILED:
      return "GATT_ERROR_FAILED";
    case BluetoothGattService::GATT_ERROR_IN_PROGRESS:
      return "GATT_ERROR_IN_PROGRESS";
    case BluetoothGattService::GATT_ERROR_INVALID_LENGTH:
      return "GATT_ERROR_INVALID_LENGTH";
    case BluetoothGattService::GATT_ERROR_NOT_PERMITTED:
      return "GATT_ERROR_NOT_PERMITTED";
    case BluetoothGattService::GATT_ERROR_NOT_AUTHORIZED:
      return "GATT_ERROR_NOT_AUTHORIZED";
    case BluetoothGattService::GATT_ERROR_NOT_PAIRED:
      return "GATT_ERROR_NOT_PAIRED";
    case BluetoothGattService::GATT_ERROR_NOT_SUPPORTED:
      return "GATT_ERROR_NOT_SUPPORTED";
  }
}

device::BluetoothRemoteGattService* GetGattBatteryService(
    BluetoothDevice* device) {
  for (device::BluetoothRemoteGattService* service :
       device->GetGattServices()) {
    if (service->GetUUID() == GetBatteryServiceUUID())
      return service;
  }
  return nullptr;
}

}  // namespace

// static
std::unique_ptr<GattBatteryPercentageFetcher>
GattBatteryPercentageFetcher::Factory::NewInstance(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address,
    BatteryPercentageCallback callback) {
  if (g_test_factory_instance_) {
    return g_test_factory_instance_->BuildInstance(adapter, device_address,
                                                   std::move(callback));
  }
  auto instance = base::WrapUnique(
      new GattBatteryPercentageFetcher(device_address, std::move(callback)));
  instance->SetAdapterAndStartFetching(adapter);
  return instance;
}

// static
void GattBatteryPercentageFetcher::Factory::SetFactoryForTesting(
    Factory* factory) {
  g_test_factory_instance_ = factory;
}

GattBatteryPercentageFetcher::GattBatteryPercentageFetcher(
    const std::string& device_address,
    BatteryPercentageCallback callback)
    : device_address_(device_address), callback_(std::move(callback)) {}

GattBatteryPercentageFetcher::~GattBatteryPercentageFetcher() {
  if (adapter_)
    adapter_->RemoveObserver(this);
}

void GattBatteryPercentageFetcher::SetAdapterAndStartFetching(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(adapter);
  adapter_ = adapter;
  adapter_->AddObserver(this);

  // Create Gatt Connection.
  BluetoothDevice* device = adapter_->GetDevice(device_address());
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "GattBatteryPercentageFetcher error for device: " << device_address()
        << ". Unable to get device from adapter on CreateGattConnection.";
    InvokeCallbackWithFailedFetch();
    return;
  }
  DCHECK(!connection_);
  device->CreateGattConnection(
      base::Bind(&GattBatteryPercentageFetcher::OnGattConnected,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&GattBatteryPercentageFetcher::OnGattConnectError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void GattBatteryPercentageFetcher::OnGattConnected(
    std::unique_ptr<device::BluetoothGattConnection> connection) {
  DCHECK_EQ(connection->GetDeviceAddress(), this->device_address());
  BluetoothDevice* device = adapter_->GetDevice(device_address());
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "GattBatteryPercentageFetcher error for device: " << device_address()
        << ". Unable to get device from adapter on OnGattConnected.";
    InvokeCallbackWithFailedFetch();
    return;
  }
  connection_ = std::move(connection);

  // If Gatt Services Discovery is not complete yet, wait until
  // GattServicesDiscovered() is called to continue fetching the battery level.
  if (device->IsGattServicesDiscoveryComplete())
    AttemptToReadBatteryCharacteristic();
}

void GattBatteryPercentageFetcher::OnGattConnectError(
    BluetoothDevice::ConnectErrorCode error_code) {
  BLUETOOTH_LOG(ERROR) << "GattBatteryPercentageFetcher error for device: "
                       << device_address() << ". OnGattConnectError"
                       << BluetoothDeviceErrorCodeToString(error_code);
  InvokeCallbackWithFailedFetch();
}

void GattBatteryPercentageFetcher::GattServicesDiscovered(
    device::BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  if (device->GetAddress() == device_address())
    AttemptToReadBatteryCharacteristic();
}

void GattBatteryPercentageFetcher::AttemptToReadBatteryCharacteristic() {
  // This function should only be called once with an active GATT connection.
  if (!connection_ || attempted_to_read_the_battery_characteristic_)
    return;

  attempted_to_read_the_battery_characteristic_ = true;
  BluetoothDevice* device = adapter_->GetDevice(device_address());
  if (!device) {
    BLUETOOTH_LOG(ERROR) << "GattBatteryPercentageFetcher error for device: "
                         << device_address()
                         << ". Unable to get device from adapter on "
                            "AttemptToReadBatteryCharacteristic.";
    InvokeCallbackWithFailedFetch();
    return;
  }

  device::BluetoothRemoteGattService* service = GetGattBatteryService(device);
  if (!service) {
    BLUETOOTH_LOG(ERROR) << "GattBatteryPercentageFetcher error for device: "
                         << device_address() << ". No battery service.";
    InvokeCallbackWithFailedFetch();
    return;
  }

  std::vector<device::BluetoothRemoteGattCharacteristic*> characteristics =
      service->GetCharacteristicsByUUID(GetBatteryLevelUUID());

  // If no battery characteristic exists, the value cannot be retrieved.
  if (characteristics.empty()) {
    BLUETOOTH_LOG(ERROR) << "GattBatteryPercentageFetcher error for device: "
                         << device_address()
                         << ". Bad format for battery level characteristic.";
    InvokeCallbackWithFailedFetch();
    return;
  }

  // Only one characteristic is expected to exist according to the GATT Battery
  // Service standard. If multiple characteristics are present, arbitrarily
  // choose the first one.
  characteristics[0]->ReadRemoteCharacteristic(
      base::Bind(&GattBatteryPercentageFetcher::OnReadBatteryLevel,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&GattBatteryPercentageFetcher::OnReadBatteryLevelError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void GattBatteryPercentageFetcher::OnReadBatteryLevel(
    const std::vector<uint8_t>& value) {
  if (value.size() != 1 || value[0] > 100) {
    BLUETOOTH_LOG(ERROR) << "GattBatteryPercentageFetcher error for device: "
                         << device_address()
                         << ". Wrong format for battery level.";
    InvokeCallbackWithFailedFetch();
    return;
  }
  InvokeCallbackWithSuccessfulFetch(value[0]);
}

void GattBatteryPercentageFetcher::OnReadBatteryLevelError(
    BluetoothGattService::GattErrorCode error_code) {
  BLUETOOTH_LOG(ERROR) << "GattBatteryPercentageFetcher error for device: "
                       << device_address() << ". OnReadBatteryLevelError - "
                       << GattErrorCodeToString(error_code);
  InvokeCallbackWithFailedFetch();
}

void GattBatteryPercentageFetcher::InvokeCallbackWithSuccessfulFetch(
    uint8_t battery_percentage) {
  connection_.reset();
  DCHECK(callback_);
  std::move(callback_).Run(battery_percentage);
}

void GattBatteryPercentageFetcher::InvokeCallbackWithFailedFetch() {
  connection_.reset();
  DCHECK(callback_);
  std::move(callback_).Run(base::nullopt);
}

}  // namespace ash
