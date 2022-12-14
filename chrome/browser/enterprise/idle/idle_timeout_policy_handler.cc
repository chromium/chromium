// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/idle_timeout_policy_handler.h"

#include <string>

#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/enterprise/idle/action.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace enterprise_idle {

namespace {

// If `other_policy_name` is unset, adds an error to `errors` and returns false.
bool CheckOtherPolicySet(const policy::PolicyMap& policies,
                         const std::string& this_policy_name,
                         const std::string& other_policy_name,
                         policy::PolicyErrorMap* errors) {
  if (policies.GetValueUnsafe(other_policy_name))
    return true;

  errors->AddError(this_policy_name, IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
                   other_policy_name);
  return false;
}

}  // namespace

IdleTimeoutPolicyHandler::IdleTimeoutPolicyHandler()
    : policy::IntRangePolicyHandler(policy::key::kIdleTimeout,
                                    prefs::kIdleTimeout,
                                    1,
                                    INT_MAX,
                                    true) {}

IdleTimeoutPolicyHandler::~IdleTimeoutPolicyHandler() = default;

void IdleTimeoutPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy::key::kIdleTimeout, base::Value::Type::INTEGER);
  DCHECK(value);

  // Apply a minimum of 1.
  base::TimeDelta time_delta = base::Minutes(std::max(value->GetInt(), 1));
  prefs->SetValue(prefs::kIdleTimeout, base::TimeDeltaToValue(time_delta));
}

bool IdleTimeoutPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Nothing to do if unset.
  if (!policies.GetValueUnsafe(policy::key::kIdleTimeout))
    return false;

  // Check that it's an integer, and that it's >= 1.
  if (!policy::IntRangePolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  // If IdleTimeoutActions is unset, add an error and do nothing.
  if (!CheckOtherPolicySet(policies, policy::key::kIdleTimeout,
                           policy::key::kIdleTimeoutActions, errors)) {
    return false;
  }

  return true;
}

IdleTimeoutActionsPolicyHandler::IdleTimeoutActionsPolicyHandler(
    policy::Schema schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kIdleTimeoutActions,
          schema.GetKnownProperty(policy::key::kIdleTimeoutActions),
          policy::SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY) {}

IdleTimeoutActionsPolicyHandler::~IdleTimeoutActionsPolicyHandler() = default;

void IdleTimeoutActionsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* policy_value = policies.GetValue(
      policy::key::kIdleTimeoutActions, base::Value::Type::LIST);
  DCHECK(policy_value);

  // Convert strings to integers (from the ActionType enum).
  base::Value::List converted_actions;
  for (const base::Value& action : policy_value->GetList()) {
    if (!action.is_string())
      continue;
    const std::string& name = action.GetString();
    if (name == "close_browsers")
      converted_actions.Append(static_cast<int>(ActionType::kCloseBrowsers));
    else if (name == "show_profile_picker")
      converted_actions.Append(
          static_cast<int>(ActionType::kShowProfilePicker));
    // Silently drop unsupported values.
  }
  prefs->SetValue(prefs::kIdleTimeoutActions,
                  base::Value(std::move(converted_actions)));
}

bool IdleTimeoutActionsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Nothing to do if unset.
  if (!policies.GetValueUnsafe(policy::key::kIdleTimeoutActions))
    return false;

  // Check that it's a list of strings, and that they're supported enum values.
  // Unsupported enum values are dropped, with a warning on chrome://policy.
  if (!policy::SchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                  errors)) {
    return false;
  }

  // If IdleTimeout is unset, add an error and do nothing.
  if (!CheckOtherPolicySet(policies, policy::key::kIdleTimeoutActions,
                           policy::key::kIdleTimeout, errors)) {
    return false;
  }

  return true;
}
}  // namespace enterprise_idle
