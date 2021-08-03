// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fake_fast_pair_gatt_service_client.h"
#include "ash/quick_pair/common/logging.h"
#include "base/callback_helpers.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

FakeFastPairGattServiceClient::FakeFastPairGattServiceClient(
    device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothAdapter> adapter,
    base::OnceCallback<void(absl::optional<PairFailure>)>
        on_initialized_callback)
    : on_initialized_callback_(std::move(on_initialized_callback)) {}

FakeFastPairGattServiceClient::~FakeFastPairGattServiceClient() = default;

void FakeFastPairGattServiceClient::RunOnGattClientInitializedCallback(
    absl::optional<PairFailure> failure) {
  std::move(on_initialized_callback_).Run(failure);
}

device::BluetoothRemoteGattService*
FakeFastPairGattServiceClient::gatt_service() {
  return nullptr;
}

}  // namespace quick_pair
}  // namespace ash
