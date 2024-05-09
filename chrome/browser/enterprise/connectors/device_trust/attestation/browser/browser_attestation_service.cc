// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/browser_attestation_service.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/attestation_switches.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/crypto_utility.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "crypto/random.h"

namespace enterprise_connectors {

namespace {

// Size of nonce for challenge response.
const size_t kChallengeResponseNonceBytesSize = 32;

// Verifies that the `signed_challenge_data` comes from Verified Access.
bool ChallengeComesFromVerifiedAccess(
    const SignedData& signed_challenge_data,
    const std::string& va_public_key_modulus_hex) {
  // Verify challenge signature.
  return CryptoUtility::VerifySignatureUsingHexKey(
      va_public_key_modulus_hex, signed_challenge_data.data(),
      signed_challenge_data.signature());
}

VAType GetVAType() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseVaDevKeys)) {
    return VAType::TEST_VA;
  }
  return VAType::DEFAULT_VA;
}

// The KeyInfo message encrypted using a public encryption key, with
// the following parameters:
//   Key encryption: RSA-OAEP with no custom parameters.
//   Data encryption: 256-bit key, AES-CBC with PKCS5 padding.
//   MAC: HMAC-SHA-512 using the AES key.
std::optional<std::string> CreateChallengeResponseString(
    const std::string& serialized_key_info,
    const SignedData& signed_challenge_data,
    const std::string& wrapping_key_modulus_hex,
    const std::string& wrapping_key_id) {
  ChallengeResponse response_pb;
  *response_pb.mutable_challenge() = signed_challenge_data;

  std::string* nonce = response_pb.mutable_nonce();
  nonce->resize(kChallengeResponseNonceBytesSize);
  crypto::RandBytes(base::as_writable_byte_span(*nonce));

  std::string key;
  if (!CryptoUtility::EncryptWithSeed(
          serialized_key_info, response_pb.mutable_encrypted_key_info(), key)) {
    return std::nullopt;
  }

  bssl::UniquePtr<RSA> rsa(CryptoUtility::GetRSA(wrapping_key_modulus_hex));
  if (!rsa) {
    return std::nullopt;
  }

  if (!CryptoUtility::WrapKeyOAEP(key, rsa.get(), wrapping_key_id,
                                  response_pb.mutable_encrypted_key_info())) {
    return std::nullopt;
  }

  // Convert the challenge response proto to a string before returning it.
  std::string serialized_response;
  if (!response_pb.SerializeToString(&serialized_response)) {
    return std::nullopt;
  }
  return serialized_response;
}

}  // namespace

BrowserAttestationService::BrowserAttestationService(
    std::vector<std::unique_ptr<Attester>> attesters)
    : attesters_(std::move(attesters)),
      background_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  CHECK(attesters_.size() > 0);
}

BrowserAttestationService::~BrowserAttestationService() = default;

// Goes through the following steps in order:
// - Validate challenge comes from VA,
// - Generated challenge response,
// - Sign response,
// - Encode encrypted data,
// - Reply to callback.
void BrowserAttestationService::BuildChallengeResponseForVAChallenge(
    const std::string& challenge,
    base::Value::Dict signals,
    const std::set<DTCPolicyLevel>& levels,
    AttestationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SignedData signed_data;
  if (challenge.empty() || !signed_data.ParseFromString(challenge)) {
    // Challenge is not properly formatted, so mark the device as untrusted (no
    // challenge response).
    std::move(callback).Run(
        {std::string(), DTAttestationResult::kBadChallengeFormat});
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ChallengeComesFromVerifiedAccess, signed_data,
                     google_keys_.va_signing_key(GetVAType()).modulus_in_hex()),
      base::BindOnce(&BrowserAttestationService::OnChallengeValidated,
                     weak_factory_.GetWeakPtr(), signed_data,
                     std::move(signals), levels, std::move(callback)));
}

