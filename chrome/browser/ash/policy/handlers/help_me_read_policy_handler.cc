// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/help_me_read_policy_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

HelpMeReadPolicyHandler::HelpMeReadPolicyHandler()
    : TypeCheckingPolicyHandler(key::kHelpMeReadSettings,
                                base::Value::Type::INTEGER) {}

void HelpMeReadPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                  PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(key::kHelpMeReadSettings, base::Value::Type::INTEGER);

  if (!value) {
    return;
  }

  // Refer to
  // components/policy/resources/templates/policy_definitions/GenerativeAI/HelpMeReadSettings.yaml
  // for the meanings of the following integer values.
  switch (value->GetInt()) {
    case 0:
      prefs->SetBoolean(ash::prefs::kHmrEnabled, true);
      prefs->SetBoolean(ash::prefs::kHmrFeedbackAllowed, true);
      prefs->SetBoolean(ash::prefs::kMagicBoostEnabled, true);
      break;
    case 1:
      prefs->SetBoolean(ash::prefs::kHmrEnabled, true);
      prefs->SetBoolean(ash::prefs::kHmrFeedbackAllowed, false);
      prefs->SetBoolean(ash::prefs::kMagicBoostEnabled, true);
      break;
    case 2:
      prefs->SetBoolean(ash::prefs::kHmrEnabled, false);
      prefs->SetBoolean(ash::prefs::kHmrFeedbackAllowed, false);
      prefs->SetBoolean(ash::prefs::kMagicBoostEnabled, true);
      break;
    default:
      break;
  }
}

}  // namespace policy
