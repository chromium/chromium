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
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_requirement_utils.h"

namespace {

using signin_util::SignedInState;

constexpr int kSigninPromoShownThreshold = 5;
constexpr int kSigninPromoDismissedThreshold = 2;

// Maps to a subset of `signin_metrics::AccessPoint`.
enum class AutofillSignInPromoType { kPassword, kAddress };

// Performs base checks for whether the sign in promos should be shown.
// Needs additional checks depending on the type of the promo (see
// `ShouldShowAddressSignInPromo` and `ShouldShowPasswordSignInPromo`).
// `profile` is the profile of the tab the promo would be shown on.
bool ShouldShowSignInPromoCommon(Profile& profile) {
  // Don't show the promo if it does not pass the sync base checks.
  if (!signin::ShouldShowSyncPromo(profile)) {
    return false;
  }

  // Don't show the promo if the user is off-the-record.
  if (profile.IsOffTheRecord()) {
    return false;
  }

  // TODO(crbug.com/386774184): Check for policies which may disallow account
  // storage.

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

bool ShouldShowPromoBasedOnImpressionCount(Profile& profile,
                                           AutofillSignInPromoType type) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile);

  // Show the promo if the user is sign in pending, regardless of impression
  // count.
  if (signin_util::IsSigninPending(identity_manager)) {
    return true;
  }

  // Don't show the promo again if it has already been shown
  // `kSigninPromoShownThreshold` times.
  AccountInfo account =
      signin_ui_util::GetSingleAccountForPromos(identity_manager);

  int show_count = 0;
  switch (type) {
    case AutofillSignInPromoType::kAddress:
      show_count =
          account.gaia.empty()
              ? profile.GetPrefs()->GetInteger(
                    prefs::kAddressSignInPromoShownCountPerProfile)
              : SigninPrefs(*profile.GetPrefs())
                    .GetAddressSigninPromoImpressionCount(account.gaia);
      break;
    case AutofillSignInPromoType::kPassword:
      show_count =
          account.gaia.empty()
              ? profile.GetPrefs()->GetInteger(
                    prefs::kPasswordSignInPromoShownCountPerProfile)
              : SigninPrefs(*profile.GetPrefs())
                    .GetPasswordSigninPromoImpressionCount(account.gaia);
  }

  return show_count < kSigninPromoShownThreshold;
}

AutofillSignInPromoType GetAutofillSignInPromoType(
    signin_metrics::AccessPoint access_point) {
  CHECK(signin::IsAutofillSigninPromo(access_point));

  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
      return AutofillSignInPromoType::kPassword;
    case signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE:
      return AutofillSignInPromoType::kAddress;
    default:
      NOTREACHED();
  }
}

}  // namespace
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace signin {

#if !BUILDFLAG(IS_ANDROID)
bool ShouldShowSyncPromo(Profile& profile) {
#if BUILDFLAG(IS_CHROMEOS)
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

  syncer::SyncPrefs prefs(profile.GetPrefs());
  // Don't show if sync is not allowed to start or is running in local mode.
  if (!SyncServiceFactory::IsSyncAllowed(&profile) ||
      prefs.IsLocalSyncEnabled()) {
    return false;
  }

  // Verified the base checks. Depending on whether the promo should be for sync
  // or signin, additional checks are necessary.
  return true;
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool ShouldShowPasswordSignInPromo(Profile& profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)

  if (!ShouldShowSignInPromoCommon(profile)) {
    return false;
  }

  if (!ShouldShowPromoBasedOnImpressionCount(
          profile, AutofillSignInPromoType::kPassword)) {
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

  if (!ShouldShowPromoBasedOnImpressionCount(
          profile, AutofillSignInPromoType::kAddress)) {
    return false;
  }

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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void RecordSignInPromoShown(signin_metrics::AccessPoint access_point,
                            Profile* profile) {
  CHECK(profile);

  AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(profile));
  AutofillSignInPromoType promo_type = GetAutofillSignInPromoType(access_point);

  // Record the pref per profile if there is no account present.
  if (account.gaia.empty()) {
    const char* pref_name;
    switch (promo_type) {
      case AutofillSignInPromoType::kPassword:
        pref_name = prefs::kPasswordSignInPromoShownCountPerProfile;
        break;
      case AutofillSignInPromoType::kAddress:
        pref_name = prefs::kAddressSignInPromoShownCountPerProfile;
        break;
    }

    int show_count = profile->GetPrefs()->GetInteger(pref_name);
    profile->GetPrefs()->SetInteger(pref_name, show_count + 1);
    return;
  }

  // Record the pref for the account that was used for the promo, either because
  // it is signed into the web or in sign in pending state.
  switch (promo_type) {
    case AutofillSignInPromoType::kPassword:
      SigninPrefs(*profile->GetPrefs())
          .IncrementPasswordSigninPromoImpressionCount(account.gaia);
      return;
    case AutofillSignInPromoType::kAddress:
      SigninPrefs(*profile->GetPrefs())
          .IncrementAddressSigninPromoImpressionCount(account.gaia);
  }
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
