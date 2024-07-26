// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_restriction_schedule_policy_handler.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy {

// static
void DeviceRestrictionSchedulePolicyHandler::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDeviceRestrictionSchedule);
}

}  // namespace policy
