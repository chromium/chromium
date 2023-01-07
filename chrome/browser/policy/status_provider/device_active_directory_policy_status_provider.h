// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_DEVICE_ACTIVE_DIRECTORY_POLICY_STATUS_PROVIDER_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_DEVICE_ACTIVE_DIRECTORY_POLICY_STATUS_PROVIDER_H_

#include <string>

#include "chrome/browser/policy/status_provider/user_active_directory_policy_status_provider.h"

namespace policy {
class ActiveDirectoryPolicyManager;
}  // namespace policy

// Provides status for Device Active Directory policy.
class DeviceActiveDirectoryPolicyStatusProvider
    : public UserActiveDirectoryPolicyStatusProvider {
 public:
  DeviceActiveDirectoryPolicyStatusProvider(
      policy::ActiveDirectoryPolicyManager* policy_manager,
      const std::string& enterprise_domain_manager);

  DeviceActiveDirectoryPolicyStatusProvider(
      const DeviceActiveDirectoryPolicyStatusProvider&) = delete;
  DeviceActiveDirectoryPolicyStatusProvider& operator=(
      const DeviceActiveDirectoryPolicyStatusProvider&) = delete;

  ~DeviceActiveDirectoryPolicyStatusProvider() override = default;

  // PolicyStatusProvider implementation.
  base::Value::Dict GetStatus() override;

 private:
  std::string enterprise_domain_manager_;
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_DEVICE_ACTIVE_DIRECTORY_POLICY_STATUS_PROVIDER_H_
