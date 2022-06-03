// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/force_safe_search_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

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
  if (policies.GetValue(key::kForceGoogleSafeSearch) ||
      policies.GetValue(key::kForceYouTubeSafetyMode) ||
      policies.GetValue(key::kForceYouTubeRestrict)) {
    return;
  }
  const base::Value* value = policies.GetValue(policy_name());
  if (value) {
    prefs->SetValue(prefs::kForceGoogleSafeSearch, value->Clone());

    // Note that ForceYouTubeRestrict is an int policy, we cannot simply deep
    // copy value, which is a boolean.
    if (value->is_bool()) {
      prefs->SetValue(
          prefs::kForceYouTubeRestrict,
          base::Value(value->GetBool()
                          ? safe_search_util::YOUTUBE_RESTRICT_MODERATE
                          : safe_search_util::YOUTUBE_RESTRICT_OFF));
    }
  }
}

}  // namespace policy
