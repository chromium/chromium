// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/app_launch_automation_policy_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/desks_storage/core/admin_template_service.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

AppLaunchAutomationPolicyHandler::AppLaunchAutomationPolicyHandler(
    const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kAppLaunchAutomation,
          chrome_schema.GetKnownProperty(key::kAppLaunchAutomation),
          SCHEMA_ALLOW_UNKNOWN) {}

AppLaunchAutomationPolicyHandler::~AppLaunchAutomationPolicyHandler() = default;

bool AppLaunchAutomationPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  // TODO(b/268538092): Validate app launch automation policy value.
  return true;
}

void AppLaunchAutomationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, /*errors=*/nullptr, &policy_value) ||
      !policy_value || !policy_value->is_list()) {
    return;
  }

  prefs->SetValue(ash::prefs::kAppLaunchAutomation,
                  base::Value::FromUniquePtrValue(std::move(policy_value)));
}

}  // namespace policy
