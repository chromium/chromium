// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/force_safe_search_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/safe_search_api/safe_search_util.h"

namespace policy {

ForceSafeSearchPolicyHandler::ForceSafeSearchPolicyHandler()
    : TypeCheckingPolicyHandler(key::kForceSafeSearch,
                                base::Value::Type::BOOLEAN) {}

ForceSafeSearchPolicyHandler::~ForceSafeSearchPolicyHandler() = default;

void ForceSafeSearchPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  // These three policies take precedence over |kForceGoogleSafeSearch|. If any
  // of them is set, their handlers will set the proper prefs.
  // https://crbug.com/476908, https://crbug.com/590478.
  if (policies.GetValue(key::kForceGoogleSafeSearch,
                        base::Value::Type::BOOLEAN) ||
      policies.GetValue(key::kForceYouTubeSafetyMode,
                        base::Value::Type::BOOLEAN) ||
      policies.GetValue(key::kForceYouTubeRestrict,
                        base::Value::Type::INTEGER)) {
    return;
  }
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (value) {
    prefs->SetValue(policy::policy_prefs::kForceGoogleSafeSearch,
                    value->Clone());

    // Note that ForceYouTubeRestrict is an int policy, we cannot simply deep
    // copy value, which is a boolean.
    if (value->is_bool()) {
      prefs->SetValue(
          policy::policy_prefs::kForceYouTubeRestrict,
          base::Value(value->GetBool()
                          ? safe_search_api::YOUTUBE_RESTRICT_MODERATE
                          : safe_search_api::YOUTUBE_RESTRICT_OFF));
    }
  }
}

}  // namespace policy
