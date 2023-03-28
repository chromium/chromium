// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/incognito_mode_policy_handler.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

IncognitoModePolicyHandler::IncognitoModePolicyHandler() {}

IncognitoModePolicyHandler::~IncognitoModePolicyHandler() {}

bool IncognitoModePolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                     PolicyErrorMap* errors) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* availability =
      policies.GetValueUnsafe(key::kIncognitoModeAvailability);
  if (availability) {
    if (!availability->is_int()) {
      errors->AddError(key::kIncognitoModeAvailability, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::INTEGER));
      return false;
    }
    policy::IncognitoModeAvailability availability_enum_value;
    if (!IncognitoModePrefs::IntToAvailability(availability->GetInt(),
                                               &availability_enum_value)) {
      errors->AddError(key::kIncognitoModeAvailability,
                       IDS_POLICY_OUT_OF_RANGE_ERROR,
                       base::NumberToString(availability->GetInt()));
      return false;
    }
    return true;
  }

  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* deprecated_enabled =
      policies.GetValueUnsafe(key::kIncognitoEnabled);
  if (deprecated_enabled && !deprecated_enabled->is_bool()) {
    errors->AddError(key::kIncognitoEnabled, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::BOOLEAN));
    return false;
  }
  return true;
}

void IncognitoModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
#if BUILDFLAG(IS_WIN)
  // When browser starts with GCPW sign-in flag, it runs in incognito mode and
  // gaia login page is loaded. With this flag, user can't use Chrome normally.
  // However GCPW can't work in non-incognito mode and policy setting prevents
  // Chrome from launching in incognito mode.To make this work, we should ignore
  // setting inconito mode policy if GCPW sign-in flag is present.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::credential_provider::kGcpwSigninSwitch))
    return;
#endif

  const base::Value* availability = policies.GetValue(
      key::kIncognitoModeAvailability, base::Value::Type::INTEGER);
  const base::Value* deprecated_enabled =
      policies.GetValue(key::kIncognitoEnabled, base::Value::Type::BOOLEAN);
  if (availability) {
    policy::IncognitoModeAvailability availability_enum_value;
    if (IncognitoModePrefs::IntToAvailability(availability->GetInt(),
                                              &availability_enum_value)) {
      prefs->SetInteger(policy::policy_prefs::kIncognitoModeAvailability,
                        static_cast<int>(availability_enum_value));
    }
  } else if (deprecated_enabled) {
    // If kIncognitoModeAvailability is not specified, check the obsolete
    // kIncognitoEnabled.
    prefs->SetInteger(
        policy::policy_prefs::kIncognitoModeAvailability,
        static_cast<int>(deprecated_enabled->GetBool()
                             ? policy::IncognitoModeAvailability::kEnabled
                             : policy::IncognitoModeAvailability::kDisabled));
  }
}

}  // namespace policy
