// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bluetooth_devices_observer.h"

#include "base/bind.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {

BluetoothDevicesObserver::BluetoothDevicesObserver(
    const DeviceChangedCallback& device_changed_callback)
    : device_changed_callback_(device_changed_callback), weak_factory_(this) {
  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothDevicesObserver::InitializeOnAdapterReady,
                 weak_factory_.GetWeakPtr()));
}

BluetoothDevicesObserver::~BluetoothDevicesObserver() {
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

void BluetoothDevicesObserver::DeviceChanged(device::BluetoothAdapter* adapter,
                                             device::BluetoothDevice* device) {
  device_changed_callback_.Run(device);
}

void BluetoothDevicesObserver::InitializeOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = std::move(adapter);
  bluetooth_adapter_->AddObserver(this);
}

bool BluetoothDevicesObserver::IsConnectedBluetoothDevice(
    const ui::InputDevice& input_device) const {
  if (!bluetooth_adapter_ || !bluetooth_adapter_->IsPowered())
    return false;

  // Since there is no map from an InputDevice to a BluetoothDevice. We just
  // comparing their vendor id and product id to guess a match.
  for (auto* device : bluetooth_adapter_->GetDevices()) {
    if (!device->IsConnected())
      continue;

    if (device->GetVendorID() == input_device.vendor_id &&
        device->GetProductID() == input_device.product_id) {
      return true;
    }
  }

  return false;
}

}  // namespace ash
