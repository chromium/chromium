// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/signin_promo.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/features.h"
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
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace {

using signin::SignInPromoType;
using signin_util::SignedInState;

constexpr int kSigninPromoShownThreshold = 5;
constexpr int kSigninPromoDismissedThreshold = 2;

syncer::DataType GetDataTypeFromSignInPromoType(SignInPromoType type) {
  switch (type) {
    case SignInPromoType::kPassword:
      return syncer::PASSWORDS;
    case SignInPromoType::kAddress:
      return syncer::CONTACT_INFO;
    case SignInPromoType::kBookmark:
      return syncer::BOOKMARKS;
    case SignInPromoType::kExtension:
      NOTREACHED();
  }
}

bool ShouldShowPromoBasedOnImpressionOrDismissalCount(Profile& profile,
                                                      SignInPromoType type) {
  // Footer sign in promos are always shown.
  if (type == signin::SignInPromoType::kExtension ||
      type == signin::SignInPromoType::kBookmark) {
    return true;
  }

  AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(&profile));

  int show_count = 0;
  switch (type) {
    case SignInPromoType::kAddress:
      show_count =
          account.gaia.empty()
              ? profile.GetPrefs()->GetInteger(
                    prefs::kAddressSignInPromoShownCountPerProfile)
              : SigninPrefs(*profile.GetPrefs())
                    .GetAddressSigninPromoImpressionCount(account.gaia);
      break;
    case SignInPromoType::kPassword:
      show_count =
          account.gaia.empty()
              ? profile.GetPrefs()->GetInteger(
                    prefs::kPasswordSignInPromoShownCountPerProfile)
              : SigninPrefs(*profile.GetPrefs())
                    .GetPasswordSigninPromoImpressionCount(account.gaia);
      break;
    case SignInPromoType::kBookmark:
    case SignInPromoType::kExtension:
      NOTREACHED();
  }

  int dismiss_count =
      account.gaia.empty()
          ? profile.GetPrefs()->GetInteger(
                prefs::kAutofillSignInPromoDismissCountPerProfile)
          : SigninPrefs(*profile.GetPrefs())
                .GetAutofillSigninPromoDismissCount(account.gaia);

  // Don't show the promo again if it
  // - has already been shown `kSigninPromoShownThreshold` times for its
  // autofill bubble promo type.
  // - has already been dismissed `kSigninPromoDismissedThreshold` times,
  // regardless of autofill bubble promo type.
  return show_count < kSigninPromoShownThreshold &&
         dismiss_count < kSigninPromoDismissedThreshold;
}

// Performs base checks for whether the sign in promos should be shown.
// Needs additional checks depending on the type of the promo (see
// `ShouldShowAddressSignInPromo` and `ShouldShowPasswordSignInPromo`).
// `profile` is the profile of the tab the promo would be shown on.
bool ShouldShowSignInPromoCommon(Profile& profile, SignInPromoType type) {
  // Don't show the promo if it does not pass the sync base checks.
  if (!signin::ShouldShowSyncPromo(profile)) {
    return false;
  }

  // Don't show the promo if the user is off-the-record.
  if (profile.IsOffTheRecord()) {
    return false;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(&profile);
  syncer::DataType data_type = GetDataTypeFromSignInPromoType(type);

  // Don't show the promo if policies disallow account storage.
  if (sync_service->GetUserSettings()->IsTypeManagedByPolicy(
          GetUserSelectableTypeFromDataType(data_type).value()) ||
      !sync_service->GetDataTypesForTransportOnlyMode().Has(data_type)) {
    return false;
  }

  SignedInState signed_in_state = signin_util::GetSignedInState(
      IdentityManagerFactory::GetForProfile(&profile));

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

  return ShouldShowPromoBasedOnImpressionOrDismissalCount(profile, type);
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
  return ShouldShowSignInPromoCommon(profile, SignInPromoType::kPassword);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool ShouldShowAddressSignInPromo(Profile& profile,
                                  const autofill::AutofillProfile& address) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Don't show the promo if the new address is not eligible for account
  // storage.
  if (!autofill::IsProfileEligibleForMigrationToAccount(
          autofill::PersonalDataManagerFactory::GetForBrowserContext(&profile)
              ->address_data_manager(),
          address)) {
    return false;
  }

  return ShouldShowSignInPromoCommon(profile, SignInPromoType::kAddress);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool ShouldShowBookmarkSignInPromo(Profile& profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!base::FeatureList::IsEnabled(
          switches::kSyncEnableBookmarksInTransportMode)) {
    return false;
  }

  return ShouldShowSignInPromoCommon(profile, SignInPromoType::kBookmark);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool IsAutofillSigninPromo(signin_metrics::AccessPoint access_point) {
  return access_point == signin_metrics::AccessPoint::kPasswordBubble ||
         access_point == signin_metrics::AccessPoint::kAddressBubble;
}

bool IsSignInPromo(signin_metrics::AccessPoint access_point) {
  if (IsAutofillSigninPromo(access_point)) {
    return true;
  }

  if (access_point == signin_metrics::AccessPoint::kExtensionInstallBubble) {
    return base::FeatureList::IsEnabled(
        switches::kEnableExtensionsExplicitBrowserSignin);
  }

  return false;
}

SignInPromoType GetSignInPromoTypeFromAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kPasswordBubble:
      return SignInPromoType::kPassword;
    case signin_metrics::AccessPoint::kAddressBubble:
      return SignInPromoType::kAddress;
    case signin_metrics::AccessPoint::kBookmarkBubble:
      return SignInPromoType::kBookmark;
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
      return SignInPromoType::kExtension;
    default:
      NOTREACHED();
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void RecordSignInPromoShown(signin_metrics::AccessPoint access_point,
                            Profile* profile) {
  CHECK(profile);
  CHECK(!profile->IsOffTheRecord());

  AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(profile));
  SignInPromoType promo_type = GetSignInPromoTypeFromAccessPoint(access_point);

  // Record the pref per profile if there is no account present.
  if (account.gaia.empty()) {
    const char* pref_name;
    switch (promo_type) {
      case SignInPromoType::kPassword:
        pref_name = prefs::kPasswordSignInPromoShownCountPerProfile;
        break;
      case SignInPromoType::kAddress:
        pref_name = prefs::kAddressSignInPromoShownCountPerProfile;
        break;
      case SignInPromoType::kBookmark:
      case SignInPromoType::kExtension:
        return;
    }

    int show_count = profile->GetPrefs()->GetInteger(pref_name);
    profile->GetPrefs()->SetInteger(pref_name, show_count + 1);
    return;
  }

  // Record the pref for the account that was used for the promo, either because
  // it is signed into the web or in sign in pending state.
  switch (promo_type) {
    case SignInPromoType::kPassword:
      SigninPrefs(*profile->GetPrefs())
          .IncrementPasswordSigninPromoImpressionCount(account.gaia);
      return;
    case SignInPromoType::kAddress:
      SigninPrefs(*profile->GetPrefs())
          .IncrementAddressSigninPromoImpressionCount(account.gaia);
      return;
    case SignInPromoType::kBookmark:
    case SignInPromoType::kExtension:
      return;
  }
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
