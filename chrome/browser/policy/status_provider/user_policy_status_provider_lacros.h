// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_USER_POLICY_STATUS_PROVIDER_LACROS_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_USER_POLICY_STATUS_PROVIDER_LACROS_H_

#include "components/policy/core/browser/webui/policy_status_provider.h"

class Profile;

namespace policy {
class PolicyLoaderLacros;
}  // namespace policy

// A cloud policy status provider for device account.
class UserPolicyStatusProviderLacros : public policy::PolicyStatusProvider {
 public:
  UserPolicyStatusProviderLacros(policy::PolicyLoaderLacros* loader,
                                 Profile* profile);

  UserPolicyStatusProviderLacros(const UserPolicyStatusProviderLacros&) =
      delete;
  UserPolicyStatusProviderLacros& operator=(
      const UserPolicyStatusProviderLacros&) = delete;

  ~UserPolicyStatusProviderLacros() override;

  // CloudPolicyCoreStatusProvider implementation.
  base::Value::Dict GetStatus() override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<policy::PolicyLoaderLacros> loader_;
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_USER_POLICY_STATUS_PROVIDER_LACROS_H_
