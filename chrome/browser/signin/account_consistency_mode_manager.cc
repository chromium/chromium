// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "google_apis/google_api_keys.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/account_manager/account_manager_util.h"
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
    return base::EqualsCaseInsensitiveASCII(allowBrowserSignin, "true");
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

// Propagates settings changes from the previous launch to the signin-allowed
// pref. `kSigninAllowedOnNextStartup` may be controlled by a policy that lives
// only in memory (`MANAGED_STORE`) and is not yet loaded on startup if policy
// loading races with profile initialization (e.g. on Windows). If
// `kSigninAllowedOnNextStartup` is currently user-modifiable (i.e. not
// overridden by a managed or supervised store) and a cached value from the
// previous session exists, uses the cached value.
AccountConsistencyModeManager::SigninAllowedOnStartupState
ComputeSigninAllowedOnStartupState(PrefService* prefs,
                                   const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ? g_browser_process->profile_manager()
                ->GetProfileAttributesStorage()
                .GetProfileAttributesWithPath(profile_path)
          : nullptr;
  if (!AccountConsistencyModeManager::IsDiceSignInAllowed(entry) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kExperimentalAiStableChannel)) {
    return AccountConsistencyModeManager::SigninAllowedOnStartupState::
        kDisallowedByCommandLineOrProfile;
  }

  const PrefService::Preference* next_startup_pref =
      prefs->FindPreference(prefs::kSigninAllowedOnNextStartup);
  const bool pref_value = next_startup_pref->GetValue()->GetBool();
  if (!next_startup_pref->IsUserModifiable()) {
    return pref_value ? AccountConsistencyModeManager::
                            SigninAllowedOnStartupState::kAllowedByPolicy
                      : AccountConsistencyModeManager::
                            SigninAllowedOnStartupState::kDisallowedByPolicy;
  }

  if (prefs->HasPrefPath(prefs::kSigninAllowedOnNextStartupCache)) {
    const bool cached_value =
        prefs->GetBoolean(prefs::kSigninAllowedOnNextStartupCache);
    if (cached_value != pref_value) {
      return cached_value ? AccountConsistencyModeManager::
                                SigninAllowedOnStartupState::kAllowedByCache
                          : AccountConsistencyModeManager::
                                SigninAllowedOnStartupState::kDisallowedByCache;
    }
  }

  return pref_value ? AccountConsistencyModeManager::
                          SigninAllowedOnStartupState::kAllowedByPref
                    : AccountConsistencyModeManager::
                          SigninAllowedOnStartupState::kDisallowedByPref;
}

bool IsSigninAllowedForStartupState(
    AccountConsistencyModeManager::SigninAllowedOnStartupState state) {
  switch (state) {
    case AccountConsistencyModeManager::SigninAllowedOnStartupState::
        kAllowedByPref:
    case AccountConsistencyModeManager::SigninAllowedOnStartupState::
        kAllowedByPolicy:
      return true;
    case AccountConsistencyModeManager::SigninAllowedOnStartupState::
        kAllowedByCache:
      return base::FeatureList::IsEnabled(kCacheSigninAllowedOnNextStartup);
    case AccountConsistencyModeManager::SigninAllowedOnStartupState::
        kDisallowedByPref:
    case AccountConsistencyModeManager::SigninAllowedOnStartupState::
        kDisallowedByPolicy:
    case AccountConsistencyModeManager::SigninAllowedOnStartupState::
        kDisallowedByCommandLineOrProfile:
      return false;
    case AccountConsistencyModeManager::SigninAllowedOnStartupState::
        kDisallowedByCache:
      return !base::FeatureList::IsEnabled(kCacheSigninAllowedOnNextStartup);
  }
  NOTREACHED();
}
#endif

