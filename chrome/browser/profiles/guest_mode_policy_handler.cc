// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/guest_mode_policy_handler.h"

#include "base/values.h"
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

GuestModePolicyHandler::GuestModePolicyHandler()
    : TypeCheckingPolicyHandler(key::kBrowserGuestModeEnabled,
                                base::Value::Type::BOOLEAN) {}

GuestModePolicyHandler::~GuestModePolicyHandler() {}

void GuestModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                 PrefValueMap* prefs) {
  const base::Value* guest_mode_value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (guest_mode_value) {
    prefs->SetBoolean(prefs::kBrowserGuestModeEnabled,
                      guest_mode_value->GetBool());
    return;
  }
#if !BUILDFLAG(IS_CHROMEOS)
  // Disable guest mode by default if force signin is enabled.
  const base::Value* browser_signin_value =
      policies.GetValue(key::kBrowserSignin, base::Value::Type::INTEGER);
  if (browser_signin_value &&
      static_cast<BrowserSigninMode>(browser_signin_value->GetInt()) ==
          BrowserSigninMode::kForced) {
    prefs->SetBoolean(prefs::kBrowserGuestModeEnabled, false);
    return;
  }
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  const base::Value* force_signin_value =
      policies.GetValue(key::kForceBrowserSignin, base::Value::Type::BOOLEAN);
  if (!browser_signin_value && force_signin_value &&
      force_signin_value->GetBool()) {
    prefs->SetBoolean(prefs::kBrowserGuestModeEnabled, false);
  }
#endif
}

}  // namespace policy
