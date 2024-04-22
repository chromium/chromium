// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/annotations/blocklist_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/common/pref_names.h"

namespace policy {

NetworkAnnotationBlocklistHandler::NetworkAnnotationBlocklistHandler() =
    default;
NetworkAnnotationBlocklistHandler::~NetworkAnnotationBlocklistHandler() =
    default;

// Only check policies if `kNetworkAnnotationMonitoring` is enabled.
// ApplyPolicySettings(...) is only called when this function returns true.
bool NetworkAnnotationBlocklistHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (base::FeatureList::IsEnabled(features::kNetworkAnnotationMonitoring)) {
    return true;
  }
  return false;
}

// Check policy values to determine which network annotations should be
// disabled.
// TODO(b/330181218): We hardcoded one annotation initially. Add logic to check
// all annotations we are interested in.
void NetworkAnnotationBlocklistHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const std::string kAutofillQueryHashCode = "88863520";  // autofill_query
  const std::string kAutofillQueryPolicy = "PasswordManagerEnabled";

  base::Value::Dict blocklist_prefs = base::Value::Dict();
  if (IsPolicyDisabled(policies, kAutofillQueryPolicy)) {
    blocklist_prefs.Set(kAutofillQueryHashCode, true);
  }
  prefs->SetValue(prefs::kNetworkAnnotationBlocklist,
                  base::Value(std::move(blocklist_prefs)));
}

void NetworkAnnotationBlocklistHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkAnnotationBlocklist);
}

bool NetworkAnnotationBlocklistHandler::IsPolicyDisabled(
    const PolicyMap& policies,
    std::string policy_name) {
  const base::Value* current_policy_value =
      policies.GetValue(policy_name, base::Value::Type::BOOLEAN);
  if (current_policy_value != nullptr && !current_policy_value->GetBool()) {
    return true;
  }
  return false;
}

}  // namespace policy
