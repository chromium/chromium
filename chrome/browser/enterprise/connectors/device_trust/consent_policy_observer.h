// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_CONSENT_POLICY_OBSERVER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_CONSENT_POLICY_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

namespace device_signals {
class UserPermissionService;
}  // namespace device_signals

namespace enterprise_connectors {

class ConsentPolicyObserver
    : public DeviceTrustConnectorService::PolicyObserver {
 public:
  explicit ConsentPolicyObserver(
      base::WeakPtr<device_signals::UserPermissionService>
          user_permission_service);
  ~ConsentPolicyObserver() override;

  // DeviceTrustConnectorService::PolicyObserver:
  void OnInlinePolicyDisabled(DTCPolicyLevel level) override;

 private:
  // Using a WeakPtr as this profile-keyed service will likely be destroyed
  // right before the owner of the current observer instance. By using a
  // WeakPtr, we are avoiding any use-after-free issues.
  const base::WeakPtr<device_signals::UserPermissionService>
      user_permission_service_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_CONSENT_POLICY_OBSERVER_H_