void BrowserAttestationService::OnChallengeValidated(
    const SignedData& signed_data,
    base::Value::Dict signals,
    const std::set<DTCPolicyLevel>& levels,
    AttestationCallback callback,
    bool is_va_challenge) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_va_challenge) {
    // Challenge does not come from VA, so mark the device as untrusted (no
    // challenge response).
    std::move(callback).Run(
        {std::string(), DTAttestationResult::kBadChallengeSource});
    return;
  }

  // Fill `key_info` out for Chrome Browser.
  auto key_info = std::make_unique<KeyInfo>();
  key_info->set_flow_type(CBCM);
  // VA should accept signals JSON string.
  std::string signals_json;
  if (!base::JSONWriter::Write(signals, &signals_json)) {
    std::move(callback).Run(
        {std::string(), DTAttestationResult::kFailedToSerializeSignals});
    return;
  }
  key_info->set_device_trust_signals_json(signals_json);

  // Populate profile and/or device level information.
  auto* key_info_ptr = key_info.get();
  auto barrier_closure = base::BarrierClosure(
      /*num_closures=*/attesters_.size(),
      base::BindOnce(&BrowserAttestationService::OnKeyInfoDecorated,
                     weak_factory_.GetWeakPtr(), signed_data, levels,
                     std::move(callback), std::move(key_info)));

  for (const auto& attester : attesters_) {
    attester->DecorateKeyInfo(levels, *key_info_ptr, barrier_closure);
  }
}

void BrowserAttestationService::OnKeyInfoDecorated(
    const SignedData& signed_data,
    const std::set<DTCPolicyLevel>& levels,
    AttestationCallback callback,
    std::unique_ptr<KeyInfo> key_info) {
  std::string serialized_key_info;
  if (!key_info->SerializeToString(&serialized_key_info)) {
    std::move(callback).Run(
        {std::string(), DTAttestationResult::kFailedToSerializeKeyInfo});
    return;
  }

  auto va_encryption_key = google_keys_.va_encryption_key(GetVAType());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateChallengeResponseString, serialized_key_info,
                     signed_data, va_encryption_key.modulus_in_hex(),
                     va_encryption_key.key_id()),
      base::BindOnce(&BrowserAttestationService::OnResponseCreated,
                     weak_factory_.GetWeakPtr(), levels, std::move(callback)));
}

void BrowserAttestationService::OnResponseCreated(
    const std::set<DTCPolicyLevel>& levels,
    AttestationCallback callback,
    std::optional<std::string> encrypted_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!encrypted_response) {
    // Failed to create a response, so mark the device as untrusted (no
    // challenge response).
    std::move(callback).Run(
        {std::string(), DTAttestationResult::kFailedToGenerateResponse});
    return;
  }

  // Add profile and/or device signature to the signed data.
  auto signed_data = std::make_unique<SignedData>();
  auto* signed_data_ptr = signed_data.get();

  auto barrier_closure = base::BarrierClosure(
      /*num_closures=*/attesters_.size(),
      base::BindOnce(&BrowserAttestationService::OnResponseSigned,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     encrypted_response.value(), std::move(signed_data)));

  for (const auto& attester : attesters_) {
    attester->SignResponse(levels, encrypted_response.value(), *signed_data_ptr,
                           barrier_closure);
  }
}

void BrowserAttestationService::OnResponseSigned(
    AttestationCallback callback,
    const std::string& encrypted_response,
    std::unique_ptr<SignedData> signed_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(signed_data);

  // Encode the challenge-response values into a JSON string and return them.
  signed_data->set_data(encrypted_response);

  std::string serialized_attestation_response;
  if (!signed_data->SerializeToString(&serialized_attestation_response)) {
    std::move(callback).Run(
        {std::string(), DTAttestationResult::kFailedToSerializeResponse});
    return;
  }

  std::string json_response;
  if (!serialized_attestation_response.empty()) {
    json_response =
        ProtobufChallengeToJsonChallenge(serialized_attestation_response);
  }

  std::move(callback).Run(
      {json_response, json_response.empty()
                          ? DTAttestationResult::kEmptySerializedResponse
                      : signed_data->has_signature()
                          ? DTAttestationResult::kSuccess
                          : DTAttestationResult::kSuccessNoSignature});
}

}  // namespace enterprise_connectors
