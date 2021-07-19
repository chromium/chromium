// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_utils.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/attestation/attestation_ca.pb.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_attestation_ca.pb.h"
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

namespace enterprise_connectors {

std::string JsonChallengeToProtobufChallenge(const std::string& challenge) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  attestation::SignedData signed_challenge;
#else
  SignedData signed_challenge;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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

std::string ProtobufChallengeToJsonChallenge(
    const std::string& challenge_response) {
  base::Value signed_data(base::Value::Type::DICTIONARY);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  attestation::SignedData signed_data_proto;
#else
  SignedData signed_data_proto;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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

}  // namespace enterprise_connectors
