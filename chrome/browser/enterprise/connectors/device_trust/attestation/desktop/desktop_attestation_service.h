// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_DESKTOP_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_DESKTOP_ATTESTATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/google_keys.h"

namespace enterprise_connectors {

class DeviceTrustSignals;
class EncryptedData;
class KeyInfo;
class SigningKeyPair;

// This class is in charge of handling the key pair used for attestation. Also
// provides the methods needed in the handshake between Chrome, an IdP and
// Verified Access.
class DesktopAttestationService : public AttestationService {
 public:
  explicit DesktopAttestationService(std::unique_ptr<SigningKeyPair> key_pair);
  ~DesktopAttestationService() override;

  // Export the public key of `key_pair_` in SubjectPublicKeyInfo format.
  std::string ExportPublicKey();

  // Builds a challenge response for the given challenge `request` and
  // `signals`, and wraps it into the `result`.
  void SignEnterpriseChallenge(const SignEnterpriseChallengeRequest& request,
                               std::unique_ptr<DeviceTrustSignals> signals,
                               SignEnterpriseChallengeReply* result);

  // AttestationService:
  void BuildChallengeResponseForVAChallenge(
      const std::string& challenge,
      std::unique_ptr<DeviceTrustSignals> signals,
      AttestationCallback callback) override;
  void StampReport(DeviceTrustReportEvent& report) override;
  bool RotateSigningKey() override;

 private:
  // Verify challenge comes from Verify Access.
  bool ChallengeComesFromVerifiedAccess(
      const std::string& serialized_signed_data,
      const std::string& public_key_modulus_hex);

  // Returns the challenge response proto.
  std::string VerifyChallengeAndMaybeCreateChallengeResponse(
      const std::string& serialized_signed_data,
      const std::string& public_key_modulus_hex,
      std::unique_ptr<DeviceTrustSignals> signals);

  // The KeyInfo message encrypted using a public encryption key, with
  // the following parameters:
  //   Key encryption: RSA-OAEP with no custom parameters.
  //   Data encryption: 256-bit key, AES-CBC with PKCS5 padding.
  //   MAC: HMAC-SHA-512 using the AES key.
  bool EncryptEnterpriseKeyInfo(VAType va_type,
                                const KeyInfo& key_info,
                                EncryptedData* encrypted_data);

  // Sign `data` using `key_pair_` and store that value in `signature`.
  bool SignChallengeData(const std::string& data, std::string* response);

  void ParseChallengeResponseAndRunCallback(
      const std::string& challenge,
      AttestationCallback callback,
      const std::string& challenge_response_proto);

  GoogleKeys google_keys_;
  std::unique_ptr<enterprise_connectors::SigningKeyPair> key_pair_;

  base::WeakPtrFactory<DesktopAttestationService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_DESKTOP_ATTESTATION_SERVICE_H_
