// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"

#include <utility>

#include "base/base64.h"
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
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "crypto/random.h"
#include "crypto/unexportable_key.h"

namespace enterprise_connectors {

namespace {
using policy::BrowserDMTokenStorage;
using policy::CloudPolicyStore;

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
absl::optional<std::string> CreateChallengeResponseString(
    const std::string& serialized_key_info,
    const SignedData& signed_challenge_data,
    const std::string& wrapping_key_modulus_hex,
    const std::string& wrapping_key_id) {
  ChallengeResponse response_pb;
  *response_pb.mutable_challenge() = signed_challenge_data;

  crypto::RandBytes(base::WriteInto(response_pb.mutable_nonce(),
                                    kChallengeResponseNonceBytesSize + 1),
                    kChallengeResponseNonceBytesSize);

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
    BrowserDMTokenStorage* dm_token_storage,
    DeviceTrustKeyManager* key_manager,
    CloudPolicyStore* browser_cloud_policy_store)
    : dm_token_storage_(dm_token_storage),
      key_manager_(key_manager),
      browser_cloud_policy_store_(browser_cloud_policy_store),
      background_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK(dm_token_storage_);
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
    const std::string& challenge,
    base::Value::Dict signals,
    const std::set<DTCPolicyLevel>& levels,
    AttestationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  key_manager_->ExportPublicKeyAsync(
      base::BindOnce(&DesktopAttestationService::OnPublicKeyExported,
                     weak_factory_.GetWeakPtr(), challenge, std::move(signals),
                     levels, std::move(callback)));
}

void DesktopAttestationService::OnPublicKeyExported(
    const std::string& challenge,
    base::Value::Dict signals,
    const std::set<DTCPolicyLevel>& levels,
    AttestationCallback callback,
    absl::optional<std::string> exported_key) {
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
      base::BindOnce(&DesktopAttestationService::OnChallengeValidated,
                     weak_factory_.GetWeakPtr(), signed_data,
                     std::move(exported_key), std::move(signals), levels,
                     std::move(callback)));
}

void DesktopAttestationService::OnChallengeValidated(
    const SignedData& signed_data,
    const absl::optional<std::string>& exported_public_key,
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

  auto dm_token = dm_token_storage_->RetrieveDMToken();
  if (!dm_token.is_valid()) {
    std::move(callback).Run(
        {std::string(), DTAttestationResult::kMissingCoreSignals});
    return;
  }

  // Fill `key_info` out for Chrome Browser.
  KeyInfo key_info;
  key_info.set_flow_type(CBCM);
  // dm_token contains all of the information required by the server to retrieve
  // the device. device_id is necessary to validate the dm_token.
  key_info.set_dm_token(dm_token.value());
  key_info.set_device_id(dm_token_storage_->RetrieveClientId());

  if (browser_cloud_policy_store_ &&
      browser_cloud_policy_store_->has_policy()) {
    const auto* policy = browser_cloud_policy_store_->policy();
    key_info.set_customer_id(policy->obfuscated_customer_id());
  }

  if (exported_public_key) {
    key_info.set_browser_instance_public_key(exported_public_key.value());
  }

  // VA should accept signals JSON string.
  std::string signals_json;
  if (!base::JSONWriter::Write(signals, &signals_json)) {
    std::move(callback).Run(
        {std::string(), DTAttestationResult::kFailedToSerializeSignals});
    return;
  }

  key_info.set_device_trust_signals_json(signals_json);

  std::string serialized_key_info;
  if (!key_info.SerializeToString(&serialized_key_info)) {
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
      base::BindOnce(&DesktopAttestationService::OnResponseCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DesktopAttestationService::OnResponseCreated(
    AttestationCallback callback,
    absl::optional<std::string> encrypted_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!encrypted_response) {
    // Failed to create a response, so mark the device as untrusted (no
    // challenge response).
    std::move(callback).Run(
        {std::string(), DTAttestationResult::kFailedToGenerateResponse});
    return;
  }

  key_manager_->SignStringAsync(
      encrypted_response.value(),
      base::BindOnce(&DesktopAttestationService::OnResponseSigned,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     encrypted_response.value()));
}

void DesktopAttestationService::OnResponseSigned(
    AttestationCallback callback,
    const std::string& encrypted_response,
    absl::optional<std::vector<uint8_t>> signed_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Encode the challenge-response values into a JSON string and return them.
  SignedData signed_data;
  signed_data.set_data(encrypted_response);

  if (signed_response) {
    signed_data.set_signature(signed_response->data(), signed_response->size());
  }

  std::string serialized_attestation_response;
  if (!signed_data.SerializeToString(&serialized_attestation_response)) {
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
      {json_response,
       json_response.empty() ? DTAttestationResult::kEmptySerializedResponse
       : signed_response     ? DTAttestationResult::kSuccess
                             : DTAttestationResult::kSuccessNoSignature});
}

}  // namespace enterprise_connectors
