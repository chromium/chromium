// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {
namespace quick_pair {

BluetoothEnabledProvider::BluetoothEnabledProvider() {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&BluetoothEnabledProvider::OnAdapterReceived,
                     weak_factory_.GetWeakPtr()));
}

BluetoothEnabledProvider::~BluetoothEnabledProvider() = default;

void BluetoothEnabledProvider::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  Update();
}

void BluetoothEnabledProvider::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  Update();
}

void BluetoothEnabledProvider::OnAdapterReceived(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  adapter_observation_.Observe(adapter_.get());

  Update();
}

void BluetoothEnabledProvider::Update() {
  if (!HasHardwareSupport()) {
    SetEnabledAndInvokeCallback(/*is_enabled=*/false);
    return;
  }

  SetEnabledAndInvokeCallback(adapter_->IsPowered());
}

bool BluetoothEnabledProvider::HasHardwareSupport() {
  if (!adapter_ || !adapter_->IsPresent())
    return false;

  if (features::IsFastPairSoftwareScanningEnabled())
    return true;

  return adapter_->GetLowEnergyScanSessionHardwareOffloadingStatus() ==
         device::BluetoothAdapter::
             LowEnergyScanSessionHardwareOffloadingStatus::kSupported;
}

}  // namespace quick_pair
}  // namespace ash
