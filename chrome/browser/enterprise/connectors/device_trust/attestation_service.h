// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_SERVICE_H_

#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/interface.pb.h"

namespace enterprise_connectors {

class DeviceTrustKeyPair;

}

namespace attestation {

// This class is in charge of handling the key pair used for attestation. Also
// provides the methods needed in the handshake between Chrome, an IdP and
// Verified Access.
// The main method is `SignEnterpriseChallenge` which take a challenge request
// and builds a challenge response and wrap it into a
// `SignEnterpriseChallengeReply`.
class AttestationService {
 public:
  AttestationService();
  ~AttestationService();

  // Export the public key of `key_pair_`.
  std::string ExportPEMPublicKey();

  void SignEnterpriseChallenge(const SignEnterpriseChallengeRequest& request,
                               SignEnterpriseChallengeReply* result);

  // Take the challenge that comes from the Idp in json format and generate a
  // SignedData proto.
  // The expected format of the challenge is the following:
  // {
  //    "challenge": {
  //        object (SignedData)
  //    }
  // }
  // SignedData has the following scheme:
  // {
  //    "data": string (base64 encoded string),
  //    "signature": string (base64 encoded string),
  // }
  std::string JsonChallengeToProtobufChallenge(const std::string& challenge);

  // Take a challenge_response proto and return the json version of it.
  // The format is the following:
  // {
  //    "challengeResponse": {
  //        object (SignedData)
  //    }
  // }
  // SignedData has the following scheme:
  // {
  //    "data": string (base64 encoded string),
  //    "signature": string (base64 encoded string),
  // }
  std::string ProtobufChallengeToJsonChallenge(
      const std::string& challenge_response);

 private:
  // Sign the challenge and return the challenge response in
  // `result.challenge_response`.
  void SignEnterpriseChallengeTask(
      const SignEnterpriseChallengeRequest& request,
      SignEnterpriseChallengeReply* result);

  // Sign `data` using `key_pair_` and store that value in `signature`.
  bool SignChallengeData(const std::string& data, std::string* response);

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  std::unique_ptr<enterprise_connectors::DeviceTrustKeyPair> key_pair_;
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
};

}  // namespace attestation

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_SERVICE_H_
