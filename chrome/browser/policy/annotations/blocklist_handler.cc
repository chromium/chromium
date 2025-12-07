// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/annotations/blocklist_handler.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/policy/annotations/annotation_control.h"
#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"

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
void NetworkAnnotationBlocklistHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::TimeTicks start_time = base::TimeTicks::Now();

  base::Value::Dict blocklist_prefs = base::Value::Dict();

  for (auto const& [hash_code, control] :
       annotation_control_provider_.GetControls()) {
    if (control.IsBlockedByPolicies(policies)) {
      blocklist_prefs.Set(hash_code, true);
    }
  }

  prefs->SetValue(prefs::kNetworkAnnotationBlocklist,
                  base::Value(std::move(blocklist_prefs)));

  // Publish time metric for this handler.
  UMA_HISTOGRAM_TIMES("ChromeOS.Regmon.PolicyHandlerTime",
                      base::TimeTicks::Now() - start_time);
}

void NetworkAnnotationBlocklistHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkAnnotationBlocklist);
}

}  // namespace policy
