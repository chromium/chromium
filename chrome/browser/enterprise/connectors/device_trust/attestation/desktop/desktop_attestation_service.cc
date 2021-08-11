// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/crypto_utility.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "crypto/random.h"

namespace enterprise_connectors {

namespace {

// Size of nonce for challenge response.
const size_t kChallengResponseNonceBytesSize = 32;

}  // namespace

DesktopAttestationService::DesktopAttestationService()
    : key_pair_(std::make_unique<SigningKeyPair>()) {}

DesktopAttestationService::~DesktopAttestationService() = default;

void DesktopAttestationService::FillValuesForCBCM() {
  if (public_key_.empty())
    public_key_ = ExportPublicKey();
  if (device_id_.empty())
    device_id_ = policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
  if (customer_id_.empty())
    MayGetCustomerId();
}

void DesktopAttestationService::MayGetCustomerId() {
  policy::ChromeBrowserPolicyConnector* browser_policy_connector =
      g_browser_process->browser_policy_connector();
  if (!browser_policy_connector)
    return;
  policy::MachineLevelUserCloudPolicyManager*
      machine_level_user_cloud_policy_manager =
          browser_policy_connector->machine_level_user_cloud_policy_manager();
  // Check that we can retrieve the customer id.
  if (!machine_level_user_cloud_policy_manager ||
      !machine_level_user_cloud_policy_manager->store() ||
      !machine_level_user_cloud_policy_manager->store()->has_policy())
    return;
  customer_id_ = machine_level_user_cloud_policy_manager->store()
                     ->policy()
                     ->obfuscated_customer_id();
}

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
  std::vector<uint8_t> public_key_info;
  if (!key_pair_->ExportPublicKey(&public_key_info))
    return std::string();
  return std::string(public_key_info.begin(), public_key_info.end());
}

void DesktopAttestationService::BuildChallengeResponseForVAChallenge(
    const std::string& challenge,
    AttestationCallback callback) {
  std::string serialized_signed_data =
      JsonChallengeToProtobufChallenge(challenge);
  // If one of this values is missing then attestation flow won't succeed.
  FillValuesForCBCM();
  if (device_id_.empty() || public_key_.empty() || customer_id_.empty())
    LOG(ERROR) << "There are missing values for the attestation flow.";

  AttestationCallback reply = base::BindOnce(
      &DesktopAttestationService::ParseChallengeResponseAndRunCallback,
      weak_factory_.GetWeakPtr(), challenge, std::move(callback));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          &DesktopAttestationService::
              VerifyChallengeAndMaybeCreateChallengeResponse,
          base::Unretained(this), JsonChallengeToProtobufChallenge(challenge),
          google_keys_.va_signing_key(VAType::DEFAULT_VA).modulus_in_hex()),
      std::move(reply));
}

void DesktopAttestationService::SetKeyPairForTesting(
    std::unique_ptr<crypto::UnexportableSigningKey> key_pair) {
  key_pair_->SetKeyPairForTesting(std::move(key_pair));  // IN-TEST
}

void DesktopAttestationService::StampReport(DeviceTrustReportEvent& report) {
  auto* credential = report.mutable_attestation_credential();
  credential->set_format(
      DeviceTrustReportEvent::Credential::EC_NID_X9_62_PRIME256V1_PUBLIC_DER);
  credential->set_credential(ExportPublicKey());
}

std::string
DesktopAttestationService::VerifyChallengeAndMaybeCreateChallengeResponse(
    const std::string& serialized_signed_data,
    const std::string& public_key_modulus_hex) {
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
  SignEnterpriseChallenge(request, &result);
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
    SignEnterpriseChallengeReply* result) {
  SignEnterpriseChallengeTask(request, result);
}

void DesktopAttestationService::SignEnterpriseChallengeTask(
    const SignEnterpriseChallengeRequest& request,
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
  key_info.set_key_type(CBCM);
  key_info.set_browser_instance_public_key(public_key_);
  key_info.set_device_id(device_id_);
  key_info.set_customer_id(customer_id_);

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
  std::string signature;
  if (!key_pair_->SignMessage(data, signed_data.mutable_signature())) {
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
