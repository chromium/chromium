// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
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
#include "components/signin/public/identity_manager/tribool.h"
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
  if (profile->IsMainProfile()) {
    // Clearing the primary account on Lacros main profile is not allowed.
    signout_setting->InitializeUserSignoutSettingIfNeeded();
  }
#endif
  return signout_setting;
}

void UserSignoutSetting::InitializeUserSignoutSettingIfNeeded() {
  if (revoke_sync_consent_allowed_ == signin::Tribool::kUnknown)
    SetRevokeSyncConsentAllowed(true);

  if (clear_primary_account_allowed_ != signin::Tribool::kUnknown)
    return;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (is_main_profile_ && !ignore_is_main_profile_for_testing_) {
    SetClearPrimaryAccountAllowed(false);
    return;
  }
#endif
  SetClearPrimaryAccountAllowed(true);
}

void UserSignoutSetting::ResetSignoutSetting() {
  clear_primary_account_allowed_ = signin::Tribool::kUnknown;
  revoke_sync_consent_allowed_ = signin::Tribool::kUnknown;
  InitializeUserSignoutSettingIfNeeded();
}

bool UserSignoutSetting::IsClearPrimaryAccountAllowed() const {
  return clear_primary_account_allowed_ == signin::Tribool::kTrue;
}

void UserSignoutSetting::SetClearPrimaryAccountAllowed(bool allowed) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!ignore_is_main_profile_for_testing_ && is_main_profile_ && allowed) {
    // Changing the primary account is not allowed as it must be the device
    // account.
    NOTREACHED()
        << "Clearing primary account can't be allowed in the main profile.";
    return;
  }
#endif
  if (allowed && revoke_sync_consent_allowed_ == signin::Tribool::kFalse) {
    NOTREACHED() << "Revoke sync is not allowed therefore it is not possible "
                    "to allow clearing the primary account. To reset signout "
                    "setting please use 'ResetSignoutSetting'";
    return;
  }

  clear_primary_account_allowed_ =
      allowed ? signin::Tribool::kTrue : signin::Tribool::kFalse;
}

bool UserSignoutSetting::IsRevokeSyncConsentAllowed() const {
  return revoke_sync_consent_allowed_ == signin::Tribool::kTrue;
}

void UserSignoutSetting::SetRevokeSyncConsentAllowed(bool allowed) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!ignore_is_main_profile_for_testing_ && is_main_profile_ && !allowed) {
    NOTREACHED()
        << "Revoke sync consent is always allowed in Lacros main profile.";
    return;
  }
#endif
  revoke_sync_consent_allowed_ =
      allowed ? signin::Tribool::kTrue : signin::Tribool::kFalse;
  if (!allowed) {
    // If revoke sync is not allowed, it implies that removing the primary
    // account is also not allowed.
    SetClearPrimaryAccountAllowed(false);
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void UserSignoutSetting::IgnoreIsMainProfileForTesting() {
  ignore_is_main_profile_for_testing_ = true;
}
#endif

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

bool IsProfileDeletionAllowed(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return !profile->IsMainProfile();
#elif BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
ProfileSeparationPolicyStateSet GetProfileSeparationPolicyState(
    Profile* profile,
    const absl::optional<std::string>& intercepted_account_level_policy_value) {
  ProfileSeparationPolicyStateSet result;

  std::string current_profile_account_restriction =
      profile->GetPrefs()->GetString(prefs::kManagedAccountsSigninRestriction);

  if (profile->GetPrefs()->GetBoolean(
          prefs::kManagedAccountsSigninRestrictionScopeMachine)) {
    result.Put(ProfileSeparationPolicyState::kEnforcedOnMachineLevel);
  }
  if (base::StartsWith(current_profile_account_restriction,
                       "primary_account")) {
    result.Put(ProfileSeparationPolicyState::kEnforcedByExistingProfile);
  }
  if (base::StartsWith(current_profile_account_restriction,
                       "primary_account_strict")) {
    result.Put(ProfileSeparationPolicyState::kStrict);
  }
  if (base::StartsWith(
          intercepted_account_level_policy_value.value_or(std::string()),
          "primary_account")) {
    result.Put(ProfileSeparationPolicyState::kEnforcedByInterceptedAccount);
  }

  if (base::StartsWith(
          intercepted_account_level_policy_value.value_or(std::string()),
          "primary_account_strict")) {
    result.Put(ProfileSeparationPolicyState::kStrict);
  }

  if (result.Empty())
    return result;

  bool profile_allows_keeping_existing_browsing_data =
      !(result.Has(ProfileSeparationPolicyState::kEnforcedByExistingProfile)) ||
      base::EndsWith(current_profile_account_restriction, "keep_existing_data");
  bool account_allows_keeping_existing_browsing_data =
      !(result.Has(
          ProfileSeparationPolicyState::kEnforcedByInterceptedAccount)) ||
      base::EndsWith(intercepted_account_level_policy_value.value(),
                     "keep_existing_data");
  // Keep Existing browsing data if both sources for the policy allow it.
  if (profile_allows_keeping_existing_browsing_data &&
      account_allows_keeping_existing_browsing_data) {
    result.Put(ProfileSeparationPolicyState::kKeepsBrowsingData);
  }

  return result;
}

bool ProfileSeparationEnforcedByPolicy(
    Profile* profile,
    const absl::optional<std::string>& intercepted_account_level_policy_value) {
  auto separation_policy_state = GetProfileSeparationPolicyState(
      profile, intercepted_account_level_policy_value);
  return !base::Intersection(
              separation_policy_state,
              {ProfileSeparationPolicyState::kStrict,
               ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
               ProfileSeparationPolicyState::kEnforcedOnMachineLevel})
              .Empty();
}

bool ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
    Profile* profile,
    const std::string& intercepted_account_level_policy_value) {
  auto profile_separation_state = GetProfileSeparationPolicyState(
      profile, intercepted_account_level_policy_value);
  return profile_separation_state.Empty() ||
         profile_separation_state.Has(
             ProfileSeparationPolicyState::kKeepsBrowsingData);
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
