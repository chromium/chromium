// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_IMPL_H_

#include <cstdint>
#include <memory>

#include "base/containers/flat_map.h"
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
      base::DictValue signals,
      const std::set<DTCPolicyLevel>& levels,
      AttestationCallback callback) override;

 private:
  // Logs an error if the key preparation failed and removes the completed
  // request from `pending_key_preparations_`.
  void KeyPrepareCallback(
      uint64_t request_id,
      const ash::attestation::TpmChallengeKeyResult& result);

  // Runs the `callback` which resumes the navigation with the `result`
  // challenge response and removes the completed request from
  // `pending_challenges_`. In case the challenge response was not successfully
  // built, an empty challenge response will be used.
  void ReturnResult(uint64_t request_id,
                    AttestationCallback callback,
                    const ash::attestation::TpmChallengeKeyResult& result);

  const raw_ptr<Profile> profile_;

  uint64_t next_request_id_ = 0;
  base::flat_map<uint64_t,
                 std::unique_ptr<ash::attestation::TpmChallengeKeySubtle>>
      pending_key_preparations_;
  base::flat_map<uint64_t,
                 std::unique_ptr<ash::attestation::TpmChallengeKeyWithTimeout>>
      pending_challenges_;

  base::WeakPtrFactory<AshAttestationServiceImpl> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_IMPL_H_
