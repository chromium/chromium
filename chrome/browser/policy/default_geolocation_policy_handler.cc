// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/default_geolocation_policy_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

DefaultGeolocationPolicyHandler::DefaultGeolocationPolicyHandler()
    : IntRangePolicyHandlerBase(key::kDefaultGeolocationSetting, 1, 3, false) {}

DefaultGeolocationPolicyHandler::~DefaultGeolocationPolicyHandler() {}

void DefaultGeolocationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies, PrefValueMap* prefs) {
  const base::Value* const value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, nullptr)
      && value_in_range == CONTENT_SETTING_BLOCK) {
    // CONTENT_SETTING_BLOCK = BlockGeolocation
    prefs->SetBoolean(arc::prefs::kArcLocationServiceEnabled, false);
  }
}

}  // namespace policy
