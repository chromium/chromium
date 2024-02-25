// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_gatt_service_client.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "base/functional/callback_helpers.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

FakeFastPairGattServiceClient::FakeFastPairGattServiceClient(
    device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothAdapter> adapter,
    base::OnceCallback<void(std::optional<PairFailure>)>
        on_initialized_callback)
    : on_initialized_callback_(std::move(on_initialized_callback)) {}

FakeFastPairGattServiceClient::~FakeFastPairGattServiceClient() = default;

void FakeFastPairGattServiceClient::RunOnGattClientInitializedCallback(
    std::optional<PairFailure> failure) {
  std::move(on_initialized_callback_).Run(failure);
}

device::BluetoothRemoteGattService*
FakeFastPairGattServiceClient::gatt_service() {
  return nullptr;
}

bool FakeFastPairGattServiceClient::IsConnected() {
  return is_connected_;
}

void FakeFastPairGattServiceClient::SetConnected(bool is_connected) {
  is_connected_ = is_connected;
}

void FakeFastPairGattServiceClient::ReadModelIdAsync(
    base::OnceCallback<void(
        std::optional<device::BluetoothGattService::GattErrorCode> error_code,
        const std::vector<uint8_t>& value)> callback) {
  read_model_id_callback_ = std::move(callback);
}

void FakeFastPairGattServiceClient::RunReadModelIdCallback(
    std::optional<device::BluetoothGattService::GattErrorCode> error_code,
    const std::vector<uint8_t>& value) {
  std::move(read_model_id_callback_).Run(error_code, value);
}

void FakeFastPairGattServiceClient::WriteRequestAsync(
    uint8_t message_type,
    uint8_t flags,
    const std::string& provider_address,
    const std::string& seekers_address,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
        write_response_callback) {
  key_based_write_response_callback_ = std::move(write_response_callback);
}

void FakeFastPairGattServiceClient::RunWriteResponseCallback(
    std::vector<uint8_t> data,
    std::optional<PairFailure> failure) {
  std::move(key_based_write_response_callback_).Run(data, failure);
}

void FakeFastPairGattServiceClient::RunWritePasskeyCallback(
    std::vector<uint8_t> data,
    std::optional<PairFailure> failure) {
  std::move(passkey_write_response_callback_).Run(data, failure);
}

void FakeFastPairGattServiceClient::WritePasskeyAsync(
    uint8_t message_type,
    uint32_t passkey,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<void(std::vector<uint8_t>, std::optional<PairFailure>)>
        write_response_callback) {
  passkey_write_response_callback_ = std::move(write_response_callback);
}

void FakeFastPairGattServiceClient::WriteAccountKey(
    std::array<uint8_t, 16> account_key,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<void(std::optional<ash::quick_pair::AccountKeyFailure>)>
        write_account_key_callback) {
  write_account_key_callback_ = std::move(write_account_key_callback);
}

void FakeFastPairGattServiceClient::RunWriteAccountKeyCallback(
    std::optional<AccountKeyFailure> failure) {
  std::move(write_account_key_callback_).Run(failure);
}

void FakeFastPairGattServiceClient::WritePersonalizedName(
    const std::string& name,
    const std::string& provider_address,
    FastPairDataEncryptor* fast_pair_data_encryptor,
    base::OnceCallback<void(std::optional<PairFailure>)>
        write_additional_data_callback) {
  write_personalized_name_callback_ = std::move(write_additional_data_callback);
}

void FakeFastPairGattServiceClient::RunWritePersonalizedNameCallback(
    std::optional<PairFailure> failure) {
  std::move(write_personalized_name_callback_).Run(failure);
}

}  // namespace quick_pair
}  // namespace ash
