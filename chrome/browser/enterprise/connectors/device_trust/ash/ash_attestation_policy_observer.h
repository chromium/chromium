// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ASH_ASH_ATTESTATION_POLICY_OBSERVER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ASH_ASH_ATTESTATION_POLICY_OBSERVER_H_

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

namespace enterprise_connectors {

class AshAttestationService;

// This class is in charge of initiating the key preparation when DTC is
// enabled.
class AshAttestationPolicyObserver
    : public DeviceTrustConnectorService::PolicyObserver {
 public:
  explicit AshAttestationPolicyObserver(
      base::WeakPtr<AshAttestationService> attestation_service);
  ~AshAttestationPolicyObserver() override;

  // DeviceTrustConnectorService::PolicyObserver:
  void OnInlinePolicyEnabled(DTCPolicyLevel level) override;

 private:
  const base::WeakPtr<AshAttestationService> attestation_service_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ASH_ASH_ATTESTATION_POLICY_OBSERVER_H_
