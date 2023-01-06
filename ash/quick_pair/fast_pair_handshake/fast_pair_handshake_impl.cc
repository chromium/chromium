// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_impl.h"

#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
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
  RecordHandshakeStep(FastPairHandshakeSteps::kHandshakeStarted, *device_);

  device::BluetoothDevice* bluetooth_device =
      adapter_->GetDevice(device_->ble_address());

  if (!bluetooth_device) {
    QP_LOG(INFO) << __func__
                 << ": Lost device before starting GATT connection.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kPairingDeviceLost);
    return;
  }

  fast_pair_gatt_service_client_ =
      FastPairGattServiceClientImpl::Factory::Create(
          bluetooth_device, adapter_,
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
    RecordHandshakeResult(/*success=*/false);
    RecordHandshakeFailureReason(HandshakeFailureReason::kFailedGattInit);
    return;
  }

  QP_LOG(INFO) << __func__
               << ": Fast Pair GATT service client initialization successful.";
  RecordHandshakeStep(FastPairHandshakeSteps::kGattInitalized, *device_);
  FastPairDataEncryptorImpl::Factory::CreateAsync(
      device_,
      base::BindOnce(&FastPairHandshakeImpl::OnDataEncryptorCreateAsync,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void FastPairHandshakeImpl::OnDataEncryptorCreateAsync(
    base::TimeTicks encryptor_create_start_time,
    std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor) {
  bool success = fast_pair_data_encryptor != nullptr;
  RecordDataEncryptorCreateResult(/*success=*/success);

  if (!fast_pair_data_encryptor) {
    QP_LOG(WARNING) << __func__
                    << ": Fast Pair Data Encryptor failed to be created.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kDataEncryptorRetrieval);
    RecordHandshakeResult(/*success=*/false);
    RecordHandshakeFailureReason(
        HandshakeFailureReason::kFailedCreateEncryptor);
    return;
  }

  fast_pair_data_encryptor_ = std::move(fast_pair_data_encryptor);
  QP_LOG(INFO) << __func__ << ": beginning key-based pairing protocol";
  RecordTotalDataEncryptorCreateTime(base::TimeTicks::Now() -
                                     encryptor_create_start_time);

  bool is_retroactive = device_->protocol() == Protocol::kFastPairRetroactive;

  fast_pair_gatt_service_client_->WriteRequestAsync(
      /*message_type=*/kKeyBasedPairingType,
      /*flags=*/is_retroactive ? kRetroactiveFlags : kInitialOrSubsequentFlags,
      /*provider_address=*/device_->ble_address(),
      /*seekers_address=*/is_retroactive ? adapter_->GetAddress() : "",
      fast_pair_data_encryptor_.get(),
      base::BindOnce(&FastPairHandshakeImpl::OnWriteResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairHandshakeImpl::OnWriteResponse(
    std::vector<uint8_t> response_bytes,
    absl::optional<PairFailure> failure) {
  RecordWriteKeyBasedCharacteristicResult(/*success=*/!failure.has_value());

  if (failure) {
    QP_LOG(WARNING)
        << __func__
        << ": Failed during key-based pairing protocol due to failure: "
        << failure.value();
    RecordWriteKeyBasedCharacteristicPairFailure(failure.value());
    RecordHandshakeResult(/*success=*/false);
    RecordHandshakeFailureReason(HandshakeFailureReason::kFailedWriteResponse);
    std::move(on_complete_callback_).Run(device_, failure.value());
    return;
  }

  QP_LOG(INFO) << __func__ << ": Successfully wrote response.";
  RecordHandshakeStep(FastPairHandshakeSteps::kKeyBasedPairingResponseReceived,
                      *device_);

  fast_pair_data_encryptor_->ParseDecryptedResponse(
      response_bytes,
      base::BindOnce(&FastPairHandshakeImpl::OnParseDecryptedResponse,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void FastPairHandshakeImpl::OnParseDecryptedResponse(
    base::TimeTicks decrypt_start_time,
    const absl::optional<DecryptedResponse>& response) {
  if (!response) {
    QP_LOG(WARNING) << __func__ << ": Missing decrypted response from parse.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kKeybasedPairingResponseDecryptFailure);
    RecordKeyBasedCharacteristicDecryptResult(/*success=*/false);
    RecordHandshakeResult(/*success=*/false);
    RecordHandshakeFailureReason(
        HandshakeFailureReason::kFailedDecryptResponse);
    return;
  }

  if (response->message_type != FastPairMessageType::kKeyBasedPairingResponse) {
    QP_LOG(WARNING) << __func__
                    << ": Incorrect message type from decrypted response.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kIncorrectKeyBasedPairingResponseType);
    RecordKeyBasedCharacteristicDecryptResult(/*success=*/false);
    RecordHandshakeResult(/*success=*/false);
    RecordHandshakeFailureReason(
        HandshakeFailureReason::kFailedIncorrectResponseType);
    return;
  }

  RecordKeyBasedCharacteristicDecryptTime(base::TimeTicks::Now() -
                                          decrypt_start_time);
  RecordKeyBasedCharacteristicDecryptResult(/*success=*/true);
  std::string device_address =
      device::CanonicalizeBluetoothAddress(response->address_bytes);
  device_->set_classic_address(device_address);

  completed_successfully_ = true;
  RecordHandshakeResult(/*success=*/true);
  RecordHandshakeStep(FastPairHandshakeSteps::kHandshakeComplete, *device_);
  std::move(on_complete_callback_).Run(device_, absl::nullopt);
}

}  // namespace quick_pair
}  // namespace ash
