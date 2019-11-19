// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_signin_policy_handler.h"

#include <memory>

#include "base/syslog_logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"

namespace policy {
BrowserSigninPolicyHandler::BrowserSigninPolicyHandler(Schema chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kBrowserSignin,
          chrome_schema.GetKnownProperty(key::kBrowserSignin),
          SCHEMA_ALLOW_UNKNOWN) {}

BrowserSigninPolicyHandler::~BrowserSigninPolicyHandler() {}

void BrowserSigninPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  int int_value;
  if (value && value->GetAsInteger(&int_value)) {
    // TODO(pastarmovj): Replace this with a int range handler once the
    // deprecating handler can handle it.
    if (static_cast<int>(BrowserSigninMode::kDisabled) > int_value ||
        static_cast<int>(BrowserSigninMode::kForced) < int_value) {
      SYSLOG(ERROR) << "Unexpected value for BrowserSigninMode: " << int_value;
      NOTREACHED();
      return;
    }
    switch (static_cast<BrowserSigninMode>(int_value)) {
      case BrowserSigninMode::kForced:
#if !defined(OS_LINUX)
        prefs->SetValue(prefs::kForceBrowserSignin, base::Value(true));
#endif
        FALLTHROUGH;
      case BrowserSigninMode::kEnabled:
        prefs->SetValue(
#if defined(OS_ANDROID)
            // The new kSigninAllowedOnNextStartup pref is only used on Desktop.
            // Keep the old kSigninAllowed pref for Android until the policy is
            // fully deprecated in M71 and can be removed.
            prefs::kSigninAllowed,
#else
            prefs::kSigninAllowedOnNextStartup,
#endif
            base::Value(true));
        break;
      case BrowserSigninMode::kDisabled:
        prefs->SetValue(
#if defined(OS_ANDROID)
            // The new kSigninAllowedOnNextStartup pref is only used on Desktop.
            // Keep the old kSigninAllowed pref for Android until the policy is
            // fully deprecated in M71 and can be removed.
            prefs::kSigninAllowed,
#else
            prefs::kSigninAllowedOnNextStartup,
#endif
            base::Value(false));
        break;
    }
  }
}

}  // namespace policy
