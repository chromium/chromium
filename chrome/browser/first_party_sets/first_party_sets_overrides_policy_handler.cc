// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_overrides_policy_handler.h"

#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace first_party_sets {

FirstPartySetsOverridesPolicyHandler::FirstPartySetsOverridesPolicyHandler(
    const policy::Schema& schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kFirstPartySetsOverrides,
          schema.GetKnownProperty(policy::key::kFirstPartySetsOverrides),
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN) {}

FirstPartySetsOverridesPolicyHandler::~FirstPartySetsOverridesPolicyHandler() =
    default;

void FirstPartySetsOverridesPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  NOTIMPLEMENTED();
}

}  // namespace first_party_sets
