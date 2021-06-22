// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_SERVICE_H_

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_interface.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/google_keys.h"

namespace enterprise_connectors {

class DeviceTrustKeyPair;

// This class is in charge of handling the key pair used for attestation. Also
// provides the methods needed in the handshake between Chrome, an IdP and
// Verified Access.
// The main method is `SignEnterpriseChallenge` which take a challenge request
// and builds a challenge response and wrap it into a
// `SignEnterpriseChallengeReply`.
class AttestationService {
 public:
  using AttestationCallback = base::OnceCallback<void(const std::string&)>;

  AttestationService();
  ~AttestationService();

  // Export the public key of `key_pair_` in SubjectPublicKeyInfo format.
  std::string ExportPublicKey();

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

  // Verify challenge comes from Verify Access.
  bool ChallengeComesFromVerifiedAccess(
      const std::string& serialized_signed_data,
      const std::string& public_key_modulus_hex);

  // Returns the challenge response proto.
  std::string VerifyChallengeAndMaybeCreateChallengeResponse(
      const std::string& serialized_signed_data,
      const std::string& public_key_modulus_hex);

  // If the challenge comes from Verified Access, generate the
  // proper challenge response, otherwise reply with empty string.
  void BuildChallengeResponseForVAChallenge(const std::string& challenge,
                                            AttestationCallback callback);

 private:
  // The KeyInfo message encrypted using a public encryption key, with
  // the following parameters:
  //   Key encryption: RSA-OAEP with no custom parameters.
  //   Data encryption: 256-bit key, AES-CBC with PKCS5 padding.
  //   MAC: HMAC-SHA-512 using the AES key.
  bool EncryptEnterpriseKeyInfo(VAType va_type,
                                const KeyInfo& key_info,
                                EncryptedData* encrypted_data);

  // Sign the challenge and return the challenge response in
  // `result.challenge_response`.
  void SignEnterpriseChallengeTask(
      const SignEnterpriseChallengeRequest& request,
      SignEnterpriseChallengeReply* result);

  // Sign `data` using `key_pair_` and store that value in `signature`.
  bool SignChallengeData(const std::string& data, std::string* response);

  void PaserChallengeResponseAndRunCallback(
      const std::string& challenge,
      AttestationCallback callback,
      const std::string& challenge_response_proto);

  // Get customer id from policy fetch if CloudPolicyStore is loaded.
  // If the CloudPolicyStore is not ready to retrieve the value, do nothing.
  void MayGetCustomerId();

  // Fill out `public_key_`, `customer_id` and `device_id_`.
  void FillValuesForCBCM();

  GoogleKeys google_keys_;
  std::string public_key_;
  std::string customer_id_;
  std::string device_id_;
  std::unique_ptr<enterprise_connectors::DeviceTrustKeyPair> key_pair_;

  base::WeakPtrFactory<AttestationService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_SERVICE_H_
