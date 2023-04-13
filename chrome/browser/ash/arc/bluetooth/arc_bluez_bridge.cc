// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/bluetooth/arc_bluez_bridge.h"

#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothAdvertisement;
using device::BluetoothDevice;
using device::BluetoothDiscoveryFilter;
using device::BluetoothDiscoverySession;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattConnection;
using device::BluetoothGattDescriptor;
using device::BluetoothGattNotifySession;
using device::BluetoothGattService;
using device::BluetoothLocalGattCharacteristic;
using device::BluetoothLocalGattDescriptor;
using device::BluetoothLocalGattService;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattDescriptor;
using device::BluetoothRemoteGattService;
using device::BluetoothTransport;
using device::BluetoothUUID;

namespace {

// Bluetooth SDP Service Class ID List Attribute identifier
constexpr uint16_t kServiceClassIDListAttributeID = 0x0001;

void OnCreateServiceRecordDone(
    arc::ArcBluetoothBridge::CreateSdpRecordCallback callback,
    uint32_t service_handle) {
  arc::mojom::BluetoothCreateSdpRecordResultPtr result =
      arc::mojom::BluetoothCreateSdpRecordResult::New();
  result->status = arc::mojom::BluetoothStatus::SUCCESS;
  result->service_handle = service_handle;

  std::move(callback).Run(std::move(result));
}

void OnCreateServiceRecordError(
    arc::ArcBluetoothBridge::CreateSdpRecordCallback callback,
    bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code) {
  arc::mojom::BluetoothCreateSdpRecordResultPtr result =
      arc::mojom::BluetoothCreateSdpRecordResult::New();
  if (error_code ==
      bluez::BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY) {
    result->status = arc::mojom::BluetoothStatus::NOT_READY;
  } else {
    result->status = arc::mojom::BluetoothStatus::FAIL;
  }

  std::move(callback).Run(std::move(result));
}

void OnRemoveServiceRecordDone(
    arc::ArcBluetoothBridge::RemoveSdpRecordCallback callback) {
  std::move(callback).Run(arc::mojom::BluetoothStatus::SUCCESS);
}

void OnRemoveServiceRecordError(
    arc::ArcBluetoothBridge::RemoveSdpRecordCallback callback,
    bluez::BluetoothServiceRecordBlueZ::ErrorCode error_code) {
  arc::mojom::BluetoothStatus status;
  if (error_code ==
      bluez::BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY) {
    status = arc::mojom::BluetoothStatus::NOT_READY;
  } else {
    status = arc::mojom::BluetoothStatus::FAIL;
  }

  std::move(callback).Run(status);
}

}  // namespace

namespace arc {

ArcBluezBridge::ArcBluezBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : ArcBluetoothBridge(context, bridge_service) {}

ArcBluezBridge::~ArcBluezBridge() = default;

bluez::BluetoothAdapterBlueZ* ArcBluezBridge::GetAdapter() const {
  return static_cast<bluez::BluetoothAdapterBlueZ*>(bluetooth_adapter_.get());
}

void ArcBluezBridge::HandlePoweredOn() {}

void ArcBluezBridge::GetSdpRecords(mojom::BluetoothAddressPtr remote_addr,
                                   const BluetoothUUID& target_uuid) {
  BluetoothDevice* device =
      GetAdapter()->GetDevice(remote_addr->To<std::string>());
  if (!device) {
    OnGetServiceRecordsError(std::move(remote_addr), target_uuid,
                             bluez::BluetoothServiceRecordBlueZ::ErrorCode::
                                 ERROR_DEVICE_DISCONNECTED);
    return;
  }

  bluez::BluetoothDeviceBlueZ* device_bluez =
      static_cast<bluez::BluetoothDeviceBlueZ*>(device);

  mojom::BluetoothAddressPtr remote_addr_clone = remote_addr.Clone();

  device_bluez->GetServiceRecords(
      base::BindOnce(&ArcBluezBridge::OnGetServiceRecordsFinished,
                     weak_factory_.GetWeakPtr(), std::move(remote_addr),
                     target_uuid),
      base::BindOnce(&ArcBluezBridge::OnGetServiceRecordsError,
                     weak_factory_.GetWeakPtr(), std::move(remote_addr_clone),
                     target_uuid));
}

void ArcBluezBridge::CreateSdpRecord(
    mojom::BluetoothSdpRecordPtr record_mojo,
    arc::ArcBluetoothBridge::CreateSdpRecordCallback callback) {
  auto record = record_mojo.To<bluez::BluetoothServiceRecordBlueZ>();

  // Check if ServiceClassIDList attribute (attribute ID 0x0001) is included
  // after type conversion, since it is mandatory for creating a service record.
  if (!record.IsAttributePresented(kServiceClassIDListAttributeID)) {
    mojom::BluetoothCreateSdpRecordResultPtr result =
        mojom::BluetoothCreateSdpRecordResult::New();
    result->status = mojom::BluetoothStatus::FAIL;
    std::move(callback).Run(std::move(result));
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetAdapter()->CreateServiceRecord(
      record,
      base::BindOnce(&OnCreateServiceRecordDone,
                     std::move(split_callback.first)),
      base::BindOnce(&OnCreateServiceRecordError,
                     std::move(split_callback.second)));
}

void ArcBluezBridge::RemoveSdpRecord(uint32_t service_handle,
                                     RemoveSdpRecordCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetAdapter()->RemoveServiceRecord(
      service_handle,
      base::BindOnce(&OnRemoveServiceRecordDone,
                     std::move(split_callback.first)),
      base::BindOnce(&OnRemoveServiceRecordError,
                     std::move(split_callback.second)));
}

}  // namespace arc
