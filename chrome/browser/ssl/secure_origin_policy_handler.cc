// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/secure_origin_policy_handler.h"

#include <string>

#include "base/check.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

SecureOriginPolicyHandler::SecureOriginPolicyHandler(const char* policy_name,
                                                     Schema schema)
    : SchemaValidatingPolicyHandler(policy_name,
                                    schema.GetKnownProperty(policy_name),
                                    SCHEMA_ALLOW_UNKNOWN) {
  DCHECK(policy_name == key::kUnsafelyTreatInsecureOriginAsSecure ||
         policy_name == key::kOverrideSecurityRestrictionsOnInsecureOrigin);
}

SecureOriginPolicyHandler::~SecureOriginPolicyHandler() = default;

void SecureOriginPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return;

  std::string pref_string;
  for (const auto& list_entry : value->GetList()) {
    if (!pref_string.empty())
      pref_string.append(",");
    pref_string.append(list_entry.GetString());
  }
  prefs->SetString(prefs::kUnsafelyTreatInsecureOriginAsSecure, pref_string);
}

}  // namespace policy
