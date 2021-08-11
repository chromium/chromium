// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_H_

#include "chrome/browser/ash/attestation/tpm_challenge_key_with_timeout.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"

class Profile;

namespace enterprise_connectors {

// This class is in charge of handling the key pair used for attestation. Also
// provides the methods needed in the handshake between Chrome, an IdP and
// Verified Access.
class AshAttestationService : public AttestationService {
 public:
  explicit AshAttestationService(Profile* profile);
  ~AshAttestationService() override;

  // AttestationService:
  void BuildChallengeResponseForVAChallenge(
      const std::string& challenge,
      AttestationCallback callback) override;

 private:
  // Run the callback that may resume the navigation with the challenge
  // response. In case the challenge response was not successfully built. An
  // empty challenge response will be used.
  void ReturnResult(AttestationCallback callback,
                    const ash::attestation::TpmChallengeKeyResult& result);

  Profile* profile_;
  std::unique_ptr<ash::attestation::TpmChallengeKeyWithTimeout>
      tpm_key_challenger_;

  base::WeakPtrFactory<AshAttestationService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_H_
