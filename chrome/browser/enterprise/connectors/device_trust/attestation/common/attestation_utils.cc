// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"

#include <memory>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "components/device_signals/core/common/signals_constants.h"

namespace enterprise_connectors {

std::string ProtobufChallengeToJsonChallenge(
    const std::string& challenge_response) {
  base::Value::Dict signed_data;

  std::string encoded = base::Base64Encode(challenge_response);

  base::Value::Dict dict;
  dict.Set("challengeResponse", base::Value(encoded));

  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

}  // namespace enterprise_connectors
