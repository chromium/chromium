// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/crypto_utility.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "crypto/random.h"
#include "crypto/unexportable_key.h"

namespace enterprise_connectors {

namespace {

constexpr VAType kVAType = VAType::DEFAULT_VA;

// Size of nonce for challenge response.
const size_t kChallengResponseNonceBytesSize = 32;

// Verifies that the `signed_challenge_data` comes from Verified Access.
bool ChallengeComesFromVerifiedAccess(
    const SignedData& signed_challenge_data,
    const std::string& va_public_key_modulus_hex) {
  // Verify challenge signature.
  return CryptoUtility::VerifySignatureUsingHexKey(
      va_public_key_modulus_hex, signed_challenge_data.data(),
      signed_challenge_data.signature());
}

// The KeyInfo message encrypted using a public encryption key, with
// the following parameters:
//   Key encryption: RSA-OAEP with no custom parameters.
//   Data encryption: 256-bit key, AES-CBC with PKCS5 padding.
//   MAC: HMAC-SHA-512 using the AES key.
absl::optional<std::string> CreateChallengeResponseString(
    const std::string& serialized_key_info,
    const SignedData& signed_challenge_data,
    const std::string& wrapping_key_modulus_hex,
    const std::string& wrapping_key_id) {
  ChallengeResponse response_pb;
  *response_pb.mutable_challenge() = signed_challenge_data;

  crypto::RandBytes(base::WriteInto(response_pb.mutable_nonce(),
                                    kChallengResponseNonceBytesSize + 1),
                    kChallengResponseNonceBytesSize);

  std::string key;
  if (!CryptoUtility::EncryptWithSeed(
          serialized_key_info, response_pb.mutable_encrypted_key_info(), key)) {
    return absl::nullopt;
  }

  bssl::UniquePtr<RSA> rsa(CryptoUtility::GetRSA(wrapping_key_modulus_hex));
  if (!rsa) {
    return absl::nullopt;
  }

  if (!CryptoUtility::WrapKeyOAEP(key, rsa.get(), wrapping_key_id,
                                  response_pb.mutable_encrypted_key_info())) {
    return absl::nullopt;
  }

  // Convert the challenge response proto to a string before returning it.
  std::string serialized_response;
  if (!response_pb.SerializeToString(&serialized_response)) {
    return absl::nullopt;
  }
  return serialized_response;
}

}  // namespace

DesktopAttestationService::DesktopAttestationService(
    DeviceTrustKeyManager* key_manager)
    : key_manager_(key_manager),
      background_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK(key_manager_);
}

DesktopAttestationService::~DesktopAttestationService() = default;

// Goes through the following steps in order:
// - Export public key,
// - Validate challenge comes from VA,
// - Generated challenge response,
// - Sign response,
// - Encode encrypted data,
// - Reply to callback.
void DesktopAttestationService::BuildChallengeResponseForVAChallenge(
    const std::string& serialized_signed_challenge,
    base::Value::Dict signals,
    AttestationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Signals have to at least have the non-empty device ID and obfuscated
  // customer ID.
  if (!signals.FindString(device_signals::names::kDeviceId) ||
      !signals.FindString(device_signals::names::kObfuscatedCustomerId)) {
    LogAttestationResult(DTAttestationResult::kMissingCoreSignals);
    std::move(callback).Run(std::string());
    return;
  }

  key_manager_->ExportPublicKeyAsync(
      base::BindOnce(&DesktopAttestationService::OnPublicKeyExported,
                     weak_factory_.GetWeakPtr(), serialized_signed_challenge,
                     std::move(signals), std::move(callback)));
}

