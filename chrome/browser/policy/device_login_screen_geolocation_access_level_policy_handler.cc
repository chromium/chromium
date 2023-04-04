// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_login_screen_geolocation_access_level_policy_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

DeviceLoginScreenGeolocationAccessLevelPolicyHandler::
    DeviceLoginScreenGeolocationAccessLevelPolicyHandler()
    : TypeCheckingPolicyHandler(key::kDeviceLoginScreenGeolocationAccessLevel,
                                base::Value::Type::INTEGER) {}

DeviceLoginScreenGeolocationAccessLevelPolicyHandler::
    ~DeviceLoginScreenGeolocationAccessLevelPolicyHandler() = default;

void DeviceLoginScreenGeolocationAccessLevelPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* const pvalue =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);

  if (pvalue) {
    CHECK(pvalue->is_int());
    int value = pvalue->GetInt();

    if (value < min_ || value > max_) {
      // Treat unknown policy values as kDisallowed.
      prefs->SetInteger(
          ash::prefs::kDeviceGeolocationAllowed,
          static_cast<int>(
              enterprise_management::
                  DeviceLoginScreenGeolocationAccessLevelProto::DISALLOWED));
    } else {
      prefs->SetInteger(ash::prefs::kDeviceGeolocationAllowed, value);
    }
  }
}

}  // namespace policy
