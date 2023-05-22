// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bluetooth_devices_observer.h"

#include "base/functional/bind.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {

BluetoothDevicesObserver::BluetoothDevicesObserver(
    const AdapterOrDeviceChangedCallback& device_changed_callback)
    : adapter_or_device_changed_callback_(device_changed_callback) {
  if (device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    device::BluetoothAdapterFactory::Get()->GetAdapter(
        base::BindOnce(&BluetoothDevicesObserver::InitializeOnAdapterReady,
                       weak_factory_.GetWeakPtr()));
  } else {
    adapter_or_device_changed_callback_.Run(/*device=*/nullptr);
  }
}

BluetoothDevicesObserver::~BluetoothDevicesObserver() {
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

void BluetoothDevicesObserver::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  adapter_or_device_changed_callback_.Run(/*device=*/nullptr);
}

void BluetoothDevicesObserver::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  adapter_or_device_changed_callback_.Run(/*device=*/nullptr);
}

void BluetoothDevicesObserver::DeviceChanged(device::BluetoothAdapter* adapter,
                                             device::BluetoothDevice* device) {
  adapter_or_device_changed_callback_.Run(device);
}

void BluetoothDevicesObserver::InitializeOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = std::move(adapter);
  bluetooth_adapter_->AddObserver(this);
}

bool BluetoothDevicesObserver::IsConnectedBluetoothDevice(
    const ui::InputDevice& input_device) const {
  return GetConnectedBluetoothDevice(input_device) != nullptr;
}

device::BluetoothDevice* BluetoothDevicesObserver::GetConnectedBluetoothDevice(
    const ui::InputDevice& input_device) const {
  if (!bluetooth_adapter_ || !bluetooth_adapter_->IsPresent() ||
      !bluetooth_adapter_->IsInitialized() ||
      !bluetooth_adapter_->IsPowered()) {
    return nullptr;
  }

  // Since there is no map from an InputDevice to a BluetoothDevice. We just
  // comparing their vendor id and product id to guess a match.
  for (auto* device : bluetooth_adapter_->GetDevices()) {
    if (!device->IsConnected()) {
      continue;
    }

    if (device->GetVendorID() == input_device.vendor_id &&
        device->GetProductID() == input_device.product_id) {
      return device;
    }
  }

  return nullptr;
}

}  // namespace ash
