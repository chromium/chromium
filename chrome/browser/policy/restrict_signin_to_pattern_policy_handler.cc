// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/restrict_signin_to_pattern_policy_handler.h"

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

RestrictSigninToPatternPolicyHandler::RestrictSigninToPatternPolicyHandler()
    : SimplePolicyHandler(key::kRestrictSigninToPattern,
                          prefs::kGoogleServicesUsernamePattern,
                          base::Value::Type::STRING) {}

RestrictSigninToPatternPolicyHandler::~RestrictSigninToPatternPolicyHandler() =
    default;

bool RestrictSigninToPatternPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (!SimplePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::STRING);
  if (value && !signin::IsUsernamePatternValid(value->GetString()) && errors) {
    errors->AddError(policy_name(),
                     IDS_POLICY_RESTRICT_SIGNIN_TO_PATTERN_WARNING, {},
                     PolicyMap::MessageType::kWarning);
  }
  return true;
}

}  // namespace policy
