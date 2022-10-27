// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/restore_on_startup_policy_handler.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

RestoreOnStartupPolicyHandler::RestoreOnStartupPolicyHandler()
    : TypeCheckingPolicyHandler(key::kRestoreOnStartup,
                                base::Value::Type::INTEGER) {}

RestoreOnStartupPolicyHandler::~RestoreOnStartupPolicyHandler() {
}

void RestoreOnStartupPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* restore_on_startup_value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  if (restore_on_startup_value) {
    prefs->SetInteger(prefs::kRestoreOnStartup,
                      restore_on_startup_value->GetInt());
  }
}

bool RestoreOnStartupPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  const base::Value* restore_policy =
      policies.GetValue(key::kRestoreOnStartup, base::Value::Type::INTEGER);

  if (restore_policy) {
    switch (restore_policy->GetInt()) {
      case 0:  // Deprecated kPrefValueHomePage.
        errors->AddError(policy_name(), IDS_POLICY_VALUE_DEPRECATED);
        break;
      case SessionStartupPref::kPrefValueLast:
      case SessionStartupPref::kPrefValueLastAndURLs: {
        // If the "restore last session" policy is set, session cookies are
        // treated as permanent cookies and site data needed to restore the
        // session is not cleared so we have to warn the user in that case.
        const base::Value* cookies_policy = policies.GetValue(
            key::kCookiesSessionOnlyForUrls, base::Value::Type::LIST);
        if (cookies_policy && !cookies_policy->GetList().empty()) {
          errors->AddError(key::kCookiesSessionOnlyForUrls,
                           IDS_POLICY_OVERRIDDEN,
                           key::kRestoreOnStartup);
        }
        break;
      }
      case SessionStartupPref::kPrefValueURLs:
      case SessionStartupPref::kPrefValueNewTab:
        // No error
        break;
      default:
        errors->AddError(policy_name(), IDS_POLICY_OUT_OF_RANGE_ERROR,
                         base::NumberToString(restore_policy->GetInt()));
    }
  }
  return true;
}

}  // namespace policy
