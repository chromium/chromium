// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_internal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace signin_util {
namespace {
enum ForceSigninPolicyCache {
  NOT_CACHED = 0,
  ENABLE,
  DISABLE
} g_is_force_signin_enabled_cache = NOT_CACHED;

void SetForceSigninPolicy(bool enable) {
  g_is_force_signin_enabled_cache = enable ? ENABLE : DISABLE;
}

}  // namespace

ScopedForceSigninSetterForTesting::ScopedForceSigninSetterForTesting(
    bool enable) {
  SetForceSigninForTesting(enable);  // IN-TEST
}

ScopedForceSigninSetterForTesting::~ScopedForceSigninSetterForTesting() {
  ResetForceSigninForTesting();  // IN-TEST
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
CookiesMover::CookiesMover(base::WeakPtr<Profile> source_profile,
                           base::WeakPtr<Profile> destination_profile,
                           base::OnceCallback<void()> callback)
    : url_(source_profile->GetPrefs()->GetString(
          prefs::kSigninInterceptionIDPCookiesUrl)),
      source_profile_(std::move(source_profile)),
      destination_profile_(std::move(destination_profile)),
      callback_(std::move(callback)) {
  CHECK(callback_);
}

CookiesMover::~CookiesMover() = default;

void CookiesMover::StartMovingCookies() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  bool allow_cookies_to_be_moved = base::FeatureList::IsEnabled(
      profile_management::features::kThirdPartyProfileManagement);
#else
  bool allow_cookies_to_be_moved = false;
#endif
  if (!allow_cookies_to_be_moved || url_.is_empty() || !url_.is_valid()) {
    std::move(callback_).Run();
    return;
  }

  source_profile_->GetPrefs()->ClearPref(
      prefs::kSigninInterceptionIDPCookiesUrl);
  source_profile_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetCookieList(url_, net::CookieOptions::MakeAllInclusive(),
                      net::CookiePartitionKeyCollection::Todo(),
                      base::BindOnce(&CookiesMover::OnCookiesReceived,
                                     weak_pointer_factory_.GetWeakPtr()));
}

void CookiesMover::OnCookiesReceived(
    const std::vector<net::CookieWithAccessResult>& included,
    const std::vector<net::CookieWithAccessResult>& excluded) {
  // If either profile was destroyed, stop the operation.
  if (source_profile_.WasInvalidated() ||
      destination_profile_.WasInvalidated()) {
    std::move(callback_).Run();
    return;
  }
  // We expected 2 * `cookies.size()` actions since we have to set the cookie at
  // destination and delete it from the source.
  base::RepeatingClosure barrier = base::BarrierClosure(
      included.size() * 2, base::BindOnce(&CookiesMover::OnCookiesMoved,
                                          weak_pointer_factory_.GetWeakPtr()));
  auto* source_cookie_manager = source_profile_->GetDefaultStoragePartition()
                                    ->GetCookieManagerForBrowserProcess();
  auto* destination_cookie_manager =
      destination_profile_->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();
  for (const auto& [cookie, _] : included) {
    destination_cookie_manager->SetCanonicalCookie(
        cookie, url_, net::CookieOptions::MakeAllInclusive(),
        base::IgnoreArgs<net::CookieAccessResult>(barrier));
    source_cookie_manager->DeleteCanonicalCookie(
        cookie, base::IgnoreArgs<bool>(barrier));
  }
}

void CookiesMover::OnCookiesMoved() {
  std::move(callback_).Run();
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

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

  if (base::StartsWith(current_profile_account_restriction,
                       "primary_account")) {
    result.Put(ProfileSeparationPolicyState::kEnforcedByExistingProfile);

    if (profile->GetPrefs()->GetBoolean(
            prefs::kManagedAccountsSigninRestrictionScopeMachine)) {
      result.Put(ProfileSeparationPolicyState::kEnforcedOnMachineLevel);
    }
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