// Computes the account consistency method for the current profile. The account
// consistency method cannot change during the lifetime of a profile.
signin::AccountConsistencyMethod ComputeAccountConsistencyMethod(
    Profile* profile) {
  CHECK(AccountConsistencyModeManager::ShouldBuildServiceForProfile(profile),
        base::NotFatalUntil::M162);

#if BUILDFLAG(IS_CHROMEOS)
  if (!ash::IsAccountManagerAvailable(profile)) {
    return AccountConsistencyMethod::kDisabled;
  }
#endif

#if BUILDFLAG(ENABLE_MIRROR)
  return AccountConsistencyMethod::kMirror;
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    VLOG(1) << "Desktop Identity Consistency disabled as sign-in to Chrome "
               "is not allowed";
    return AccountConsistencyMethod::kDisabled;
  }

  return AccountConsistencyMethod::kDice;
#else
  NOTREACHED();
#endif
}

}  // namespace

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
BASE_FEATURE(kCacheSigninAllowedOnNextStartup,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

AccountConsistencyModeManager::AccountConsistencyModeManager(Profile* profile)
    : profile_(profile) {
  CHECK(profile_, base::NotFatalUntil::M162);
  CHECK(ShouldBuildServiceForProfile(profile), base::NotFatalUntil::M162);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  PrefService* prefs = profile_->GetPrefs();
  const SigninAllowedOnStartupState startup_state =
      ComputeSigninAllowedOnStartupState(prefs, profile_->GetPath());
  const bool signin_allowed = IsSigninAllowedForStartupState(startup_state);
  prefs->SetBoolean(prefs::kSigninAllowed, signin_allowed);

  // Seed or reset the cache to the current value of
  // `kSigninAllowedOnNextStartup`. If a policy was removed while Chrome was
  // closed, no pref change notification will fire during this session, so
  // resetting here ensures the previous session's cached policy value is only
  // used for one startup unless re-confirmed when policies finish loading.
  UpdateSigninAllowedOnNextStartupCache();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kSigninAllowedOnNextStartup,
      base::BindRepeating(
          &AccountConsistencyModeManager::UpdateSigninAllowedOnNextStartupCache,
          base::Unretained(this)));

  base::UmaHistogramBoolean("Signin.SigninAllowed", signin_allowed);
  base::UmaHistogramEnumeration("Signin.SigninAllowedOnStartupState",
                                startup_state);
#endif

  account_consistency_ = ComputeAccountConsistencyMethod(profile_);
  CHECK_EQ(account_consistency_, ComputeAccountConsistencyMethod(profile_),
           base::NotFatalUntil::M162);
  account_consistency_initialized_ = true;
}

AccountConsistencyModeManager::~AccountConsistencyModeManager() = default;

void AccountConsistencyModeManager::Shutdown() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  pref_change_registrar_.RemoveAll();
#endif
}

// static
void AccountConsistencyModeManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSigninAllowedOnNextStartup, true);
  registry->RegisterBooleanPref(prefs::kSigninAllowedOnNextStartupCache, true);
}

// static
AccountConsistencyMethod AccountConsistencyModeManager::GetMethodForProfile(
    Profile* profile) {
  if (!ShouldBuildServiceForProfile(profile)) {
    return AccountConsistencyMethod::kDisabled;
  }

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
      entry && !entry->GetProfileManagementOidcTokens().id_token.empty() &&
      entry->IsDasherlessManagement();
  return CanEnableDiceForBuild() && IsBrowserSigninAllowedByCommandLine() &&
         !is_oidc_sign_in_disallowed &&
         (!entry || entry->GetProfileManagementEnrollmentToken().empty());
}

void AccountConsistencyModeManager::UpdateSigninAllowedOnNextStartupCache() {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kSigninAllowedOnNextStartupCache,
                    prefs->GetBoolean(prefs::kSigninAllowedOnNextStartup));
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
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/40583837): ChromeOS should use the cached value.
  // Changing the value dynamically is not supported.
  return ComputeAccountConsistencyMethod(profile_);
#else
  // The account consistency method should not change during the lifetime of a
  // profile. We always return the cached value, but still check that it did not
  // change, in order to detect inconsistent states. See crbug.com/40583741
  CHECK(account_consistency_initialized_);
  CHECK_EQ(ComputeAccountConsistencyMethod(profile_), account_consistency_);
  return account_consistency_;
#endif
}
