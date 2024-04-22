// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/annotations/blocklist_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/policy/annotations/annotation_control.h"
#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"

namespace policy {

// Setup annotation to policy mappings with some hand-picked network
// annotations. Annotations are keyed by hash codes which are generated at
// compile time. There is also a helper script to generate these hash codes at:
// `tools/traffic_annotation/scripts/auditor/README.md`
NetworkAnnotationBlocklistHandler::NetworkAnnotationBlocklistHandler() {
  // autofill_query
  // Note: This one is purposefully incorrect to allow for initial testing. It
  //       should have the same policies as 'autofill_upload' below.
  annotation_controls_["88863520"] =
      AnnotationControl().Add(key::kPasswordManagerEnabled, base::Value(false));

  // autofill_upload
  annotation_controls_["104798869"] =
      AnnotationControl()
          .Add(key::kPasswordManagerEnabled, base::Value(false))
          .Add(key::kAutofillAddressEnabled, base::Value(false))
          .Add(key::kAutofillCreditCardEnabled, base::Value(false));

  // calendar_get_events
  annotation_controls_["86429515"] = AnnotationControl().Add(
      key::kCalendarIntegrationEnabled, base::Value(false));

  // remoting_log_to_server
  annotation_controls_["99742369"] =
      AnnotationControl()
          .Add(key::kRemoteAccessHostAllowEnterpriseRemoteSupportConnections,
               base::Value(false))
          .Add(key::kRemoteAccessHostAllowRemoteSupportConnections,
               base::Value(false));
}

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
void NetworkAnnotationBlocklistHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  base::Value::Dict blocklist_prefs = base::Value::Dict();

  for (auto const& [hash_code, control] : annotation_controls_) {
    if (control.IsBlockedByPolicies(policies)) {
      blocklist_prefs.Set(hash_code, true);
    }
  }

  prefs->SetValue(prefs::kNetworkAnnotationBlocklist,
                  base::Value(std::move(blocklist_prefs)));
}

void NetworkAnnotationBlocklistHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkAnnotationBlocklist);
}

}  // namespace policy
