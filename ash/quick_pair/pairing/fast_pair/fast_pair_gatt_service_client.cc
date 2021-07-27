// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_gatt_service_client.h"

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/logging.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

namespace ash {
namespace quick_pair {

FastPairGattServiceClient::FastPairGattServiceClient(
    device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothAdapter> adapter,
    base::OnceCallback<void(absl::optional<PairFailure>)>
        on_initialized_callback)
    : device_address_(device->GetAddress()),
      on_initialized_callback_(std::move(on_initialized_callback)),
      adapter_(std::move(adapter)) {
  adapter_observation_.Observe(adapter_.get());
  QP_LOG(VERBOSE) << "Starting the Gatt connection to device at address:["
                  << device_address_ << "].";
  device->CreateGattConnection(
      base::BindOnce(&FastPairGattServiceClient::OnGattConnection,
                     weak_ptr_factory_.GetWeakPtr()),
      kFastPairBluetoothUuid);
}

FastPairGattServiceClient::~FastPairGattServiceClient() = default;

void FastPairGattServiceClient::OnGattConnection(
    std::unique_ptr<device::BluetoothGattConnection> gatt_connection,
    absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code) {
    QP_LOG(WARNING) << "Error creating GATT connection to device at address:["
                    << device_address_ << "].";
    NotifyError(PairFailure::kCreateGattConnection);
  } else {
    QP_LOG(VERBOSE)
        << "Successful creation of GATT connection to device at address:["
        << device_address_ << "].";
    gatt_connection_ = std::move(gatt_connection);
  }
}

void FastPairGattServiceClient::NotifyError(PairFailure failure) {
  adapter_.reset();
  adapter_observation_.Reset();
  gatt_connection_.reset();
  gatt_service_ = nullptr;
  DCHECK(on_initialized_callback_);
  std::move(on_initialized_callback_).Run(failure);
}

void FastPairGattServiceClient::GattDiscoveryCompleteForService(
    device::BluetoothAdapter* adapter,
    device::BluetoothRemoteGattService* service) {
  // Verify that the discovered service and device are the ones we care about.
  if (service->GetUUID() == kFastPairBluetoothUuid &&
      service->GetDevice()->GetAddress() == device_address_) {
    QP_LOG(VERBOSE)
        << "Gatt discovery complete for service related to device at address:["
        << device_address_ << "].";
    gatt_service_ = service;
  }
}

}  // namespace quick_pair
}  // namespace ash
