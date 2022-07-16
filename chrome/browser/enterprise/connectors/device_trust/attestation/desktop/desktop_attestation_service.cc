// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/crypto_utility.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "crypto/random.h"
#include "crypto/unexportable_key.h"

namespace enterprise_connectors {

namespace {

// Size of nonce for challenge response.
const size_t kChallengResponseNonceBytesSize = 32;

}  // namespace

DesktopAttestationService::DesktopAttestationService(
    std::unique_ptr<KeyPersistenceDelegate> key_persistence_delegate)
    : key_persistence_delegate_(std::move(key_persistence_delegate)) {
  DCHECK(key_persistence_delegate_);
  key_pair_ = SigningKeyPair::Create(key_persistence_delegate_.get());
}

DesktopAttestationService::~DesktopAttestationService() = default;

bool DesktopAttestationService::ChallengeComesFromVerifiedAccess(
    const std::string& serialized_signed_data,
    const std::string& public_key_modulus_hex) {
  SignedData signed_challenge;
  signed_challenge.ParseFromString(serialized_signed_data);
  // Verify challenge signature.
  return CryptoUtility::VerifySignatureUsingHexKey(
      public_key_modulus_hex, signed_challenge.data(),
      signed_challenge.signature());
}

std::string DesktopAttestationService::ExportPublicKey() {
  if (!key_pair_ || !key_pair_->key()) {
    return std::string();
  }
  auto public_key_info = key_pair_->key()->GetSubjectPublicKeyInfo();
  return std::string(public_key_info.begin(), public_key_info.end());
}

void DesktopAttestationService::BuildChallengeResponseForVAChallenge(
    const std::string& challenge,
    std::unique_ptr<DeviceTrustSignals> signals,
    AttestationCallback callback) {
  DCHECK(!ExportPublicKey().empty());
  DCHECK(signals);
  DCHECK(signals->has_device_id() && !signals->device_id().empty());
  DCHECK(signals->has_obfuscated_customer_id() &&
         !signals->obfuscated_customer_id().empty());

  AttestationCallback reply = base::BindOnce(
      &DesktopAttestationService::ParseChallengeResponseAndRunCallback,
      weak_factory_.GetWeakPtr(), challenge, std::move(callback));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          &DesktopAttestationService::
              VerifyChallengeAndMaybeCreateChallengeResponse,
          base::Unretained(this), JsonChallengeToProtobufChallenge(challenge),
          google_keys_.va_signing_key(VAType::DEFAULT_VA).modulus_in_hex(),
          std::move(signals)),
      std::move(reply));
}

std::string
DesktopAttestationService::VerifyChallengeAndMaybeCreateChallengeResponse(
    const std::string& serialized_signed_data,
    const std::string& public_key_modulus_hex,
    std::unique_ptr<DeviceTrustSignals> signals) {
  if (!ChallengeComesFromVerifiedAccess(serialized_signed_data,
                                        public_key_modulus_hex)) {
    LOG(ERROR) << "Challenge signature verification did not succeed.";
    return std::string();
  }
  // If the verification that the challenge comes from Verified Access succeed,
  // generate the challenge response.
  SignEnterpriseChallengeRequest request;
  SignEnterpriseChallengeReply result;
  request.set_challenge(serialized_signed_data);
  request.set_va_type(VAType::DEFAULT_VA);
  SignEnterpriseChallenge(request, std::move(signals), &result);
  return result.challenge_response();
}

void DesktopAttestationService::ParseChallengeResponseAndRunCallback(
    const std::string& challenge,
    AttestationCallback callback,
    const std::string& challenge_response_proto) {
  if (challenge_response_proto != std::string()) {
    // Return to callback (throttle with the challenge response) with empty
    // challenge response.
    std::move(callback).Run(
        ProtobufChallengeToJsonChallenge(challenge_response_proto));
  } else {
    // Make challenge response
    std::move(callback).Run("");
  }
}

void DesktopAttestationService::SignEnterpriseChallenge(
    const SignEnterpriseChallengeRequest& request,
    std::unique_ptr<DeviceTrustSignals> signals,
    SignEnterpriseChallengeReply* result) {
  // Validate that the challenge is coming from the expected source.
  SignedData signed_challenge;
  if (!signed_challenge.ParseFromString(request.challenge())) {
    LOG(ERROR) << __func__ << ": Failed to parse signed challenge.";
    result->set_status(STATUS_INVALID_PARAMETER_ERROR);
    return;
  }
  KeyInfo key_info;
  // Fill `key_info` out for Chrome Browser.
  // TODO(crbug.com/1241870): Remove public key from signals.
  key_info.set_key_type(CBCM);
  key_info.set_browser_instance_public_key(ExportPublicKey());
  key_info.set_device_id(signals->device_id());
  key_info.set_customer_id(signals->obfuscated_customer_id());

  key_info.set_allocated_device_trust_signals(signals.release());

  ChallengeResponse response_pb;
  *response_pb.mutable_challenge() = signed_challenge;

  crypto::RandBytes(base::WriteInto(response_pb.mutable_nonce(),
                                    kChallengResponseNonceBytesSize + 1),
                    kChallengResponseNonceBytesSize);
  if (!EncryptEnterpriseKeyInfo(request.va_type(), key_info,
                                response_pb.mutable_encrypted_key_info())) {
    LOG(ERROR) << __func__ << ": Failed to encrypt KeyInfo.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }

  // Serialize and sign the response protobuf.
  std::string serialized;
  if (!response_pb.SerializeToString(&serialized)) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  // Sign data using the client generated key pair.
  if (!SignChallengeData(serialized, result->mutable_challenge_response())) {
    result->clear_challenge_response();
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
}

bool DesktopAttestationService::EncryptEnterpriseKeyInfo(
    VAType va_type,
    const KeyInfo& key_info,
    EncryptedData* encrypted_data) {
  std::string serialized;
  if (!key_info.SerializeToString(&serialized)) {
    LOG(ERROR) << "Failed to serialize key info.";
    return false;
  }

  std::string key;
  if (!CryptoUtility::EncryptWithSeed(serialized, encrypted_data, key)) {
    LOG(ERROR) << "EncryptWithSeed failed.";
    return false;
  }
  bssl::UniquePtr<RSA> rsa(CryptoUtility::GetRSA(
      google_keys_.va_encryption_key(va_type).modulus_in_hex()));
  if (!rsa)
    return false;
  if (!CryptoUtility::WrapKeyOAEP(
          key, rsa.get(), google_keys_.va_encryption_key(va_type).key_id(),
          encrypted_data)) {
    encrypted_data->Clear();
    return false;
  }
  return true;
}

bool DesktopAttestationService::SignChallengeData(const std::string& data,
                                                  std::string* response) {
  SignedData signed_data;
  signed_data.set_data(data);

  absl::optional<std::vector<uint8_t>> signature;
  if (key_pair_ && key_pair_->key()) {
    signature =
        key_pair_->key()->SignSlowly(base::as_bytes(base::make_span(data)));
  } else {
    LOG(ERROR) << __func__ << ": Failed, no key to sign data.";
    return false;
  }

  if (signature.has_value()) {
    signed_data.set_signature(signature->data(), signature->size());
  } else {
    LOG(ERROR) << __func__ << ": Failed to sign data.";
    return false;
  }

  if (!signed_data.SerializeToString(response)) {
    LOG(ERROR) << __func__ << ": Failed to serialize signed data.";
    return false;
  }
  return true;
}

}  // namespace enterprise_connectors
