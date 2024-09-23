// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/contextual_google_integrations_policies_handler.h"

#include <memory>
#include <string_view>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {
namespace {

constexpr auto kKnownGoogleIntegrations =
    base::MakeFixedFlatSet<std::string_view>(
        {ash::prefs::kGoogleCalendarIntegrationName,
         ash::prefs::kGoogleClassroomIntegrationName,
         ash::prefs::kGoogleTasksIntegrationName,
         ash::prefs::kChromeSyncIntegrationName,
         ash::prefs::kGoogleDriveIntegrationName,
         ash::prefs::kWeatherIntegrationName});

}  // namespace

ContextualGoogleIntegrationsPoliciesHandler::
    ContextualGoogleIntegrationsPoliciesHandler(const Schema& schema)
    : SchemaValidatingPolicyHandler(
          key::kContextualGoogleIntegrationsConfiguration,
          schema.GetKnownProperty(
              key::kContextualGoogleIntegrationsConfiguration),
          policy::SchemaOnErrorStrategy::
              SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY) {}

ContextualGoogleIntegrationsPoliciesHandler::
    ~ContextualGoogleIntegrationsPoliciesHandler() = default;

bool ContextualGoogleIntegrationsPoliciesHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return TypeCheckingPolicyHandler::CheckPolicySettings(
             key::kContextualGoogleIntegrationsEnabled,
             base::Value::Type::BOOLEAN,
             policies.Get(key::kContextualGoogleIntegrationsEnabled), errors) &&
         SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors);
}

void ContextualGoogleIntegrationsPoliciesHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const auto* const umbrella_policy_value = policies.GetValue(
      key::kContextualGoogleIntegrationsEnabled, base::Value::Type::BOOLEAN);
  const bool is_umbrella_policy_enabled =
      !umbrella_policy_value || umbrella_policy_value->GetBool();
  if (!is_umbrella_policy_enabled) {
    prefs->SetValue(ash::prefs::kContextualGoogleIntegrationsConfiguration,
                    base::Value(base::Value::List()));
    return;
  }

  if (policies.IsPolicySet(key::kContextualGoogleIntegrationsConfiguration)) {
    std::unique_ptr<base::Value> value;
    CheckAndGetValue(policies, /*errors=*/nullptr, &value);
    CHECK(value);

    base::Value::List enabled_integrations;
    for (const auto& integration_value : value->GetList()) {
      if (integration_value.is_string() &&
          kKnownGoogleIntegrations.contains(integration_value.GetString())) {
        enabled_integrations.Append(integration_value.Clone());
      }
    }
    prefs->SetValue(ash::prefs::kContextualGoogleIntegrationsConfiguration,
                    base::Value(std::move(enabled_integrations)));
  }
}

}  // namespace policy
