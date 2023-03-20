// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/force_youtube_safety_mode_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/safe_search_api/safe_search_util.h"

namespace policy {

ForceYouTubeSafetyModePolicyHandler::ForceYouTubeSafetyModePolicyHandler()
    : TypeCheckingPolicyHandler(key::kForceYouTubeSafetyMode,
                                base::Value::Type::BOOLEAN) {}

ForceYouTubeSafetyModePolicyHandler::~ForceYouTubeSafetyModePolicyHandler() =
    default;

void ForceYouTubeSafetyModePolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  // If only the deprecated ForceYouTubeSafetyMode policy is set,
  // but not ForceYouTubeRestrict, set ForceYouTubeRestrict to Moderate.
  if (policies.GetValue(key::kForceYouTubeRestrict, base::Value::Type::INTEGER))
    return;

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (value) {
    prefs->SetValue(policy_prefs::kForceYouTubeRestrict,
                    base::Value(value->GetBool()
                                    ? safe_search_api::YOUTUBE_RESTRICT_MODERATE
                                    : safe_search_api::YOUTUBE_RESTRICT_OFF));
  }
}

}  // namespace policy
