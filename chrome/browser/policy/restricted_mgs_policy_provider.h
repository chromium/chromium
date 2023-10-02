// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_RESTRICTED_MGS_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_RESTRICTED_MGS_POLICY_PROVIDER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/configuration_policy_provider.h"

namespace policy {

// Policy provider for a restricted Managed Guest Session (MGS). The provider
// applies restrictions on a list of policies if the
// DeviceRestrictedManagedGuestSessionEnabled policy is enabled when the session
// starts. The restricted MGS is only used in the context of shared sessions. If
// shared sessions are enabled, the session will not launch without the enabled
// DeviceRestrictedManagedGuestSessionEnabled policy. We do not check for
// updates of the policy in session because if it is disabled during a running
// shared session, it will deny the unlock of the shared session.
class RestrictedMGSPolicyProvider : public ConfigurationPolicyProvider {
 public:
  RestrictedMGSPolicyProvider();

  RestrictedMGSPolicyProvider(const RestrictedMGSPolicyProvider&) = delete;
  RestrictedMGSPolicyProvider& operator=(const RestrictedMGSPolicyProvider&) =
      delete;

  ~RestrictedMGSPolicyProvider() override;

  // ConfigurationPolicyProvider:
  void RefreshPolicies(PolicyFetchReason reason) override;

  // Factory function to create and initialize a provider. Returns nullptr if we
  // are not in a Managed Guest Session.
  static std::unique_ptr<RestrictedMGSPolicyProvider> Create();

 private:
  // Gets the current PolicyBundle and applies restrictions in case the
  // DeviceRestrictedManagedGuestSessionEnabled policy is enabled.
  void UpdatePolicyBundle();

  void ApplyRestrictedManagedGuestSessionOverride(PolicyMap* chrome_policy);

  base::WeakPtrFactory<RestrictedMGSPolicyProvider> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_RESTRICTED_MGS_POLICY_PROVIDER_H_
