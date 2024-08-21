// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_custom_background_enabled_policy_handler.h"

#include "base/values.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

NtpCustomBackgroundEnabledPolicyHandler::
    NtpCustomBackgroundEnabledPolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kNTPCustomBackgroundEnabled,
                                base::Value::Type::BOOLEAN) {}

NtpCustomBackgroundEnabledPolicyHandler::
    ~NtpCustomBackgroundEnabledPolicyHandler() {}

void NtpCustomBackgroundEnabledPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (value && !value->GetBool()) {
    // NOTE: Using GetThemePrefNameInMigration is invalid since FeatureList is
    // not initialized yet (see crbug.com/361121492). As a workaround, both the
    // prefs are written to instead.
    prefs->SetValue(prefs::kNonSyncingNtpCustomBackgroundDictDoNotUse,
                    base::Value(base::Value::Type::DICT));
    prefs->SetValue(prefs::kNtpCustomBackgroundDictDoNotUse,
                    base::Value(base::Value::Type::DICT));
  }
}
