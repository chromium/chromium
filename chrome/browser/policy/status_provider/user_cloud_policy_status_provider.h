// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_USER_CLOUD_POLICY_STATUS_PROVIDER_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_USER_CLOUD_POLICY_STATUS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/policy/status_provider/cloud_policy_core_status_provider.h"

class Profile;

namespace policy {
class CloudPolicyCore;
}  // namespace policy

// A cloud policy status provider for user policy.
class UserCloudPolicyStatusProvider : public CloudPolicyCoreStatusProvider {
 public:
  explicit UserCloudPolicyStatusProvider(policy::CloudPolicyCore* core,
                                         Profile* profile);

  UserCloudPolicyStatusProvider(const UserCloudPolicyStatusProvider&) = delete;
  UserCloudPolicyStatusProvider& operator=(
      const UserCloudPolicyStatusProvider&) = delete;

  ~UserCloudPolicyStatusProvider() override;

  // CloudPolicyCoreStatusProvider implementation.
  base::Value::Dict GetStatus() override;

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_USER_CLOUD_POLICY_STATUS_PROVIDER_H_
