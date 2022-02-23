// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_UTILS_H_

#include <string>

namespace enterprise_connectors {

// Take the challenge that comes from the Idp in json format and generate a
// SignedData proto.
// The expected format of the challenge is the following:
// {
//    "challenge": base64 encoded SignedData
// }
std::string JsonChallengeToProtobufChallenge(const std::string& json_challenge);

// Take a challenge_response proto and return the json version of it.
// The format follows Vaapi v2 definition:
// {
//    "challengeResponse": base64 encoded SignedData
// }
std::string ProtobufChallengeToJsonChallenge(
    const std::string& challenge_response);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_UTILS_H_
