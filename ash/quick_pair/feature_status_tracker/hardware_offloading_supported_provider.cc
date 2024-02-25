// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/hardware_offloading_supported_provider.h"

#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "hardware_offloading_supported_provider.h"

namespace ash {
namespace quick_pair {

HardwareOffloadingSupportedProvider::HardwareOffloadingSupportedProvider() {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&HardwareOffloadingSupportedProvider::OnAdapterReceived,
                     weak_factory_.GetWeakPtr()));
}

HardwareOffloadingSupportedProvider::~HardwareOffloadingSupportedProvider() =
    default;

void HardwareOffloadingSupportedProvider::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  SetEnabled();
}

void HardwareOffloadingSupportedProvider::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  SetEnabled();
}

void HardwareOffloadingSupportedProvider::
    LowEnergyScanSessionHardwareOffloadingStatusChanged(
        device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
            status) {
  SetEnabled();
}

void HardwareOffloadingSupportedProvider::OnAdapterReceived(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  if (adapter_) {
    adapter_observation_.Observe(adapter_.get());
  }
  SetEnabled();
}

bool HardwareOffloadingSupportedProvider::IsEnabled() {
  if (!adapter_) {
    return false;
  }

  // An adapter must be present in the physical system in order for it to scan.
  if (!adapter_->IsPresent()) {
    return false;
  }

  // The adapter must be powered in order for it to scan.
  if (!adapter_->IsPowered()) {
    return false;
  }

  device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
      offloading_status =
          adapter_->GetLowEnergyScanSessionHardwareOffloadingStatus();
  return offloading_status ==
         device::BluetoothAdapter::
             LowEnergyScanSessionHardwareOffloadingStatus::kSupported;
}

void HardwareOffloadingSupportedProvider::SetEnabled() {
  SetEnabledAndInvokeCallback(IsEnabled());
}

}  // namespace quick_pair
}  // namespace ash
