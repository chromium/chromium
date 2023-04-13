// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>

#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "base/functional/callback_helpers.h"
#include "base/guid.h"
#include "chrome/browser/ash/arc/bluetooth/arc_floss_bridge.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_socket_manager.h"

#include "base/logging.h"

#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_sdp_types.h"

using device::BluetoothUUID;
using floss::BluetoothDeviceFloss;

namespace arc {

ArcFlossBridge::ArcFlossBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : ArcBluetoothBridge(context, bridge_service) {}

ArcFlossBridge::~ArcFlossBridge() {
  if (GetAdapter() && GetAdapter()->IsPowered() &&
      floss::FlossDBusManager::Get()->GetAdapterClient()->HasObserver(this)) {
    floss::FlossDBusManager::Get()->GetAdapterClient()->RemoveObserver(this);
  }
}

floss::BluetoothAdapterFloss* ArcFlossBridge::GetAdapter() const {
  return static_cast<floss::BluetoothAdapterFloss*>(bluetooth_adapter_.get());
}

void ArcFlossBridge::HandlePoweredOn() {
  floss::FlossDBusManager::Get()->GetAdapterClient()->AddObserver(this);
}

void ArcFlossBridge::OnSdpSearchResult(mojom::BluetoothAddressPtr remote_addr,
                                       const device::BluetoothUUID& target_uuid,
                                       floss::DBusResult<bool> result) {
  if (!result.has_value()) {
    OnGetServiceRecordsError(
        std::move(remote_addr), target_uuid,
        bluez::BluetoothServiceRecordBlueZ::ErrorCode::UNKNOWN);
    return;
  }

  if (!*result) {
    OnGetServiceRecordsError(
        std::move(remote_addr), target_uuid,
        bluez::BluetoothServiceRecordBlueZ::ErrorCode::UNKNOWN);
    return;
  }
}

void ArcFlossBridge::GetSdpRecords(mojom::BluetoothAddressPtr remote_addr,
                                   const BluetoothUUID& target_uuid) {
  if (!AdapterReadyAndRegistered()) {
    OnGetServiceRecordsError(
        std::move(remote_addr), target_uuid,
        bluez::BluetoothServiceRecordBlueZ::ErrorCode::ERROR_ADAPTER_NOT_READY);
    return;
  }

  const floss::FlossDeviceId remote_device = floss::FlossDeviceId(
      {.address = remote_addr->To<std::string>(), .name = ""});
  floss::ResponseCallback<bool> response_callback = base::BindOnce(
      &ArcFlossBridge::OnSdpSearchResult, weak_factory_.GetWeakPtr(),
      std::move(remote_addr), target_uuid);

  floss::FlossDBusManager::Get()->GetAdapterClient()->SdpSearch(
      std::move(response_callback), remote_device, target_uuid);
}

void ArcFlossBridge::CreateSdpRecordComplete(device::BluetoothUUID uuid,
                                             floss::DBusResult<bool> result) {
  if (!result.has_value()) {
    arc::mojom::BluetoothCreateSdpRecordResultPtr callback_result =
        arc::mojom::BluetoothCreateSdpRecordResult::New();
    callback_result->status = mojom::BluetoothStatus::FAIL;
    CompleteCreateSdpRecord(uuid, std::move(callback_result));
    return;
  }

  if (!*result) {
    arc::mojom::BluetoothCreateSdpRecordResultPtr callback_result =
        arc::mojom::BluetoothCreateSdpRecordResult::New();
    callback_result->status = mojom::BluetoothStatus::FAIL;
    CompleteCreateSdpRecord(uuid, std::move(callback_result));
    return;
  }
}

void ArcFlossBridge::CreateSdpRecord(mojom::BluetoothSdpRecordPtr record_mojo,
                                     CreateSdpRecordCallback callback) {
  if (!AdapterReadyAndRegistered()) {
    arc::mojom::BluetoothCreateSdpRecordResultPtr result =
        arc::mojom::BluetoothCreateSdpRecordResult::New();
    result->status = mojom::BluetoothStatus::NOT_READY;
    std::move(callback).Run(std::move(result));
    return;
  }
  const floss::BtSdpRecord sdp_record =
      mojo::TypeConverter<floss::BtSdpRecord,
                          bluez::BluetoothServiceRecordBlueZ>::
          Convert(mojo::TypeConverter<
                  bluez::BluetoothServiceRecordBlueZ,
                  mojom::BluetoothSdpRecordPtr>::Convert(record_mojo));
  const absl::optional<device::BluetoothUUID> uuid =
      floss::GetUUIDFromSdpRecord(sdp_record);
  if (!uuid.has_value()) {
    arc::mojom::BluetoothCreateSdpRecordResultPtr result =
        arc::mojom::BluetoothCreateSdpRecordResult::New();
    result->status = mojom::BluetoothStatus::PARM_INVALID;
    std::move(callback).Run(std::move(result));
    return;
  }
  create_sdp_record_callbacks_.insert_or_assign(*uuid, std::move(callback));

  floss::ResponseCallback<bool> response_callback =
      base::BindOnce(&ArcFlossBridge::CreateSdpRecordComplete,
                     weak_factory_.GetWeakPtr(), *uuid);
  floss::FlossDBusManager::Get()->GetAdapterClient()->CreateSdpRecord(
      std::move(response_callback), sdp_record);
}

void ArcFlossBridge::RemoveSdpRecordComplete(RemoveSdpRecordCallback callback,
                                             floss::DBusResult<bool> result) {
  if (!result.has_value()) {
    std::move(callback).Run(arc::mojom::BluetoothStatus::FAIL);
    return;
  }

  if (!*result) {
    std::move(callback).Run(arc::mojom::BluetoothStatus::FAIL);
    return;
  }

  std::move(callback).Run(arc::mojom::BluetoothStatus::SUCCESS);
}

void ArcFlossBridge::RemoveSdpRecord(uint32_t service_handle,
                                     RemoveSdpRecordCallback callback) {
  if (!AdapterReadyAndRegistered()) {
    std::move(callback).Run(arc::mojom::BluetoothStatus::NOT_READY);
    return;
  }

  floss::ResponseCallback<bool> response_callback =
      base::BindOnce(&ArcFlossBridge::RemoveSdpRecordComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  floss::FlossDBusManager::Get()->GetAdapterClient()->RemoveSdpRecord(
      std::move(response_callback), service_handle);
}

void ArcFlossBridge::CloseBluetoothListeningSocket(
    BluetoothListeningSocket* ptr) {}

void ArcFlossBridge::CloseBluetoothConnectingSocket(
    BluetoothConnectingSocket* ptr) {}

void ArcFlossBridge::SdpSearchComplete(
    const floss::FlossDeviceId device,
    const device::BluetoothUUID uuid,
    const std::vector<floss::BtSdpRecord> records) {
  mojom::BluetoothAddressPtr address =
      mojom::BluetoothAddress::From(device.address);
  std::vector<bluez::BluetoothServiceRecordBlueZ> records_bluez;
  for (auto record : records) {
    records_bluez.push_back(
        mojo::TypeConverter<bluez::BluetoothServiceRecordBlueZ,
                            floss::BtSdpRecord>::Convert(record));
  }
  OnGetServiceRecordsFinished(std::move(address), uuid, records_bluez);
}

void ArcFlossBridge::SdpRecordCreated(const floss::BtSdpRecord record,
                                      const int32_t handle) {
  const absl::optional<device::BluetoothUUID> uuid =
      floss::GetUUIDFromSdpRecord(record);
  if (!uuid.has_value()) {
    return;
  }
  arc::mojom::BluetoothCreateSdpRecordResultPtr callback_result =
      arc::mojom::BluetoothCreateSdpRecordResult::New();
  callback_result->status = mojom::BluetoothStatus::SUCCESS;
  callback_result->service_handle = handle;
  CompleteCreateSdpRecord(*uuid, std::move(callback_result));
}

void ArcFlossBridge::SendCachedDevices() const {
  auto* bluetooth_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->bluetooth(), OnDevicePropertiesChanged);
  if (!bluetooth_instance) {
    return;
  }

  for (const auto* device : bluetooth_adapter_->GetDevices()) {
    const BluetoothDeviceFloss* floss_device =
        static_cast<const BluetoothDeviceFloss*>(device);
    if (!floss_device->HasReadProperties()) {
      VLOG(1) << "Skipping device that hasn't read properties: "
              << floss_device->GetAddress();
      continue;
    }

    // Since a cached device may not be a currently available device, we use
    // OnDevicePropertiesChanged() instead of OnDeviceFound() to avoid trigger
    // the logic of device found in Android.
    bluetooth_instance->OnDevicePropertiesChanged(
        mojom::BluetoothAddress::From(device->GetAddress()),
        GetDeviceProperties(mojom::BluetoothPropertyType::ALL, device));
  }
}
void ArcFlossBridge::CreateBluetoothListenSocket(
    mojom::BluetoothSocketType type,
    mojom::BluetoothSocketFlagsPtr flags,
    int port,
    ArcFlossBridge::BluetoothSocketListenCallback callback) {
  NOTIMPLEMENTED();
}

void ArcFlossBridge::CreateBluetoothConnectSocket(
    mojom::BluetoothSocketType type,
    mojom::BluetoothSocketFlagsPtr flags,
    mojom::BluetoothAddressPtr addr,
    int port,
    ArcFlossBridge::BluetoothSocketConnectCallback callback) {
  NOTIMPLEMENTED();
}

void ArcFlossBridge::CompleteCreateSdpRecord(
    device::BluetoothUUID uuid,
    arc::mojom::BluetoothCreateSdpRecordResultPtr result) {
  if (!base::Contains(create_sdp_record_callbacks_, uuid)) {
    return;
  }

  CreateSdpRecordCallback callback =
      std::move(create_sdp_record_callbacks_[uuid]);
  create_sdp_record_callbacks_.erase(uuid);
  std::move(callback).Run(std::move(result));
}

bool ArcFlossBridge::AdapterReadyAndRegistered() {
  if (!GetAdapter() || !GetAdapter()->IsPresent()) {
    return false;
  }

  if (!floss::FlossDBusManager::Get()->GetAdapterClient()->HasObserver(this)) {
    floss::FlossDBusManager::Get()->GetAdapterClient()->AddObserver(this);
  }

  return true;
}

}  // namespace arc
