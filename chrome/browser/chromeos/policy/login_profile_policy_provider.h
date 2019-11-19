// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_LOGIN_PROFILE_POLICY_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_LOGIN_PROFILE_POLICY_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

// Policy provider for the login/lock screen app profile. Since these profiles
// are not associated with any user, it does not receive regular user policy.
// However, several device policies that control features on the login/lock
// screen surface as user policies in the login and the lock screen app profile.
class LoginProfilePolicyProvider : public ConfigurationPolicyProvider,
                                   public PolicyService::Observer {
 public:
  explicit LoginProfilePolicyProvider(PolicyService* device_policy_service);
  ~LoginProfilePolicyProvider() override;

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;
  void Shutdown() override;
  void RefreshPolicies() override;

  // PolicyService::Observer:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;
  void OnPolicyServiceInitialized(PolicyDomain domain) override;

  void OnDevicePolicyRefreshDone();

 private:
  void UpdateFromDevicePolicy();

  PolicyService* device_policy_service_;  // Not owned.

  bool waiting_for_device_policy_refresh_;

  base::WeakPtrFactory<LoginProfilePolicyProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginProfilePolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_LOGIN_PROFILE_POLICY_PROVIDER_H_
