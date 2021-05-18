// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation_service.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/crypto_utility.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_key_pair.h"

namespace enterprise_connectors {

AttestationService::AttestationService() {
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  key_pair_ = std::make_unique<enterprise_connectors::DeviceTrustKeyPair>();
  key_pair_->Init();
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
}

AttestationService::~AttestationService() = default;

bool AttestationService::ChallengeComesFromVerifiedAccess(
    const std::string& serialized_signed_data,
    const std::string& public_key_modulus_hex) {
  SignedData signed_challenge;
  signed_challenge.ParseFromString(serialized_signed_data);
  // Verify challenge signature.
  return enterprise_connector::CryptoUtility::VerifySignatureUsingHexKey(
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

  base::Base64Decode(data.value().FindPath("challenge.data")->GetString(),
                     signed_challenge.mutable_data());
  base::Base64Decode(data.value().FindPath("challenge.signature")->GetString(),
                     signed_challenge.mutable_signature());

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

  std::string encoded;
  base::Base64Encode(challenge_response, &encoded);
  signed_data.SetKey("data", base::Value(encoded));

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  std::string signature;
  key_pair_->GetSignatureInBase64(challenge_response, &signature);
  signed_data.SetKey("signature", base::Value(signature));
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("challengeResponse", std::move(signed_data));

  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
std::string AttestationService::ExportPEMPublicKey() {
  return key_pair_->ExportPEMPublicKey();
}
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

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
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }

  KeyInfo key_info;
  // Set the public key so VA can verify the client.
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  key_info.set_signed_public_key_and_challenge(ExportPEMPublicKey());
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

  ChallengeResponse response_pb;
  *response_pb.mutable_challenge() = signed_challenge;
  // TODO(b/185459013): Encrypt `key_info` and add it to `response_pb`.

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

bool AttestationService::SignChallengeData(const std::string& data,
                                           std::string* response) {
  std::string signature;
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  if (!key_pair_->GetSignatureInBase64(data, &signature)) {
    LOG(ERROR) << __func__ << ": Failed to sign data.";
    return false;
  }
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  SignedData signed_data;
  signed_data.set_data(data);
  signed_data.set_signature(signature);
  if (!signed_data.SerializeToString(response)) {
    LOG(ERROR) << __func__ << ": Failed to serialize signed data.";
    return false;
  }
  return true;
}

void AttestationService::BuildChallengeResponseForVAChallenge(
    const std::string& challenge,
    AttestationCallback callback) {
  std::string serialized_signed_data =
      JsonChallengeToProtobufChallenge(challenge);

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

}  // namespace enterprise_connectors
