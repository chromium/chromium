// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/network_prediction_policy_handler.h"

#include "base/values.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

bool NetworkPredictionPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  // Deprecated boolean preference.
  const base::Value* network_prediction_enabled =
      policies.GetValueUnsafe(key::kDnsPrefetchingEnabled);
  // New enumerated preference.
  const base::Value* network_prediction_options =
      policies.GetValueUnsafe(key::kNetworkPredictionOptions);

  if (network_prediction_enabled && !network_prediction_enabled->is_bool()) {
    errors->AddError(key::kDnsPrefetchingEnabled, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::BOOLEAN));
  }

  if (network_prediction_options && !network_prediction_options->is_int()) {
    errors->AddError(key::kNetworkPredictionOptions, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::INTEGER));
  }

  if (network_prediction_enabled && network_prediction_options) {
    errors->AddError(key::kDnsPrefetchingEnabled,
                     IDS_POLICY_OVERRIDDEN,
                     key::kNetworkPredictionOptions);
  }

  return true;
}

void NetworkPredictionPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* network_prediction_options = policies.GetValue(
      key::kNetworkPredictionOptions, base::Value::Type::INTEGER);
  if (network_prediction_options) {
    prefs->SetInteger(prefetch::prefs::kNetworkPredictionOptions,
                      network_prediction_options->GetInt());
    return;
  }

  // Observe deprecated policy setting for compatibility.
  const base::Value* network_prediction_enabled = policies.GetValue(
      key::kDnsPrefetchingEnabled, base::Value::Type::BOOLEAN);
  if (network_prediction_enabled) {
    prefetch::NetworkPredictionOptions setting =
        network_prediction_enabled->GetBool()
            ? prefetch::NetworkPredictionOptions::kWifiOnlyDeprecated
            : prefetch::NetworkPredictionOptions::kDisabled;
    prefs->SetInteger(prefetch::prefs::kNetworkPredictionOptions,
                      static_cast<int>(setting));
  }
}

}  // namespace policy
