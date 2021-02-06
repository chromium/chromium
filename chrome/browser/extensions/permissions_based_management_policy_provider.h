// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PERMISSIONS_BASED_MANAGEMENT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_PERMISSIONS_BASED_MANAGEMENT_POLICY_PROVIDER_H_

#include <string>

#include "base/macros.h"
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
  ~PermissionsBasedManagementPolicyProvider() override;

  // ManagementPolicy::Provider implementation.
  std::string GetDebugPolicyProviderName() const override;
  bool UserMayLoad(const Extension* extension,
                           base::string16* error) const override;

 private:
  ExtensionManagement* settings_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsBasedManagementPolicyProvider);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PERMISSIONS_BASED_MANAGEMENT_POLICY_PROVIDER_H_
