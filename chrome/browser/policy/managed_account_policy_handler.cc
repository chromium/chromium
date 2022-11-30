// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/managed_account_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

ManagedAccountRestrictionsPolicyHandler::
    ManagedAccountRestrictionsPolicyHandler(policy::Schema schema)
    : policy::SimpleSchemaValidatingPolicyHandler(
          policy::key::kManagedAccountsSigninRestriction,
          prefs::kManagedAccountsSigninRestriction,
          schema,
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

ManagedAccountRestrictionsPolicyHandler::
    ~ManagedAccountRestrictionsPolicyHandler() = default;

void ManagedAccountRestrictionsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  SimpleSchemaValidatingPolicyHandler::ApplyPolicySettings(policies, prefs);
  const auto* policy = policies.Get(policy_name());
  if (policy) {
    bool user_cloud_policy = policy->scope == policy::POLICY_SCOPE_USER &&
                             policy->source == policy::POLICY_SOURCE_CLOUD;
    // Only user cloud policies do not target the whole machine.
    prefs->SetBoolean(prefs::kManagedAccountsSigninRestrictionScopeMachine,
                      !user_cloud_policy);
  }
}
