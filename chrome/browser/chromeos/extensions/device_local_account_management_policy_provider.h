// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_MANAGEMENT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_MANAGEMENT_POLICY_PROVIDER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "extensions/browser/management_policy.h"

// TODO(crbug.com/1033508): Refactor this class, because the behavior of
// IsWhitelisted, and UserMayLoad are no longer used.

namespace chromeos {

// A managed policy for device-local accounts that ensures only extensions whose
// type or ID has been whitelisted for use in device-local accounts can be
// installed.
class DeviceLocalAccountManagementPolicyProvider
    : public extensions::ManagementPolicy::Provider {
 public:
  explicit DeviceLocalAccountManagementPolicyProvider(
      policy::DeviceLocalAccount::Type account_type);
  ~DeviceLocalAccountManagementPolicyProvider() override;

  // Used to check whether an extension is explicitly whitelisted.
  static bool IsWhitelisted(const std::string& extension_id);

  // extensions::ManagementPolicy::Provider:
  std::string GetDebugPolicyProviderName() const override;
  bool UserMayLoad(const extensions::Extension* extension,
                   std::u16string* error) const override;

 private:
  const policy::DeviceLocalAccount::Type account_type_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountManagementPolicyProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_MANAGEMENT_POLICY_PROVIDER_H_
