// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"

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
  SetEnabledAndInvokeCallback(powered);
}

void BluetoothEnabledProvider::OnAdapterReceived(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  adapter_observation_.Observe(adapter_.get());

  SetEnabledAndInvokeCallback(adapter_->IsPowered());
}

}  // namespace quick_pair
}  // namespace ash
