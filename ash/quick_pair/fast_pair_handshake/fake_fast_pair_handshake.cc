// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "base/functional/callback.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::quick_pair {

FakeFastPairHandshake::FakeFastPairHandshake(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    OnCompleteCallback on_complete,
    std::unique_ptr<FastPairDataEncryptor> data_encryptor,
    std::unique_ptr<FastPairGattServiceClient> gatt_service_client)
    : FastPairHandshake(std::move(adapter),
                        std::move(device),
                        std::move(on_complete),
                        std::move(data_encryptor),
                        std::move(gatt_service_client)) {}

FakeFastPairHandshake::~FakeFastPairHandshake() = default;

void FakeFastPairHandshake::SetUpHandshake(
    OnFailureCallback on_failure_callback,
    OnCompleteCallbackNew on_success_callback) {
  completed_successfully_ = true;
}

void FakeFastPairHandshake::Reset() {}

void FakeFastPairHandshake::InvokeCallback(std::optional<PairFailure> failure) {
  completed_successfully_ = !failure.has_value();
  std::move(on_complete_callback_).Run(device_, failure);
}

}  // namespace ash::quick_pair
