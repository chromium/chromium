// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_gatt_service_client_impl.h"
#include "base/callback.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

FastPairPairer::FastPairPairer(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
    base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
        pair_failed_callback,
    base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
        account_key_failure_callback,
    base::OnceCallback<void(scoped_refptr<Device>)> pairing_procedure_complete)
    : adapter_(std::move(adapter)),
      device_(std::move(device)),
      paired_callback_(std::move(paired_callback)),
      pair_failed_callback_(std::move(pair_failed_callback)),
      account_key_failure_callback_(std::move(account_key_failure_callback)),
      pairing_procedure_complete_(std::move(pairing_procedure_complete)) {
  fast_pair_gatt_service_client_ =
      FastPairGattServiceClientImpl::Factory::Create(
          adapter_->GetDevice(device_->address), adapter_,
          base::BindRepeating(&FastPairPairer::OnGattClientInitializedCallback,
                              weak_ptr_factory_.GetWeakPtr()));
}

FastPairPairer::~FastPairPairer() = default;

void FastPairPairer::OnGattClientInitializedCallback(
    absl::optional<PairFailure> failure) {
  if (failure) {
    std::move(pair_failed_callback_).Run(device_, failure.value());
    return;
  }

  QP_LOG(VERBOSE) << "Fast Pair GATT service client initialization successful.";
}

}  // namespace quick_pair
}  // namespace ash
