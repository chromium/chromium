// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_policy_handler.h"

#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy {

DeviceWeeklyScheduledSuspendPolicyHandler::
    DeviceWeeklyScheduledSuspendPolicyHandler(const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kDeviceWeeklyScheduledSuspend,
          chrome_schema.GetKnownProperty(key::kDeviceWeeklyScheduledSuspend),
          SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN) {}

DeviceWeeklyScheduledSuspendPolicyHandler::
    ~DeviceWeeklyScheduledSuspendPolicyHandler() = default;

// static
void DeviceWeeklyScheduledSuspendPolicyHandler::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(::prefs::kDeviceWeeklyScheduledSuspend);
}

// ConfigurationPolicyHandler methods:
bool DeviceWeeklyScheduledSuspendPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* device_weekly_scheduled_suspend =
      policies.GetValueUnsafe(key::kDeviceWeeklyScheduledSuspend);
  if (!device_weekly_scheduled_suspend) {
    return true;
  }

  // TODO(b/322341636): Validate that the schedule contains entries with valid
  // start/end times, and that it does not contain overlapped entries.

  return true;
}

void DeviceWeeklyScheduledSuspendPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const policy::PolicyMap::Entry* policy =
      policies.Get(key::kDeviceWeeklyScheduledSuspend);
  if (!policy) {
    return;
  }

  if (const base::Value* value = policy->value_unsafe(); value) {
    prefs->SetValue(::prefs::kDeviceWeeklyScheduledSuspend, value->Clone());
  }
}

}  // namespace policy
