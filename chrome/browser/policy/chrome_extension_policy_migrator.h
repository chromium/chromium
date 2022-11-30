// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_EXTENSION_POLICY_MIGRATOR_H_
#define CHROME_BROWSER_POLICY_CHROME_EXTENSION_POLICY_MIGRATOR_H_

#include "components/policy/core/common/policy_migrator.h"

namespace policy {

// A helper class that migrates a deprecated policy to a new policy across
// domain boundaries, by setting up the new policy based on the old one. It can
// migrate a deprecated extension policy to a new Chrome policy.
//
// PolicyMigrator with chrome-specific helper functions.
class ChromeExtensionPolicyMigrator : public PolicyMigrator {
 public:
  ~ChromeExtensionPolicyMigrator() override {}

 protected:
  // Helper function intended for implementers who want to rename policies and
  // copy them from an extension domain to the Chrome domain. If one of the
  // Chrome domain policies is already set, it is not overridden.
  // hashed_extension_id is the SHA1-hashed extension_id to avoid the necessity
  // of possibly hardcoding extension ids.
  static void CopyPoliciesIfUnset(PolicyBundle* bundle,
                                  const std::string& hashed_extension_id,
                                  base::span<const Migration> migrations);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_EXTENSION_POLICY_MIGRATOR_H_
