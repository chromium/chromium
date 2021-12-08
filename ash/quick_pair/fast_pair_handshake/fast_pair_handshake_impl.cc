// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_impl.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "base/callback.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace ash {
namespace quick_pair {

constexpr uint8_t kKeyBasedPairingType = 0x00;
constexpr uint8_t kInitialOrSubsequentFlags = 0x00;
constexpr uint8_t kRetroactiveFlags = 0x10;

FastPairHandshakeImpl::FastPairHandshakeImpl(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    OnCompleteCallback on_complete)
    : FastPairHandshake(std::move(adapter),
                        std::move(device),
                        std::move(on_complete),
                        nullptr,
                        nullptr) {
  fast_pair_gatt_service_client_ =
      FastPairGattServiceClientImpl::Factory::Create(
          adapter_->GetDevice(device_->ble_address), adapter_,
          base::BindRepeating(
              &FastPairHandshakeImpl::OnGattClientInitializedCallback,
              weak_ptr_factory_.GetWeakPtr()));
}

FastPairHandshakeImpl::~FastPairHandshakeImpl() = default;

void FastPairHandshakeImpl::OnGattClientInitializedCallback(
    absl::optional<PairFailure> failure) {
  if (failure) {
    QP_LOG(WARNING) << __func__
                    << ": Failed to init gatt client with failure = "
                    << failure.value();
    std::move(on_complete_callback_).Run(device_, failure.value());
    return;
  }

  FastPairDataEncryptorImpl::Factory::CreateAsync(
      device_,
      base::BindOnce(&FastPairHandshakeImpl::OnDataEncryptorCreateAsync,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairHandshakeImpl::OnDataEncryptorCreateAsync(
    std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor) {
  if (!fast_pair_data_encryptor) {
    QP_LOG(WARNING) << __func__
                    << ": Fast Pair Data Encryptor failed to be created.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kDataEncryptorRetrieval);
    return;
  }

  fast_pair_data_encryptor_ = std::move(fast_pair_data_encryptor);
  QP_LOG(INFO) << __func__
               << ": Fast Pair GATT service client initialization successful.";

  bool is_retroactive = device_->protocol == Protocol::kFastPairRetroactive;

  fast_pair_gatt_service_client_->WriteRequestAsync(
      /*message_type=*/kKeyBasedPairingType,
      /*flags=*/is_retroactive ? kRetroactiveFlags : kInitialOrSubsequentFlags,
      /*provider_address=*/device_->ble_address,
      /*seekers_address=*/is_retroactive ? adapter_->GetAddress() : "",
      fast_pair_data_encryptor_.get(),
      base::BindOnce(&FastPairHandshakeImpl::OnWriteResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairHandshakeImpl::OnWriteResponse(
    std::vector<uint8_t> response_bytes,
    absl::optional<PairFailure> failure) {
  if (failure) {
    QP_LOG(WARNING) << __func__
                    << ": Failed to write request: " << failure.value();
    std::move(on_complete_callback_).Run(device_, failure.value());
    return;
  }

  QP_LOG(INFO) << __func__ << ": Successfully wrote response.";

  fast_pair_data_encryptor_->ParseDecryptedResponse(
      response_bytes,
      base::BindOnce(&FastPairHandshakeImpl::OnParseDecryptedResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairHandshakeImpl::OnParseDecryptedResponse(
    const absl::optional<DecryptedResponse>& response) {
  if (!response) {
    QP_LOG(WARNING) << __func__ << ": Missing decrypted response from parse.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kKeybasedPairingResponseDecryptFailure);
    return;
  }

  if (response->message_type != FastPairMessageType::kKeyBasedPairingResponse) {
    QP_LOG(WARNING) << __func__
                    << ": Incorrect message type from decrypted response.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kIncorrectKeyBasedPairingResponseType);
    return;
  }

  std::string device_address =
      device::CanonicalizeBluetoothAddress(response->address_bytes);
  device_->set_classic_address(device_address);

  completed_successfully_ = true;
  std::move(on_complete_callback_).Run(device_, absl::nullopt);
}

}  // namespace quick_pair
}  // namespace ash
