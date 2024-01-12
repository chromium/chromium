// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_LOGIN_LOGIN_PROFILE_POLICY_PROVIDER_H_
#define CHROME_BROWSER_ASH_POLICY_LOGIN_LOGIN_PROFILE_POLICY_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

// Policy provider for the signin, lock screen and lock screen apps
// (`!ProfileHelper::IsUserProfile`) profiles. Since these profiles are not
// associated with any user, it does not receive regular user policy. However,
// several device policies that control features on the login/lock screen
// surface as user policies in the login and the lock screen app profile.
class LoginProfilePolicyProvider : public ConfigurationPolicyProvider,
                                   public PolicyService::Observer {
 public:
  explicit LoginProfilePolicyProvider(PolicyService* device_policy_service);

  LoginProfilePolicyProvider(const LoginProfilePolicyProvider&) = delete;
  LoginProfilePolicyProvider& operator=(const LoginProfilePolicyProvider&) =
      delete;

  ~LoginProfilePolicyProvider() override;

  // ConfigurationPolicyProvider:
  void Init(SchemaRegistry* registry) override;
  void Shutdown() override;
  void RefreshPolicies(PolicyFetchReason reason) override;

  // PolicyService::Observer:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;
  void OnPolicyServiceInitialized(PolicyDomain domain) override;

  void OnDevicePolicyRefreshDone();

 private:
  void UpdateFromDevicePolicy();

  raw_ptr<PolicyService> device_policy_service_;  // Not owned.

  bool waiting_for_device_policy_refresh_;

  base::WeakPtrFactory<LoginProfilePolicyProvider> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_LOGIN_LOGIN_PROFILE_POLICY_PROVIDER_H_
