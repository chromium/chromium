// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo_util.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/features.h"
#include "components/sync_bookmarks/switches.h"
#include "net/base/network_change_notifier.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/sync/service/sync_prefs.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_sync_util.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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
#include "components/user_education/common/user_education_features.h"

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
      return syncer::EXTENSIONS;
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

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(&profile);

  // Don't show the promo if the sync service is not available, e.g. if the
  // profile is off-the-record.
  if (!sync_service) {
    return false;
  }
  CHECK(!profile.IsOffTheRecord());

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

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool ShouldShowExtensionSyncPromo(Profile& profile,
                                  const extensions::Extension& extension) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Don't show the promo if it does not pass the sync base checks.
  if (!signin::ShouldShowSyncPromo(profile)) {
    return false;
  }

  if (!extensions::sync_util::ShouldSync(&profile, &extension)) {
    return false;
  }

  // `ShouldShowSyncPromo()` does not check if extensions are syncing in
  // transport mode. That's why `IsSyncingExtensionsEnabled()` is added so the
  // sign in promo is not shown in that case.
  if (extensions::sync_util::IsSyncingExtensionsEnabled(&profile)) {
    return false;
  }

  // The promo is not shown to users that have explicitly signed in through the
  // browser (even if extensions are not syncing).
  if (profile.GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin)) {
    return false;
  }

  return true;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool ShouldShowExtensionSignInPromo(Profile& profile,
                                    const extensions::Extension& extension) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!base::FeatureList::IsEnabled(
          switches::kEnableExtensionsExplicitBrowserSignin)) {
    return false;
  }

  if (!ShouldShowExtensionSyncPromo(profile, extension)) {
    return false;
  }

  return ShouldShowSignInPromoCommon(profile, SignInPromoType::kExtension);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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

  // Do not show the promo if a user was previously syncing, as this may result
  // in duplicate data.
  // TODO(crbug.com/402748138): Remove this once bookmarks de-duplication is
  // implemented.
  if (!profile.GetPrefs()
           ->GetString(::prefs::kGoogleServicesLastSyncingGaiaId)
           .empty()) {
    return false;
  }

  if (!ShouldShowSignInPromoCommon(profile, SignInPromoType::kBookmark)) {
    return false;
  }

  // At this point, both the identity manager and sync service should not be
  // null.
  IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(&profile);
  CHECK(identity_manager);
  CHECK(sync_service);

  // If the user is in sign in pending state, the promo should only be shown if
  // they already have account storage for bookmarks enabled.
  return !signin_util::IsSigninPending(identity_manager) ||
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kBookmarks);
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

  if (access_point == signin_metrics::AccessPoint::kBookmarkBubble) {
    return base::FeatureList::IsEnabled(
        switches::kSyncEnableBookmarksInTransportMode);
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

SyncPromoIdentityPillManager::SyncPromoIdentityPillManager(Profile& profile)
    : SyncPromoIdentityPillManager(
          profile,
          user_education::features::GetNewBadgeShowCount(),
          user_education::features::GetNewBadgeFeatureUsedCount()) {}

SyncPromoIdentityPillManager::SyncPromoIdentityPillManager(Profile& profile,
                                                           int max_shown_count,
                                                           int max_used_count)
    : profile_(profile),
      max_shown_count_(max_shown_count),
      max_used_count_(max_used_count) {}

bool SyncPromoIdentityPillManager::ShouldShowPromo() const {
  const AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(&profile_.get()));
  if (account.gaia.empty()) {
    // If there is no account available, the promo should not be shown (the sync
    // promo should be shown only for signed in users).
    return false;
  }
  if (!ArePromotionsEnabled()) {
    return false;
  }
  const int show_count = SigninPrefs(*profile_->GetPrefs())
                             .GetSyncPromoIdentityPillShownCount(account.gaia);
  const int used_count = SigninPrefs(*profile_->GetPrefs())
                             .GetSyncPromoIdentityPillUsedCount(account.gaia);
  return show_count < max_shown_count_ && used_count < max_used_count_;
}

void SyncPromoIdentityPillManager::RecordPromoShown() {
  const AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(&profile_.get()));
  if (account.gaia.empty()) {
    // If there is no account available, there is nothing to record (the sync
    // promo should be shown only for signed in users).
    return;
  }
  SigninPrefs(*profile_->GetPrefs())
      .IncrementSyncPromoIdentityPillShownCount(account.gaia);
}

void SyncPromoIdentityPillManager::RecordPromoUsed() {
  const AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(&profile_.get()));
  if (account.gaia.empty()) {
    // If there is no account available, there is nothing to record (the sync
    // promo should be shown only for signed in users).
    return;
  }
  SigninPrefs(*profile_->GetPrefs())
      .IncrementSyncPromoIdentityPillUsedCount(account.gaia);
}

bool SyncPromoIdentityPillManager::ArePromotionsEnabled() const {
  PrefService* local_state = g_browser_process->local_state();
  return local_state && local_state->GetBoolean(prefs::kPromotionsEnabled);
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
