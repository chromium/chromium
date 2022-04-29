// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_internal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace signin_util {
namespace {
constexpr char kSignoutSettingKey[] = "signout_setting";

enum ForceSigninPolicyCache {
  NOT_CACHED = 0,
  ENABLE,
  DISABLE
} g_is_force_signin_enabled_cache = NOT_CACHED;

void SetForceSigninPolicy(bool enable) {
  g_is_force_signin_enabled_cache = enable ? ENABLE : DISABLE;
}
}  // namespace

UserSignoutSetting::UserSignoutSetting() = default;
UserSignoutSetting::~UserSignoutSetting() = default;

// static Per-profile manager for the signout allowed setting.
UserSignoutSetting* UserSignoutSetting::GetForProfile(Profile* profile) {
  UserSignoutSetting* signout_setting = static_cast<UserSignoutSetting*>(
      profile->GetUserData(kSignoutSettingKey));

  if (!signout_setting) {
    profile->SetUserData(kSignoutSettingKey,
                         std::make_unique<UserSignoutSetting>());
    signout_setting = static_cast<UserSignoutSetting*>(
        profile->GetUserData(kSignoutSettingKey));
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  signout_setting->is_main_profile_ = profile->IsMainProfile();
#endif
  return signout_setting;
}

void UserSignoutSetting::SetSignoutAllowed(bool is_allowed) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (is_main_profile_ && !is_allowed) {
    // Turn off sync is always allowed in the main profile. For managed
    // profiles, it does not introduce cross-sync risks as the primary account
    // can't be changed.
    DCHECK(false) << "Signout is always allowed in the main profile.";
    return;
  }
#endif
  signout_allowed_ =
      is_allowed ? signin::Tribool::kTrue : signin::Tribool::kFalse;
}

signin::Tribool UserSignoutSetting::signout_allowed() const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (is_main_profile_) {
    return signin::Tribool::kTrue;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return signout_allowed_;
}

ScopedForceSigninSetterForTesting::ScopedForceSigninSetterForTesting(
    bool enable) {
  SetForceSigninForTesting(enable);  // IN-TEST
}

ScopedForceSigninSetterForTesting::~ScopedForceSigninSetterForTesting() {
  ResetForceSigninForTesting();  // IN-TEST
}

bool IsForceSigninEnabled() {
  if (g_is_force_signin_enabled_cache == NOT_CACHED) {
    PrefService* prefs = g_browser_process->local_state();
    if (prefs)
      SetForceSigninPolicy(prefs->GetBoolean(prefs::kForceBrowserSignin));
    else
      return false;
  }
  return (g_is_force_signin_enabled_cache == ENABLE);
}

void SetForceSigninForTesting(bool enable) {
  SetForceSigninPolicy(enable);
}

void ResetForceSigninForTesting() {
  g_is_force_signin_enabled_cache = NOT_CACHED;
}

bool IsUserSignoutAllowedForProfile(Profile* profile) {
  return UserSignoutSetting::GetForProfile(profile)->signout_allowed() ==
         signin::Tribool::kTrue;
}

void SetUserSignoutAllowedForProfile(Profile* profile, bool is_allowed) {
  UserSignoutSetting::GetForProfile(profile)->SetSignoutAllowed(is_allowed);
}

void EnsureUserSignoutAllowedIsInitializedForProfile(Profile* profile) {
  if (UserSignoutSetting::GetForProfile(profile)->signout_allowed() ==
      signin::Tribool::kUnknown) {
    SetUserSignoutAllowedForProfile(profile, /*is_allowed=*/true);
  }
}

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
bool ProfileSeparationEnforcedByPolicy(
    Profile* profile,
    const std::string& intercepted_account_level_policy_value) {
  std::string current_profile_account_restriction =
      profile->GetPrefs()->GetString(prefs::kManagedAccountsSigninRestriction);

  bool is_machine_level_policy = profile->GetPrefs()->GetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine);

  // Enforce profile separation for all new signins if any restriction is
  // applied at a machine level.
  if (is_machine_level_policy) {
    return !current_profile_account_restriction.empty() &&
           current_profile_account_restriction != "none";
  }

  // Enforce profile separation for all new signins if "primary_account_strict"
  // is set at the user account level.
  return base::StartsWith(current_profile_account_restriction,
                          "primary_account_strict") ||
         base::StartsWith(intercepted_account_level_policy_value,
                          "primary_account");
}

bool ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
    Profile* profile,
    const std::string& intercepted_account_level_policy_value) {
  std::string current_profile_account_restriction =
      profile->GetPrefs()->GetString(prefs::kManagedAccountsSigninRestriction);

  return !ProfileSeparationEnforcedByPolicy(
             profile, intercepted_account_level_policy_value) ||
         base::EndsWith(current_profile_account_restriction,
                        "keep_existing_data") ||
         base::EndsWith(intercepted_account_level_policy_value,
                        "keep_existing_data");
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

void RecordEnterpriseProfileCreationUserChoice(bool enforced_by_policy,
                                               bool created) {
  base::UmaHistogramBoolean(
      enforced_by_policy
          ? "Signin.Enterprise.WorkProfile.ProfileCreatedWithPolicySet"
          : "Signin.Enterprise.WorkProfile.ProfileCreatedwithPolicyUnset",
      created);
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace signin_util
