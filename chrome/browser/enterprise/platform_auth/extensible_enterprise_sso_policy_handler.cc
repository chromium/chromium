// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_policy_handler.h"

#include <memory>
#include <set>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_metadata.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace enterprise_auth {

constexpr char kAllIdentityProviders[] = "all";

ExtensibleEnterpriseSSOPolicyHandler::ExtensibleEnterpriseSSOPolicyHandler(
    const policy::Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          policy::key::kExtensibleEnterpriseSSOBlocklist,
          chrome_schema.GetKnownProperty(
              policy::key::kExtensibleEnterpriseSSOBlocklist),
          policy::SCHEMA_STRICT) {}

ExtensibleEnterpriseSSOPolicyHandler::~ExtensibleEnterpriseSSOPolicyHandler() =
    default;

void ExtensibleEnterpriseSSOPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs_value_map) {
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* policy_value = policies.GetValueUnsafe(policy_name());
  if (!policy_value) {
    return;
  }

  base::flat_set<std::string> supported_idps_set =
      base::MakeFlatSet<std::string>(
          GetSupportedIdentityProviders(), std::less<>(),
          [](const SsoExtensionMetadata& data) { return data.idp_name; });

  for (const base::Value& policy_item : policy_value->GetList()) {
    const std::string* item = policy_item.GetIfString();
    if (!item) {
      continue;
    }
    // Feature disabled if all idps are disabled.
    if (*item == enterprise_auth::kAllIdentityProviders) {
      supported_idps_set.clear();
      break;
    }
    // Remove blocklist idps from supported list.
    if (auto it = supported_idps_set.find(*item);
        it != supported_idps_set.end()) {
      supported_idps_set.erase(it);
    }
  }
  // Feature disabled if no idp is left.
  base::Value supported_idps(base::Value::Type::LIST);
  for (const auto& idp : supported_idps_set) {
    supported_idps.GetList().Append(base::Value(idp));
  }
  prefs_value_map->SetValue(prefs::kExtensibleEnterpriseSSOEnabled,
                            base::Value(supported_idps_set.empty() ? 0 : 1));
  prefs_value_map->SetValue(prefs::kExtensibleEnterpriseSSOEnabledIdps,
                            std::move(supported_idps));
}

}  // namespace enterprise_auth
