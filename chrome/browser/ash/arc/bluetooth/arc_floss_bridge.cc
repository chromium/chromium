// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/bluetooth/arc_floss_bridge.h"

#include "base/logging.h"

#include "ash/components/arc/bluetooth/bluetooth_type_converters.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"

using device::BluetoothUUID;
using floss::BluetoothDeviceFloss;

namespace arc {

ArcFlossBridge::ArcFlossBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : ArcBluetoothBridge(context, bridge_service) {}

ArcFlossBridge::~ArcFlossBridge() = default;

floss::BluetoothAdapterFloss* ArcFlossBridge::GetAdapter() const {
  return static_cast<floss::BluetoothAdapterFloss*>(bluetooth_adapter_.get());
}

void ArcFlossBridge::GetSdpRecords(mojom::BluetoothAddressPtr remote_addr,
                                   const BluetoothUUID& target_uuid) {
  NOTIMPLEMENTED();
}

void ArcFlossBridge::CreateSdpRecord(mojom::BluetoothSdpRecordPtr record_mojo,
                                     CreateSdpRecordCallback callback) {
  auto result = mojom::BluetoothCreateSdpRecordResult::New();
  result->status = mojom::BluetoothStatus::FAIL;
  std::move(callback).Run(std::move(result));

  NOTIMPLEMENTED();
}

void ArcFlossBridge::RemoveSdpRecord(uint32_t service_handle,
                                     RemoveSdpRecordCallback callback) {
  std::move(callback).Run(mojom::BluetoothStatus::FAIL);

  NOTIMPLEMENTED();
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

}  // namespace arc
