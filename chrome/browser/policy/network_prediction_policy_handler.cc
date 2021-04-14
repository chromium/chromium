// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/network_prediction_policy_handler.h"

#include "base/values.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

NetworkPredictionPolicyHandler::NetworkPredictionPolicyHandler() {
}

NetworkPredictionPolicyHandler::~NetworkPredictionPolicyHandler() {
}

bool NetworkPredictionPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  // Deprecated boolean preference.
  const base::Value* network_prediction_enabled =
      policies.GetValue(key::kDnsPrefetchingEnabled);
  // New enumerated preference.
  const base::Value* network_prediction_options =
      policies.GetValue(key::kNetworkPredictionOptions);

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
  const base::Value* network_prediction_options =
      policies.GetValue(key::kNetworkPredictionOptions);
  if (network_prediction_options && network_prediction_options->is_int()) {
    prefs->SetInteger(prefs::kNetworkPredictionOptions,
                      network_prediction_options->GetInt());
    return;
  }

  // Observe deprecated policy setting for compatibility.
  const base::Value* network_prediction_enabled =
      policies.GetValue(key::kDnsPrefetchingEnabled);
  bool bool_setting;
  if (network_prediction_enabled &&
      network_prediction_enabled->GetAsBoolean(&bool_setting)) {
    // Some predictive network actions, most notably prefetch, used to be
    // hardwired never to run on cellular network.  In order to retain this
    // behavior (unless explicitly overriden by kNetworkPredictionOptions),
    // kNetworkPredictionEnabled = true is translated to
    // kNetworkPredictionOptions = WIFI_ONLY.
    prefs->SetInteger(prefs::kNetworkPredictionOptions,
                      bool_setting
                          ? chrome_browser_net::NETWORK_PREDICTION_WIFI_ONLY
                          : chrome_browser_net::NETWORK_PREDICTION_NEVER);
  }
}

}  // namespace policy
