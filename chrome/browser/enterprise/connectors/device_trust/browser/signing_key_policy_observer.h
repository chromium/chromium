// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_BROWSER_SIGNING_KEY_POLICY_OBSERVER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_BROWSER_SIGNING_KEY_POLICY_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

namespace enterprise_connectors {

class DeviceTrustKeyManager;

class SigningKeyPolicyObserver
    : public DeviceTrustConnectorService::PolicyObserver {
 public:
  explicit SigningKeyPolicyObserver(DeviceTrustKeyManager* browser_key_manager);
  ~SigningKeyPolicyObserver() override;

  // DeviceTrustConnectorService::PolicyObserver:
  void OnInlinePolicyEnabled(DTCPolicyLevel level) override;

 private:
  // Browser-process-level object that will outlive the current instance.
  const raw_ptr<DeviceTrustKeyManager> browser_key_manager_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_BROWSER_SIGNING_KEY_POLICY_OBSERVER_H_
