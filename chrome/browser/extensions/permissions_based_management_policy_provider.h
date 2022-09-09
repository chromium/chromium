// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PERMISSIONS_BASED_MANAGEMENT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_PERMISSIONS_BASED_MANAGEMENT_POLICY_PROVIDER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "extensions/browser/management_policy.h"

namespace extensions {

class Extension;
class ExtensionManagement;

// A ManagementPolicyProvider controlled by enterprise policy, and prevent
// certain extensions from loading by checking if its permission data conflicts
// with policy or not.
class PermissionsBasedManagementPolicyProvider
    : public ManagementPolicy::Provider {
 public:
  explicit PermissionsBasedManagementPolicyProvider(
      ExtensionManagement* settings);

  PermissionsBasedManagementPolicyProvider(
      const PermissionsBasedManagementPolicyProvider&) = delete;
  PermissionsBasedManagementPolicyProvider& operator=(
      const PermissionsBasedManagementPolicyProvider&) = delete;

  ~PermissionsBasedManagementPolicyProvider() override;

  // ManagementPolicy::Provider implementation.
  std::string GetDebugPolicyProviderName() const override;
  bool UserMayLoad(const Extension* extension,
                   std::u16string* error) const override;

 private:
  raw_ptr<ExtensionManagement> settings_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PERMISSIONS_BASED_MANAGEMENT_POLICY_PROVIDER_H_
