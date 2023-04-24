// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"

class Profile;

namespace ash {
namespace attestation {
struct TpmChallengeKeyResult;
class TpmChallengeKeyWithTimeout;
}  // namespace attestation
}  // namespace ash

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
      const std::string& serialized_signed_challenge,
      base::Value::Dict signals,
      AttestationCallback callback) override;

 private:
  // Runs the `callback` which resumes the navigation with the `result`
  // challenge response. In case the challenge response was not successfully
  // built. An empty challenge response will be used. `tpm_key_challenger` is
  // also forwarded to ensure the instance lives as long as the callback runs.
  void ReturnResult(
      std::unique_ptr<ash::attestation::TpmChallengeKeyWithTimeout>
          tpm_key_challenger,
      AttestationCallback callback,
      const ash::attestation::TpmChallengeKeyResult& result);

  const raw_ptr<Profile, ExperimentalAsh> profile_;

  base::WeakPtrFactory<AshAttestationService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_H_
