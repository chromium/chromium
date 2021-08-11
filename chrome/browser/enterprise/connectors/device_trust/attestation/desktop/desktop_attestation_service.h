// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_DESKTOP_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_DESKTOP_ATTESTATION_SERVICE_H_

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/google_keys.h"
#include "crypto/unexportable_key.h"

namespace enterprise_connectors {

class SigningKeyPair;

// This class is in charge of handling the key pair used for attestation. Also
// provides the methods needed in the handshake between Chrome, an IdP and
// Verified Access.
class DesktopAttestationService : public AttestationService {
 public:
  DesktopAttestationService();
  ~DesktopAttestationService() override;

  // Export the public key of `key_pair_` in SubjectPublicKeyInfo format.
  std::string ExportPublicKey();

  // Builds a challenge response for the given challenge |request| and wraps it
  // into the |result|.
  void SignEnterpriseChallenge(const SignEnterpriseChallengeRequest& request,
                               SignEnterpriseChallengeReply* result);

  // Set a signing key for testing so that it does not need to be read from
  // platform specific storage.
  void SetKeyPairForTesting(
      std::unique_ptr<crypto::UnexportableSigningKey> key_pair);

  // AttestationService:
  void BuildChallengeResponseForVAChallenge(
      const std::string& challenge,
      AttestationCallback callback) override;
  void StampReport(DeviceTrustReportEvent& report) override;

 private:
  // Verify challenge comes from Verify Access.
  bool ChallengeComesFromVerifiedAccess(
      const std::string& serialized_signed_data,
      const std::string& public_key_modulus_hex);

  // Returns the challenge response proto.
  std::string VerifyChallengeAndMaybeCreateChallengeResponse(
      const std::string& serialized_signed_data,
      const std::string& public_key_modulus_hex);

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

  void ParseChallengeResponseAndRunCallback(
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
  std::unique_ptr<enterprise_connectors::SigningKeyPair> key_pair_;

  base::WeakPtrFactory<DesktopAttestationService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_DESKTOP_ATTESTATION_SERVICE_H_
