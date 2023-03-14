// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_extension_policy_migrator.h"

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/common/hashed_extension_id.h"

namespace policy {

void ChromeExtensionPolicyMigrator::CopyPoliciesIfUnset(
    PolicyBundle* bundle,
    const std::string& hashed_extension_id,
    base::span<const Migration> migrations) {
  // HashedExtensionId gives an all-uppercase output, so make sure the input is
  // all uppercase.
  std::string hashed_extension_id_uppercase = hashed_extension_id;
  base::ranges::transform(hashed_extension_id_uppercase,
                          hashed_extension_id_uppercase.begin(), ::toupper);

  // Look for an extension with this hash.
  PolicyMap* extension_map = nullptr;
  for (auto& policy : *bundle) {
    const PolicyNamespace& policy_namespace = policy.first;

    if (policy_namespace.domain == PolicyDomain::POLICY_DOMAIN_EXTENSIONS &&
        extensions::HashedExtensionId(policy_namespace.component_id).value() ==
            hashed_extension_id_uppercase) {
      extension_map = &policy.second;
      break;
    }
  }

  if (extension_map == nullptr || extension_map->empty()) {
    VLOG(3) << "Extension with hashed id '" << hashed_extension_id_uppercase
            << "' not installed. Aborting policy migration.";
    return;
  }

  PolicyMap& chrome_map = bundle->Get(PolicyNamespace(
      PolicyDomain::POLICY_DOMAIN_CHROME, /* component_id */ std::string()));

  for (const auto& migration : migrations) {
    CopyPolicyIfUnset(*extension_map, &chrome_map, migration);
  }
}

}  // namespace policy
