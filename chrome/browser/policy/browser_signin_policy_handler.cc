// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_signin_policy_handler.h"

#include <memory>

#include "base/command_line.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"

namespace policy {

BrowserSigninPolicyHandler::BrowserSigninPolicyHandler(Schema chrome_schema)
    : IntRangePolicyHandler(key::kBrowserSignin,
                            prefs::kForceBrowserSignin,
                            static_cast<int>(BrowserSigninMode::kDisabled),
                            static_cast<int>(BrowserSigninMode::kForced),
                            false /* clamp */) {}

BrowserSigninPolicyHandler::~BrowserSigninPolicyHandler() {}

void BrowserSigninPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
#if BUILDFLAG(IS_WIN)
  // Browser sign in policies shouldn't be enforced on gcpw signin
  // mode as gcpw is invoked in windows login UI screen.
  // Also note that GCPW launches chrome in incognito mode using a
  // special user's logon_token. So the end user won't have access
  // to this session after user logs in via GCPW.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::credential_provider::kGcpwSigninSwitch))
    return;
#endif

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  switch (static_cast<BrowserSigninMode>(value->GetInt())) {
    case BrowserSigninMode::kForced:
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
      prefs->SetValue(prefs::kForceBrowserSignin, base::Value(true));
#endif
      [[fallthrough]];
    case BrowserSigninMode::kEnabled:
      prefs->SetValue(
#if BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_ANDROID)
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

}  // namespace policy
