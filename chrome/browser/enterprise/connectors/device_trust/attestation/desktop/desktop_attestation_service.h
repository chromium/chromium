// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_DESKTOP_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_DESKTOP_ATTESTATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/task_runner.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/google_keys.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {
class BrowserDMTokenStorage;
}  // namespace policy

namespace enterprise_connectors {

class DeviceTrustKeyManager;

// This class is in charge of handling the key pair used for attestation. Also
// provides the methods needed in the handshake between Chrome, an IdP and
// Verified Access.
class DesktopAttestationService : public AttestationService {
 public:
  explicit DesktopAttestationService(
      policy::BrowserDMTokenStorage* dm_token_storage,
      DeviceTrustKeyManager* key_manager);
  ~DesktopAttestationService() override;

  // AttestationService:
  void BuildChallengeResponseForVAChallenge(
      const std::string& challenge,
      base::Value::Dict signals,
      AttestationCallback callback) override;

 private:
  void OnChallengeParsed(AttestationCallback callback,
                         base::Value::Dict signals,
                         const std::string& serialized_signed_challenge);

  void OnPublicKeyExported(const std::string& serialized_signed_challenge,
                           base::Value::Dict signals,
                           AttestationCallback callback,
                           absl::optional<std::string> exported_key);

  void OnChallengeValidated(const SignedData& signed_data,
                            const std::string& exported_public_key,
                            base::Value::Dict signals,
                            AttestationCallback callback,
                            bool is_va_challenge);

  void OnResponseCreated(AttestationCallback callback,
                         absl::optional<std::string> serialized_response);

  void OnResponseSigned(
      AttestationCallback callback,
      const std::string& serialized_response,
      absl::optional<std::vector<uint8_t>> encrypted_response);

  GoogleKeys google_keys_;

  // Helper for handling DMToken and DeviceID.
  const raw_ptr<policy::BrowserDMTokenStorage> dm_token_storage_;

  // Owned by the CBCMController, which is eventually owned by the browser
  // process. Since the current service is owned at the profile level, this
  // respects the browser shutdown sequence.
  raw_ptr<DeviceTrustKeyManager> key_manager_;

  // Runner for tasks needed to be run in the background.
  scoped_refptr<base::TaskRunner> background_task_runner_;

  // Checker used to validate that non-background tasks should be
  // running on the original sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DesktopAttestationService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_DESKTOP_ATTESTATION_SERVICE_H_
