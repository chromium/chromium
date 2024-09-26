// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "google_apis/google_api_keys.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT) && BUILDFLAG(ENABLE_MIRROR)
#error "Dice and Mirror cannot be both enabled."
#endif

#if !BUILDFLAG(ENABLE_DICE_SUPPORT) && !BUILDFLAG(ENABLE_MIRROR)
#error "Either Dice or Mirror should be enabled."
#endif

using signin::AccountConsistencyMethod;

namespace {

// By default, DICE is not enabled in builds lacking an API key. May be set to
// true for tests.
bool g_ignore_missing_oauth_client_for_testing = false;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const char kAllowBrowserSigninArgument[] = "allow-browser-signin";

bool IsBrowserSigninAllowedByCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kAllowBrowserSigninArgument)) {
    std::string allowBrowserSignin =
        command_line->GetSwitchValueASCII(kAllowBrowserSigninArgument);
    return base::ToLowerASCII(allowBrowserSignin) == "true";
  }
  // If the commandline flag is not provided, the default is true.
  return true;
}

// Returns true if Desktop Identity Consistency can be enabled for this build
// (i.e. if OAuth client ID and client secret are configured).
bool CanEnableDiceForBuild() {
  if (g_ignore_missing_oauth_client_for_testing ||
      google_apis::HasOAuthClientConfigured()) {
    return true;
  }

  // Only log this once.
  [[maybe_unused]] static bool logged_warning = []() {
    LOG(WARNING) << "Desktop Identity Consistency cannot be enabled as no "
                    "OAuth client ID and client secret have been configured.";
    return true;
  }();

  return false;
}
#endif

}  // namespace

AccountConsistencyModeManager::AccountConsistencyModeManager(Profile* profile)
    : profile_(profile),
      account_consistency_(signin::AccountConsistencyMethod::kDisabled),
      account_consistency_initialized_(false) {
  DCHECK(profile_);
  DCHECK(ShouldBuildServiceForProfile(profile));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  auto* entry = g_browser_process->profile_manager()
                    ? g_browser_process->profile_manager()
                          ->GetProfileAttributesStorage()
                          .GetProfileAttributesWithPath(profile_->GetPath())
                    : nullptr;
  PrefService* prefs = profile_->GetPrefs();
  // Propagate settings changes from the previous launch to the signin-allowed
  // pref.
  bool signin_allowed = IsDiceSignInAllowed(entry) &&
                        prefs->GetBoolean(prefs::kSigninAllowedOnNextStartup);
  prefs->SetBoolean(prefs::kSigninAllowed, signin_allowed);

  UMA_HISTOGRAM_BOOLEAN("Signin.SigninAllowed", signin_allowed);
#endif

  account_consistency_ = ComputeAccountConsistencyMethod(profile_);
  DCHECK_EQ(account_consistency_, ComputeAccountConsistencyMethod(profile_));
  account_consistency_initialized_ = true;
}

AccountConsistencyModeManager::~AccountConsistencyModeManager() {}

// static
void AccountConsistencyModeManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSigninAllowedOnNextStartup, true);
}

// static
AccountConsistencyMethod AccountConsistencyModeManager::GetMethodForProfile(
    Profile* profile) {
  if (!ShouldBuildServiceForProfile(profile))
    return AccountConsistencyMethod::kDisabled;

  return AccountConsistencyModeManagerFactory::GetForProfile(profile)
      ->GetAccountConsistencyMethod();
}

// static
bool AccountConsistencyModeManager::IsDiceEnabledForProfile(Profile* profile) {
  return GetMethodForProfile(profile) == AccountConsistencyMethod::kDice;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// static
bool AccountConsistencyModeManager::IsDiceSignInAllowed(
    ProfileAttributesEntry* entry) {
  // Sign in should only be allowed for OIDC profiles with 3P identities that
  // are sync-ed to Google. Otherwise, we won't have a valid GAIA ID to sign in
  // to.
  bool is_oidc_sign_in_disallowed =
      entry && !entry->GetProfileManagementOidcTokens().auth_token.empty() &&
      entry->IsDasherlessManagement();
  return CanEnableDiceForBuild() && IsBrowserSigninAllowedByCommandLine() &&
         !is_oidc_sign_in_disallowed &&
         (!entry || entry->GetProfileManagementEnrollmentToken().empty());
  ;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// static
bool AccountConsistencyModeManager::IsMirrorEnabledForProfile(
    Profile* profile) {
  return GetMethodForProfile(profile) == AccountConsistencyMethod::kMirror;
}

// static
void AccountConsistencyModeManager::SetIgnoreMissingOAuthClientForTesting() {
  g_ignore_missing_oauth_client_for_testing = true;
}

// static
bool AccountConsistencyModeManager::ShouldBuildServiceForProfile(
    Profile* profile) {
  return profile->IsRegularProfile();
}

AccountConsistencyMethod
AccountConsistencyModeManager::GetAccountConsistencyMethod() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40583837): ChromeOS should use the cached value.
  // Changing the value dynamically is not supported.
  return ComputeAccountConsistencyMethod(profile_);
#else
  // The account consistency method should not change during the lifetime of a
  // profile. We always return the cached value, but still check that it did not
  // change, in order to detect inconsisent states. See https://crbug.com/860471
  CHECK(account_consistency_initialized_);
  CHECK_EQ(ComputeAccountConsistencyMethod(profile_), account_consistency_);
  return account_consistency_;
#endif
}

// static
signin::AccountConsistencyMethod
AccountConsistencyModeManager::ComputeAccountConsistencyMethod(
    Profile* profile) {
  DCHECK(ShouldBuildServiceForProfile(profile));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ash::IsAccountManagerAvailable(profile))
    return AccountConsistencyMethod::kDisabled;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Account consistency is unavailable on Guest and Managed Guest Sessions.
  if (chromeos::IsManagedGuestSession() || profile->IsGuestSession()) {
    return AccountConsistencyMethod::kDisabled;
  }
#endif

#if BUILDFLAG(ENABLE_MIRROR)
  return AccountConsistencyMethod::kMirror;
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    VLOG(1) << "Desktop Identity Consistency disabled as sign-in to Chrome "
               "is not allowed";
    return AccountConsistencyMethod::kDisabled;
  }

  return AccountConsistencyMethod::kDice;
#endif

  NOTREACHED_IN_MIGRATION();
  return AccountConsistencyMethod::kDisabled;
}
