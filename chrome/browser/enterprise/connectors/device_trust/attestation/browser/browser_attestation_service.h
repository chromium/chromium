// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_BROWSER_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_BROWSER_ATTESTATION_SERVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/task_runner.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/attester.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/google_keys.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"

namespace enterprise_connectors {

class KeyInfo;

// This class provides the methods needed in the handshake between Chrome, an
// IdP and Verified Access.
class BrowserAttestationService : public AttestationService {
 public:
  explicit BrowserAttestationService(
      std::vector<std::unique_ptr<Attester>> attesters);
  ~BrowserAttestationService() override;

  // AttestationService:
  void BuildChallengeResponseForVAChallenge(
      const std::string& challenge,
      base::Value::Dict signals,
      const std::set<DTCPolicyLevel>& levels,
      AttestationCallback callback) override;

 private:
  void OnChallengeParsed(AttestationCallback callback,
                         base::Value::Dict signals,
                         const std::string& serialized_signed_challenge);

  void OnChallengeValidated(const SignedData& signed_data,
                            base::Value::Dict signals,
                            const std::set<DTCPolicyLevel>& levels,
                            AttestationCallback callback,
                            bool is_va_challenge);

  void OnKeyInfoDecorated(const SignedData& signed_data,
                          const std::set<DTCPolicyLevel>& levels,
                          AttestationCallback callback,
                          std::unique_ptr<KeyInfo> key_info);

  void OnResponseCreated(const std::set<DTCPolicyLevel>& levels,
                         AttestationCallback callback,
                         std::optional<std::string> encrypted_response);

  void OnResponseSigned(AttestationCallback callback,
                        const std::string& encrypted_response,
                        std::unique_ptr<SignedData> signed_data);

  GoogleKeys google_keys_;

  // Array of attesters each of which carry out specific attestation actions
  // respective to their policy level.
  std::vector<std::unique_ptr<Attester>> attesters_;

  // Runner for tasks needed to be run in the background.
  scoped_refptr<base::TaskRunner> background_task_runner_;

  // Checker used to validate that non-background tasks should be
  // running on the original sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BrowserAttestationService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_BROWSER_ATTESTATION_SERVICE_H_
