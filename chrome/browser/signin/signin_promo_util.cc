// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "net/base/network_change_notifier.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/sync/service/sync_prefs.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/profile_requirement_utils.h"

namespace {

using signin_util::SignedInState;

constexpr int kSigninPromoShownThreshold = 5;
constexpr int kSigninPromoDismissedThreshold = 2;

}  // namespace
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if !BUILDFLAG(IS_ANDROID)
namespace {

// Performs base checks for whether the sync/sign in promos should be shown.
// Needs additional checks depending on the type of the promo (see
// ShouldShowSyncPromo and ShouldShowSignInPromo). |profile| is the profile of
// the tab the promo would be shown on.
bool ShouldShowPromoCommon(Profile& profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // There's no need to show the sign in promo on cros since cros users are
  // already logged in.
  return false;
#else

  // Don't bother if we don't have any kind of network connection.
  if (net::NetworkChangeNotifier::IsOffline()) {
    return false;
  }

  // Consider original profile even if an off-the-record profile was
  // passed to this method as sign-in state is only defined for the
  // primary profile.
  Profile* original_profile = profile.GetOriginalProfile();

  // Don't show for supervised child profiles.
  if (original_profile->IsChild()) {
    return false;
  }

  // Don't show if sign in is not allowed.
  if (!original_profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    return false;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(original_profile);

  // No promo if the user is already syncing.
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return false;
  }

  // Verified the base checks. Depending on whether the promo should be for sync
  // or signin, additional checks are necessary.
  return true;
#endif
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
bool ShouldShowSignInPromoCommon(Profile& profile) {
  // Don't show the promo if it does not pass the base checks.
  if (!ShouldShowPromoCommon(profile)) {
    return false;
  }

  // Don't show the promo if the user is off-the-record.
  if (profile.IsOffTheRecord()) {
    return false;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile);

  SignedInState signed_in_state =
      signin_util::GetSignedInState(identity_manager);

  switch (signed_in_state) {
    case signin_util::SignedInState::kSignedIn:
    case signin_util::SignedInState::kSyncing:
    case signin_util::SignedInState::kSyncPaused:
      // Don't show the promo if the user is already signed in or syncing.
      return false;
    case signin_util::SignedInState::kSignInPending:
      // Always show the promo in sign in pending state.
      return true;
    case signin_util::SignedInState::kSignedOut:
    case signin_util::SignedInState::kWebOnlySignedIn:
      break;
  }

  // Don't show the promo again after it was dismissed twice, regardless of
  // autofill bubble promo type.
  AccountInfo account =
      signin_ui_util::GetSingleAccountForPromos(identity_manager);
  int dismiss_count =
      account.gaia.empty()
          ? profile.GetPrefs()->GetInteger(
                prefs::kAutofillSignInPromoDismissCountPerProfile)
          : SigninPrefs(*profile.GetPrefs())
                .GetAutofillSigninPromoDismissCount(account.gaia);

  if (dismiss_count >= kSigninPromoDismissedThreshold) {
    return false;
  }

  // Only show the promo if explicit browser signin is enabled.
  return switches::IsExplicitBrowserSigninUIOnDesktopEnabled();
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace
#endif  // !BUILDFLAG(IS_ANDROID)

namespace signin {

#if !BUILDFLAG(IS_ANDROID)
bool ShouldShowSyncPromo(Profile& profile) {
  // Don't show the promo if it does not pass the base checks.
  if (!ShouldShowPromoCommon(profile)) {
    return false;
  }

  syncer::SyncPrefs prefs(profile.GetPrefs());
  // Don't show if sync is not allowed to start or is running in local mode.
  if (!SyncServiceFactory::IsSyncAllowed(&profile) ||
      prefs.IsLocalSyncEnabled()) {
    return false;
  }

  return true;
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool ShouldShowPasswordSignInPromo(Profile& profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)

  if (!ShouldShowSignInPromoCommon(profile)) {
    return false;
  }

  IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile);

  // Show the promo if the user is sign in pending, regardless of impression
  // count.
  if (signin_util::IsSigninPending(identity_manager)) {
    return true;
  }

  // Don't show the promo again if it has already been shown 5 times.
  AccountInfo account =
      signin_ui_util::GetSingleAccountForPromos(identity_manager);
  int show_count =
      account.gaia.empty()
          ? profile.GetPrefs()->GetInteger(
                prefs::kPasswordSignInPromoShownCountPerProfile)
          : SigninPrefs(*profile.GetPrefs())
                .GetPasswordSigninPromoImpressionCount(account.gaia);

  if (show_count >= kSigninPromoShownThreshold) {
    return false;
  }

  return true;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool ShouldShowAddressSignInPromo(Profile& profile,
                                  const autofill::AutofillProfile& address) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Remove this once enabled by default.
  if (!switches::IsImprovedSigninUIOnDesktopEnabled()) {
    return false;
  }

  if (!ShouldShowSignInPromoCommon(profile)) {
    return false;
  }

  // Don't show the promo if the new address is not eligible for account
  // storage.
  if (!autofill::IsProfileEligibleForMigrationToAccount(
          autofill::PersonalDataManagerFactory::GetForBrowserContext(&profile)
              ->address_data_manager(),
          address)) {
    return false;
  }

  // TODO (crbug.com/5776109): Check for impression count.

  return true;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool IsAutofillSigninPromo(signin_metrics::AccessPoint access_point) {
  return access_point ==
             signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE ||
         access_point ==
             signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE;
}

}  // namespace signin
