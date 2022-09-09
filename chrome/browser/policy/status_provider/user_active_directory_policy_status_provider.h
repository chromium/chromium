// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_USER_ACTIVE_DIRECTORY_POLICY_STATUS_PROVIDER_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_USER_ACTIVE_DIRECTORY_POLICY_STATUS_PROVIDER_H_

#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

class Profile;

namespace policy {
class ActiveDirectoryPolicyManager;
}  // namespace policy

// Provides status for Active Directory user policy.
class UserActiveDirectoryPolicyStatusProvider
    : public policy::PolicyStatusProvider,
      public policy::CloudPolicyStore::Observer {
 public:
  explicit UserActiveDirectoryPolicyStatusProvider(
      policy::ActiveDirectoryPolicyManager* policy_manager,
      Profile* profile);

  UserActiveDirectoryPolicyStatusProvider(
      const UserActiveDirectoryPolicyStatusProvider&) = delete;
  UserActiveDirectoryPolicyStatusProvider& operator=(
      const UserActiveDirectoryPolicyStatusProvider&) = delete;

  ~UserActiveDirectoryPolicyStatusProvider() override;

  // PolicyStatusProvider implementation.
  base::Value::Dict GetStatus() override;

  // policy::CloudPolicyStore::Observer implementation.
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

 private:
  policy::ActiveDirectoryPolicyManager* const policy_manager_;  // not owned.
  Profile* profile_;
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_USER_ACTIVE_DIRECTORY_POLICY_STATUS_PROVIDER_H_
