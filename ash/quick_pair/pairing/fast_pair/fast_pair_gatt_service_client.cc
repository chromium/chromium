// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_gatt_service_client.h"

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/logging.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

namespace {

// We have two UUID possibilities for each characteristic because they changed
// across different Fast Pair versions.
const device::BluetoothUUID kKeyBasedCharacteristicUuidV1("1234");
const device::BluetoothUUID kKeyBasedCharacteristicUuidV2(
    "FE2C1234-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kPasskeyCharacteristicUuidV1("1235");
const device::BluetoothUUID kPasskeyCharacteristicUuidV2(
    "FE2C1235-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kAccountKeyCharacteristicUuidV1("1236");
const device::BluetoothUUID kAccountKeyCharacteristicUuidV2(
    "FE2C1236-8366-4814-8EB0-01DE32100BEA");

}  // namespace

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
  QP_LOG(VERBOSE) << "Starting the GATT connection to device at address:["
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
  key_based_characteristic_ = nullptr;
  passkey_characteristic_ = nullptr;
  bluetooth_gatt_notify_sessions_.clear();
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
        << "GATT discovery complete for service related to device at address:["
        << device_address_ << "].";
    gatt_service_ = service;
    FindGattCharacteristicsAndStartNotifySessions();
  }
}

std::vector<device::BluetoothRemoteGattCharacteristic*>
FastPairGattServiceClient::GetCharacteristicsByUUIDs(
    const device::BluetoothUUID uuidV1,
    const device::BluetoothUUID uuidV2) {
  if (!gatt_service_)
    return {};

  std::vector<device::BluetoothRemoteGattCharacteristic*> characteristics =
      gatt_service_->GetCharacteristicsByUUID(uuidV1);
  characteristics = characteristics.size()
                        ? characteristics
                        : gatt_service_->GetCharacteristicsByUUID(uuidV2);
  return characteristics;
}

void FastPairGattServiceClient::
    FindGattCharacteristicsAndStartNotifySessions() {
  std::vector<device::BluetoothRemoteGattCharacteristic*>
      key_based_characteristics = GetCharacteristicsByUUIDs(
          kKeyBasedCharacteristicUuidV1, kKeyBasedCharacteristicUuidV2);
  if (key_based_characteristics.empty()) {
    NotifyError(PairFailure::kKeyBasedPairingCharacteristicDiscovery);
    return;
  }

  std::vector<device::BluetoothRemoteGattCharacteristic*>
      passkey_characteristics = GetCharacteristicsByUUIDs(
          kPasskeyCharacteristicUuidV1, kPasskeyCharacteristicUuidV2);
  if (passkey_characteristics.empty()) {
    NotifyError(PairFailure::kPasskeyCharacteristicDiscovery);
    return;
  }

  std::vector<device::BluetoothRemoteGattCharacteristic*>
      account_key_characteristics = GetCharacteristicsByUUIDs(
          kAccountKeyCharacteristicUuidV1, kAccountKeyCharacteristicUuidV2);
  if (account_key_characteristics.empty()) {
    NotifyError(PairFailure::kAccountKeyCharacteristicDiscovery);
    return;
  }
  account_key_characteristic_ = account_key_characteristics[0];

  key_based_characteristic_ = key_based_characteristics[0];
  key_based_characteristic_->StartNotifySession(
      base::BindOnce(&FastPairGattServiceClient::OnNotifySession,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClient::OnGattError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kKeyBasedPairingCharacteristicNotifySession));
  passkey_characteristic_ = passkey_characteristics[0];
  passkey_characteristic_->StartNotifySession(
      base::BindOnce(&FastPairGattServiceClient::OnNotifySession,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairGattServiceClient::OnGattError,
                     weak_ptr_factory_.GetWeakPtr(),
                     PairFailure::kPasskeyCharacteristicNotifySession));
}

void FastPairGattServiceClient::OnNotifySession(
    std::unique_ptr<device::BluetoothGattNotifySession> session) {
  bluetooth_gatt_notify_sessions_.push_back(std::move(session));

  // Once expected characteristics are notifying, Run the callback with no
  // error. Here, we are waiting for both the key based characteristics and the
  // pass key characteristics to notify, thus size "2";
  if (bluetooth_gatt_notify_sessions_.size() == 2) {
    QP_LOG(VERBOSE) << "GATT service is ready for device at address:["
                    << device_address_ << "].";
    DCHECK(on_initialized_callback_);
    std::move(on_initialized_callback_).Run(/*failure=*/absl::nullopt);
  }
}

void FastPairGattServiceClient::OnGattError(
    PairFailure failure,
    device::BluetoothGattService::GattErrorCode error) {
  QP_LOG(VERBOSE) << "StartNotifySession failed due to GATT error.";
  NotifyError(failure);
}

}  // namespace quick_pair
}  // namespace ash
