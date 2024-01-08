// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service.h"

class Profile;

namespace ash {
namespace attestation {
struct TpmChallengeKeyResult;
class TpmChallengeKeyWithTimeout;
class TpmChallengeKeySubtle;
}  // namespace attestation
}  // namespace ash

namespace enterprise_connectors {

class AshAttestationServiceImpl : public AshAttestationService {
 public:
  explicit AshAttestationServiceImpl(Profile* profile);
  ~AshAttestationServiceImpl() override;

  using Username = base::StrongAlias<class UsernameTag, std::string>;
  using KeyName = base::StrongAlias<class UsernameTag, std::string>;

  // Returns the DTC key name corresponding to the username. The key will be
  // associated with a DeviceTrustConnectorUserCertificate.
  static Username GetDeviceTrustConnectorUserKeyName(const Username& username);

  // Returns a WeakPtr for the current service.
  base::WeakPtr<AshAttestationServiceImpl> GetWeakPtr();

  // AshAttestationService:
  void TryPrepareKey() override;

  // AttestationService:
  void BuildChallengeResponseForVAChallenge(
      const std::string& serialized_signed_challenge,
      base::Value::Dict signals,
      const std::set<DTCPolicyLevel>& levels,
      AttestationCallback callback) override;

 private:
  // Logs an error if the key preparation failed.
  void KeyPrepareCallback(
      std::unique_ptr<ash::attestation::TpmChallengeKeySubtle>
          tpm_key_challenger,
      const ash::attestation::TpmChallengeKeyResult& result);

  // Runs the `callback` which resumes the navigation with the `result`
  // challenge response. In case the challenge response was not successfully
  // built. An empty challenge response will be used. `tpm_key_challenger` is
  // also forwarded to ensure the instance lives as long as the callback runs.
  void ReturnResult(
      std::unique_ptr<ash::attestation::TpmChallengeKeyWithTimeout>
          tpm_key_challenger,
      AttestationCallback callback,
      const ash::attestation::TpmChallengeKeyResult& result);

  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<AshAttestationServiceImpl> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_IMPL_H_
