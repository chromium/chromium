// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
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
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_internal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
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
// Returns true if managed accounts signin are required to create a new profile
// by policies set in `profile`.
bool IsProfileSeparationEnforcedByProfile(
    Profile* profile,
    const std::string& intercepted_account_email) {
  if (!intercepted_account_email.empty() &&
      !IsAccountExemptedFromEnterpriseProfileSeparation(
          profile, intercepted_account_email)) {
    return true;
  }
  std::string legacy_policy_for_current_profile =
      profile->GetPrefs()->GetString(prefs::kManagedAccountsSigninRestriction);
  bool enforced_by_existing_profile = base::StartsWith(
      legacy_policy_for_current_profile, "primary_account_strict");
  bool enforced_at_machine_level =
      base::StartsWith(legacy_policy_for_current_profile, "primary_account") &&
      profile->GetPrefs()->GetBoolean(
          prefs::kManagedAccountsSigninRestrictionScopeMachine);
  return enforced_by_existing_profile || enforced_at_machine_level;
}

// Returns true if profile separation is enforced by
// `intercepted_account_separation_policies`.
bool IsProfileSeparationEnforcedByPolicies(
    const policy::ProfileSeparationPolicies&
        intercepted_account_separation_policies) {
  if (intercepted_account_separation_policies.profile_separation_settings()
          .value_or(policy::ProfileSeparationSettings::SUGGESTED) ==
      policy::ProfileSeparationSettings::ENFORCED) {
    return true;
  }

  std::string legacy_policy_for_intercepted_profile =
      intercepted_account_separation_policies
          .managed_accounts_signin_restrictions()
          .value_or(std::string());
  return base::StartsWith(legacy_policy_for_intercepted_profile,
                          "primary_account");
}

bool ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
    Profile* profile,
    const policy::ProfileSeparationPolicies&
        intercepted_account_separation_policies) {
  // We should not move managed data.
  if (enterprise_util::UserAcceptedAccountManagement(profile)) {
    return false;
  }

  std::string legacy_policy_for_intercepted_profile =
      intercepted_account_separation_policies
          .managed_accounts_signin_restrictions()
          .value_or(std::string());
  std::string legacy_policy_for_current_profile =
      profile->GetPrefs()->GetString(prefs::kManagedAccountsSigninRestriction);
  bool allowed_by_existing_profile =
      legacy_policy_for_current_profile.empty() ||
      legacy_policy_for_current_profile == "none" ||
      base::EndsWith(legacy_policy_for_current_profile, "keep_existing_data");
  bool allowed_by_intercepted_account =
      intercepted_account_separation_policies
              .profile_separation_data_migration_settings()
              .value_or(policy::ProfileSeparationDataMigrationSettings::
                            USER_OPT_IN) !=
          policy::ProfileSeparationDataMigrationSettings::ALWAYS_SEPARATE &&
      (legacy_policy_for_intercepted_profile.empty() ||
       legacy_policy_for_intercepted_profile == "none" ||
       base::EndsWith(legacy_policy_for_intercepted_profile,
                      "keep_existing_data"));
  return allowed_by_existing_profile && allowed_by_intercepted_account;
}

bool IsAccountExemptedFromEnterpriseProfileSeparation(
    Profile* profile,
    const std::string& email) {
  if (profile->GetPrefs()
          ->FindPreference(prefs::kProfileSeparationDomainExceptionList)
          ->IsDefaultValue()) {
    return true;
  }

  const std::string domain = gaia::ExtractDomainName(email);
  const auto& allowed_domains = profile->GetPrefs()->GetList(
      prefs::kProfileSeparationDomainExceptionList);
  return base::Contains(allowed_domains, base::Value(domain));
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

PrimaryAccountError SetPrimaryAccountWithInvalidToken(
    Profile* profile,
    const std::string& user_email,
    const std::string& gaia_id,
    bool is_under_advanced_protection,
    signin_metrics::AccessPoint access_point,
    signin_metrics::SourceForRefreshTokenOperation source) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  CHECK(identity_manager->FindExtendedAccountInfoByEmailAddress(user_email)
            .IsEmpty());
  CHECK(!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  DVLOG(1) << "Adding user with gaia id <" << gaia_id << "> and email <"
           << user_email << "> with invalid refresh token.";

  // Lock AccountReconcilor temporarily to prevent AddOrUpdateAccount failure
  // since we have an invalid refresh token.
  AccountReconcilor::Lock account_reconcilor_lock(
      AccountReconcilorFactory::GetForProfile(profile));

  CoreAccountId account_id =
      identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
          gaia_id, user_email, GaiaConstants::kInvalidRefreshToken,
          is_under_advanced_protection, access_point, source);

  DVLOG(1) << "Account id <" << account_id.ToString()
           << "> has been added to the profile with invalid token.";

  auto set_primary_account_result =
      identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
          account_id, signin::ConsentLevel::kSignin);
  DVLOG(1) << "Operation of setting account id <" << account_id.ToString()
           << "> received the following result: "
           << static_cast<int>(set_primary_account_result);

  return set_primary_account_result;
}

bool IsSigninPending(signin::IdentityManager* identity_manager) {
  return !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
         identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
         identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
             identity_manager->GetPrimaryAccountId(
                 signin::ConsentLevel::kSignin));
}

SignedInState GetSignedInState(signin::IdentityManager* identity_manager) {
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSync))) {
      return SignedInState::kSyncPaused;
    }
    return SignedInState::kSyncing;
  }

  // If explicit browser signin is not enabled, this returns `kSignedIn`
  // regardless of the error state of the refresh token. There might for example
  // be an error in the following two cases: (a) The account is managed. (b) The
  // account is not managed, but the `SigninManager` has not been notified yet,
  // which would sign the user out.
  //
  // If the error state of the primary account is relevant, then it needs to be
  // checked in addition to this state.
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
               identity_manager->GetPrimaryAccountId(
                   signin::ConsentLevel::kSignin)) &&
                   switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
               ? SignedInState::kSignInPending
               : SignedInState::kSignedIn;
  }

  // Not signed, but at least one account is signed in on the web.
  if (!identity_manager->GetAccountsWithRefreshTokens().empty()) {
    return SignedInState::kWebOnlySignedIn;
  }

  return SignedInState::kSignedOut;
}

}  // namespace signin_util
