// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_lookup_impl.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace ash {
namespace quick_pair {

constexpr uint8_t kKeyBasedPairingType = 0x00;
constexpr uint8_t kInitialOrSubsequentFlags = 0x00;
constexpr uint8_t kRetroactiveFlags = 0x10;
constexpr uint8_t kSupportBleSpecFlags = 0x08;

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
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": Lost device before starting GATT connection.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kPairingDeviceLost);
    return;
  }

  auto* fast_pair_gatt_service_client =
      FastPairGattServiceClientLookup::GetInstance()->Get(bluetooth_device);

  if (fast_pair_gatt_service_client) {
    if (fast_pair_gatt_service_client->IsConnected()) {
      CD_LOG(VERBOSE, Feature::FP)
          << __func__
          << ": Reusing existing GATT service client for handshake.";
      RecordHandshakeStep(FastPairHandshakeSteps::kGattInitalized, *device_);
      FastPairDataEncryptorImpl::Factory::CreateAsync(
          device_, base::BindOnce(
                       &FastPairHandshakeImpl::OnDataEncryptorCreateAsync,
                       weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
      return;
    } else {
      // If the previous gatt service client did not complete successfully,
      // erase it before attempting to create a new gatt connection for the
      // device.
      FastPairGattServiceClientLookup::GetInstance()->Erase(bluetooth_device);
    }
  }

  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Creating new GATT service client for handshake.";
  FastPairGattServiceClientLookup::GetInstance()->Create(
      adapter_, bluetooth_device,
      base::BindOnce(&FastPairHandshakeImpl::OnGattClientInitializedCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

FastPairHandshakeImpl::~FastPairHandshakeImpl() = default;

void FastPairHandshakeImpl::SetUpHandshake(
    OnFailureCallback on_failure_callback,
    OnCompleteCallbackNew on_success_callback) {}

void FastPairHandshakeImpl::Reset() {}

void FastPairHandshakeImpl::OnGattClientInitializedCallback(
    std::optional<PairFailure> failure) {
  if (failure) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to init gatt client with failure = " << failure.value();
    std::move(on_complete_callback_).Run(device_, failure.value());
    RecordHandshakeResult(/*success=*/false);
    RecordHandshakeFailureReason(HandshakeFailureReason::kFailedGattInit);
    return;
  }

  CD_LOG(INFO, Feature::FP)
      << __func__
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
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Fast Pair Data Encryptor failed to be created.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kDataEncryptorRetrieval);
    RecordHandshakeResult(/*success=*/false);
    RecordHandshakeFailureReason(
        HandshakeFailureReason::kFailedCreateEncryptor);
    return;
  }

  fast_pair_data_encryptor_ = std::move(fast_pair_data_encryptor);
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": beginning key-based pairing protocol";
  RecordTotalDataEncryptorCreateTime(base::TimeTicks::Now() -
                                     encryptor_create_start_time);

  bool is_retroactive = device_->protocol() == Protocol::kFastPairRetroactive;

  auto* device = adapter_->GetDevice(device_->ble_address());
  if (!device) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": device lost when attempting to retrieve GATT service client.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kPairingDeviceLost);
    RecordHandshakeResult(/*success=*/false);
    return;
  }

  auto* fast_pair_gatt_service_client =
      FastPairGattServiceClientLookup::GetInstance()->Get(device);
  CHECK(fast_pair_gatt_service_client);

  uint8_t flags =
      is_retroactive ? kRetroactiveFlags : kInitialOrSubsequentFlags;
  if (ash::features::IsFastPairKeyboardsEnabled() &&
      floss::features::IsFlossEnabled()) {
    flags |= kSupportBleSpecFlags;
  }

  fast_pair_gatt_service_client->WriteRequestAsync(
      /*message_type=*/kKeyBasedPairingType,
      /*flags=*/flags,
      /*provider_address=*/device_->ble_address(),
      /*seekers_address=*/is_retroactive ? adapter_->GetAddress() : "",
      fast_pair_data_encryptor_.get(),
      base::BindOnce(&FastPairHandshakeImpl::OnWriteResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairHandshakeImpl::OnWriteResponse(
    std::vector<uint8_t> response_bytes,
    std::optional<PairFailure> failure) {
  RecordWriteKeyBasedCharacteristicResult(/*success=*/!failure.has_value());

  if (failure) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed during key-based pairing protocol due to failure: "
        << failure.value();
    RecordWriteKeyBasedCharacteristicPairFailure(failure.value());
    RecordHandshakeResult(/*success=*/false);
    RecordHandshakeFailureReason(HandshakeFailureReason::kFailedWriteResponse);
    std::move(on_complete_callback_).Run(device_, failure.value());
    return;
  }

  CD_LOG(INFO, Feature::FP) << __func__ << ": Successfully wrote response.";
  RecordHandshakeStep(FastPairHandshakeSteps::kKeyBasedPairingResponseReceived,
                      *device_);

  fast_pair_data_encryptor_->ParseDecryptedResponse(
      response_bytes,
      base::BindOnce(&FastPairHandshakeImpl::OnParseDecryptedResponse,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void FastPairHandshakeImpl::OnParseDecryptedResponse(
    base::TimeTicks decrypt_start_time,
    const std::optional<DecryptedResponse>& response) {
  if (!response) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Missing decrypted response from parse.";
    std::move(on_complete_callback_)
        .Run(device_, PairFailure::kKeybasedPairingResponseDecryptFailure);
    RecordKeyBasedCharacteristicDecryptResult(/*success=*/false);
    RecordHandshakeResult(/*success=*/false);
    RecordHandshakeFailureReason(
        HandshakeFailureReason::kFailedDecryptResponse);
    return;
  }

  bool is_ext_resp = ash::features::IsFastPairKeyboardsEnabled() &&
                     floss::features::IsFlossEnabled() &&
                     response->message_type ==
                         FastPairMessageType::kKeyBasedPairingExtendedResponse;
  if (response->message_type != FastPairMessageType::kKeyBasedPairingResponse &&
      !is_ext_resp) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Incorrect message type from decrypted response.";
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
  if (ash::features::IsFastPairKeyboardsEnabled() &&
      floss::features::IsFlossEnabled() && response->flags.has_value()) {
    device_->set_key_based_pairing_flags(response->flags.value());
  }

  completed_successfully_ = true;
  RecordHandshakeResult(/*success=*/true);
  RecordHandshakeStep(FastPairHandshakeSteps::kHandshakeComplete, *device_);
  std::move(on_complete_callback_).Run(device_, std::nullopt);
}

}  // namespace quick_pair
}  // namespace ash
