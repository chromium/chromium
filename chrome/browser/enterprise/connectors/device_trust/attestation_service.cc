// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation_service.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/crypto_utility.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_key_pair.h"
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

AttestationService::AttestationService() {
  key_pair_ = std::make_unique<DeviceTrustKeyPair>();
  if (!key_pair_->Init())
    LOG(ERROR) << "Error while initializing the key pair.";
}

AttestationService::~AttestationService() = default;

void AttestationService::FillValuesForCBCM() {
  if (public_key_.empty())
    public_key_ = ExportPublicKey();
  if (device_id_.empty())
    device_id_ = policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
  if (customer_id_.empty())
    MayGetCustomerId();
}

void AttestationService::MayGetCustomerId() {
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

bool AttestationService::ChallengeComesFromVerifiedAccess(
    const std::string& serialized_signed_data,
    const std::string& public_key_modulus_hex) {
  SignedData signed_challenge;
  signed_challenge.ParseFromString(serialized_signed_data);
  // Verify challenge signature.
  return CryptoUtility::VerifySignatureUsingHexKey(
      public_key_modulus_hex, signed_challenge.data(),
      signed_challenge.signature());
}

std::string AttestationService::JsonChallengeToProtobufChallenge(
    const std::string& challenge) {
  SignedData signed_challenge;
  // Get challenge and decode it.
  absl::optional<base::Value> data = base::JSONReader::Read(
      challenge, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  // If json is malformed or it doesn't include the needed fields return
  // an empty string.
  if (!data || !data.value().FindPath("challenge.data") ||
      !data.value().FindPath("challenge.signature"))
    return std::string();

  if (!base::Base64Decode(data.value().FindPath("challenge.data")->GetString(),
                          signed_challenge.mutable_data()))
    LOG(ERROR) << "Error during decoding base64 challenge data.";
  if (!base::Base64Decode(
          data.value().FindPath("challenge.signature")->GetString(),
          signed_challenge.mutable_signature()))
    LOG(ERROR) << "Error during decoding base64 challenge signature.";

  std::string serialized_signed_challenge;
  if (!signed_challenge.SerializeToString(&serialized_signed_challenge)) {
    LOG(ERROR) << __func__ << ": Failed to serialize signed data.";
    return std::string();
  }
  return serialized_signed_challenge;
}

std::string AttestationService::ProtobufChallengeToJsonChallenge(
    const std::string& challenge_response) {
  base::Value signed_data(base::Value::Type::DICTIONARY);

  SignedData signed_data_proto;
  signed_data_proto.ParseFromString(challenge_response);
  std::string encoded;
  base::Base64Encode(signed_data_proto.data(), &encoded);
  signed_data.SetKey("data", base::Value(encoded));

  base::Base64Encode(signed_data_proto.signature(), &encoded);
  signed_data.SetKey("signature", base::Value(encoded));

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("challengeResponse", std::move(signed_data));

  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

std::string AttestationService::ExportPublicKey() {
  std::vector<uint8_t> public_key_info;
  if (!key_pair_->ExportPublicKey(&public_key_info))
    return std::string();
  return std::string(public_key_info.begin(), public_key_info.end());
}

void AttestationService::BuildChallengeResponseForVAChallenge(
    const std::string& challenge,
    AttestationCallback callback) {
  std::string serialized_signed_data =
      JsonChallengeToProtobufChallenge(challenge);
  // If one of this values is missing then attestation flow won't succeed.
  FillValuesForCBCM();
  if (device_id_.empty() || public_key_.empty() || customer_id_.empty())
    LOG(ERROR) << "There are missing values for the attestation flow.";

  AttestationCallback reply = base::BindOnce(
      &AttestationService::PaserChallengeResponseAndRunCallback,
      weak_factory_.GetWeakPtr(), challenge, std::move(callback));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          &AttestationService::VerifyChallengeAndMaybeCreateChallengeResponse,
          base::Unretained(this), JsonChallengeToProtobufChallenge(challenge),
          google_keys_.va_signing_key(VAType::DEFAULT_VA).modulus_in_hex()),
      std::move(reply));
}

std::string AttestationService::VerifyChallengeAndMaybeCreateChallengeResponse(
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

void AttestationService::PaserChallengeResponseAndRunCallback(
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

void AttestationService::SignEnterpriseChallenge(
    const SignEnterpriseChallengeRequest& request,
    SignEnterpriseChallengeReply* result) {
  SignEnterpriseChallengeTask(request, result);
}

void AttestationService::SignEnterpriseChallengeTask(
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

bool AttestationService::EncryptEnterpriseKeyInfo(
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

bool AttestationService::SignChallengeData(const std::string& data,
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