void DesktopAttestationService::OnPublicKeyExported(
    const std::string& serialized_signed_challenge,
    base::Value::Dict signals,
    AttestationCallback callback,
    absl::optional<std::string> exported_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!exported_key) {
    // No key is available, so mark the device as untrusted (no challenge
    // response).
    LogAttestationResult(DTAttestationResult::kMissingSigningKey);
    std::move(callback).Run(std::string());
    return;
  }

  SignedData signed_data;
  if (serialized_signed_challenge.empty() ||
      !signed_data.ParseFromString(serialized_signed_challenge)) {
    // Challenge is not properly formatted, so mark the device as untrusted (no
    // challenge response).
    LogAttestationResult(DTAttestationResult::kBadChallengeFormat);
    std::move(callback).Run(std::string());
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ChallengeComesFromVerifiedAccess, signed_data,
                     google_keys_.va_signing_key(kVAType).modulus_in_hex()),
      base::BindOnce(&DesktopAttestationService::OnChallengeValidated,
                     weak_factory_.GetWeakPtr(), signed_data,
                     exported_key.value(), std::move(signals),
                     std::move(callback)));
}

void DesktopAttestationService::OnChallengeValidated(
    const SignedData& signed_data,
    const std::string& exported_public_key,
    base::Value::Dict signals,
    AttestationCallback callback,
    bool is_va_challenge) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_va_challenge) {
    // Challenge does not come from VA, so mark the device as untrusted (no
    // challenge response).
    LogAttestationResult(DTAttestationResult::kBadChallengeSource);
    std::move(callback).Run(std::string());
    return;
  }

  // Fill `key_info` out for Chrome Browser.
  // TODO(crbug.com/1241870): Remove public key from signals.
  KeyInfo key_info;
  key_info.set_key_type(CBCM);
  key_info.set_browser_instance_public_key(exported_public_key);
  key_info.set_device_id(*signals.FindString(device_signals::names::kDeviceId));
  key_info.set_customer_id(
      *signals.FindString(device_signals::names::kObfuscatedCustomerId));

  // VA currently only accepts the signals in a protobuf format.
  std::unique_ptr<DeviceTrustSignals> signals_proto =
      DictionarySignalsToProtobufSignals(signals);
  key_info.set_allocated_device_trust_signals(signals_proto.release());

  std::string serialized_key_info;
  if (!key_info.SerializeToString(&serialized_key_info)) {
    LogAttestationResult(DTAttestationResult::kFailedToSerializeKeyInfo);
    std::move(callback).Run(std::string());
    return;
  }

  auto va_encryption_key = google_keys_.va_encryption_key(kVAType);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateChallengeResponseString, serialized_key_info,
                     signed_data, va_encryption_key.modulus_in_hex(),
                     va_encryption_key.key_id()),
      base::BindOnce(&DesktopAttestationService::OnResponseCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DesktopAttestationService::OnResponseCreated(
    AttestationCallback callback,
    absl::optional<std::string> serialized_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!serialized_response) {
    // Failed to create a response, so mark the device as untrusted (no
    // challenge response).
    LogAttestationResult(DTAttestationResult::kFailedToGenerateResponse);
    std::move(callback).Run(std::string());
    return;
  }

  key_manager_->SignStringAsync(
      serialized_response.value(),
      base::BindOnce(&DesktopAttestationService::OnResponseSigned,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     serialized_response.value()));
}

void DesktopAttestationService::OnResponseSigned(
    AttestationCallback callback,
    const std::string& serialized_response,
    absl::optional<std::vector<uint8_t>> encrypted_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!encrypted_response) {
    // Failed to sign the response, so mark the device as untrusted (no
    // challenge response).
    LogAttestationResult(DTAttestationResult::kFailedToSignResponse);
    std::move(callback).Run(std::string());
    return;
  }

  // Encode the challenge-response values into a JSON string and return them.
  SignedData signed_data;
  signed_data.set_data(serialized_response);
  signed_data.set_signature(encrypted_response->data(),
                            encrypted_response->size());

  std::string serialized_attestation_response;
  if (!signed_data.SerializeToString(&serialized_attestation_response)) {
    LogAttestationResult(DTAttestationResult::kFailedToSerializeResponse);
    std::move(callback).Run(std::string());
    return;
  }

  std::string json_response;
  if (!serialized_attestation_response.empty()) {
    LogAttestationResult(DTAttestationResult::kSuccess);
    json_response =
        ProtobufChallengeToJsonChallenge(serialized_attestation_response);
  } else {
    LogAttestationResult(DTAttestationResult::kEmptySerializedResponse);
  }

  std::move(callback).Run(json_response);
}

}  // namespace enterprise_connectors
