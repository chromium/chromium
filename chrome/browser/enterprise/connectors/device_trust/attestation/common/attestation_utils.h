// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_UTILS_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"

namespace enterprise_connectors {

// Take a challenge_response proto and return the json version of it.
// The format follows Vaapi v2 definition:
// {
//    "challengeResponse": base64 encoded SignedData
// }
std::string ProtobufChallengeToJsonChallenge(
    const std::string& challenge_response);

// Takes the dictionary of signals `signals_dict` and converts it to a
// signals proto.
std::unique_ptr<SignalsType> DictionarySignalsToProtobufSignals(
    const base::Value::Dict& signals_dict);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_UTILS_H_
