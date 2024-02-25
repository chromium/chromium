// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/default_geolocation_policy_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

DefaultGeolocationPolicyHandler::DefaultGeolocationPolicyHandler()
    : IntRangePolicyHandlerBase(key::kDefaultGeolocationSetting, 1, 3, false) {}

DefaultGeolocationPolicyHandler::~DefaultGeolocationPolicyHandler() {}

bool DefaultGeolocationPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* const value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, nullptr) &&
      value_in_range == CONTENT_SETTING_BLOCK) {
    // CONTENT_SETTING_BLOCK = BlockGeolocation
    if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
      // This policy should not affect the system location permission when
      // GoogleLocationServicesEnabled policy is set.
      if (policies.IsPolicySet(key::kGoogleLocationServicesEnabled)) {
        errors->AddError(policy_name(),
                         IDS_POLICY_DEFAULT_GEO_POLICY_ARC_CONFLICT,
                         key::kGoogleLocationServicesEnabled);
      }
    }
  }

  // Always continue to `ApplyPolicySettings()` which can handle invalid policy
  // values.
  return true;
}

void DefaultGeolocationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* const value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, nullptr)
      && value_in_range == CONTENT_SETTING_BLOCK) {
    // CONTENT_SETTING_BLOCK = BlockGeolocation
    if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
      // This policy should not affect the system location permission when
      // GoogleLocationServicesEnabled policy is set.
      if (policies.IsPolicySet(key::kGoogleLocationServicesEnabled)) {
        return;
      }

      // Keep the effective old behavior during the transition period.
      // Eventually, this policy will stop affecting the system location state
      // and this class will be removed.
      prefs->SetInteger(
          ash::prefs::kUserGeolocationAccessLevel,
          static_cast<int>(ash::GeolocationAccessLevel::kDisallowed));
    } else {
      prefs->SetBoolean(arc::prefs::kArcLocationServiceEnabled, false);
    }
  }
}

}  // namespace policy
