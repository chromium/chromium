// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_MANAGEMENT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_MANAGEMENT_POLICY_PROVIDER_H_

#include <string>

#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "extensions/browser/management_policy.h"

namespace chromeos {

// A managed policy for device-local accounts that ensures only extensions whose
// type or ID has been whitelisted for use in device-local accounts can be
// installed.
class DeviceLocalAccountManagementPolicyProvider
    : public extensions::ManagementPolicy::Provider {
 public:
  explicit DeviceLocalAccountManagementPolicyProvider(
      policy::DeviceLocalAccountType account_type);

  DeviceLocalAccountManagementPolicyProvider(
      const DeviceLocalAccountManagementPolicyProvider&) = delete;
  DeviceLocalAccountManagementPolicyProvider& operator=(
      const DeviceLocalAccountManagementPolicyProvider&) = delete;

  ~DeviceLocalAccountManagementPolicyProvider() override;

  // extensions::ManagementPolicy::Provider:
  std::string GetDebugPolicyProviderName() const override;
  bool UserMayLoad(const extensions::Extension* extension,
                   std::u16string* error) const override;

 private:
  const policy::DeviceLocalAccountType account_type_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_MANAGEMENT_POLICY_PROVIDER_H_
