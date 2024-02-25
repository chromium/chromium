// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/device_settings_cache.h"

#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/common/pref_names.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace em = enterprise_management;

namespace ash {

namespace device_settings_cache {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceSettingsCache, "invalid");
}

bool Store(const em::PolicyData& policy, PrefService* local_state) {
  if (!local_state)
    return false;

  local_state->SetString(prefs::kDeviceSettingsCache,
                         PolicyDataToString(policy));
  return true;
}

bool Retrieve(em::PolicyData* policy, PrefService* local_state) {
  if (local_state) {
    std::string encoded =
        local_state->GetString(prefs::kDeviceSettingsCache);
    std::string policy_string;
    if (!base::Base64Decode(encoded, &policy_string)) {
      // This is normal and happens on first boot.
      VLOG(1) << "Can't decode policy from base64.";
      return false;
    }
    return policy->ParseFromString(policy_string);
  }
  return false;
}

std::string PolicyDataToString(const em::PolicyData& policy) {
  const std::string policy_string = policy.SerializeAsString();
  return base::Base64Encode(policy_string);
}

}  // namespace device_settings_cache

}  // namespace ash
