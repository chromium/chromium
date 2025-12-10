// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo_util.h"

#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/features.h"
#include "components/sync_bookmarks/switches.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/base/network_change_notifier.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "components/sync/service/sync_prefs.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/sync/extension_sync_util.h"
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
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace signin {

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

using signin::SignInPromoType;
using signin_util::SignedInState;

constexpr int kSigninPromoShownThreshold = 5;
constexpr int kSigninPromoDismissedThreshold = 2;

// Prefs that are part of the dictionary from
// `SigninPrefs::GetOrCreateAvatarButtonPromoCountDictionary()` that maps the
// used and shown counts for the promos listed in
// `ProfileMenuAvatarButtonPromoInfo::Type` (Except for
// `ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo`).
constexpr char kAvatarButtonHistorySyncPromoShownCount[] =
    "AvatarButtonHistorySyncPromoShownCount";
constexpr char kAvatarButtonHistorySyncPromoUsedCount[] =
    "AvatarButtonHistorySyncPromoUsedCount";
constexpr char kAvatarButtonBatchUploadPromoShownCount[] =
    "AvatarButtonBatchUploadPromoShownCount";
constexpr char kAvatarButtonBatchUploadPromoUsedCount[] =
    "AvatarButtonBatchUploadPromoUsedCount";
constexpr char kAvatarButtonBatchUploadBookmarkPromoShownCount[] =
    "AvatarButtonBatchUploadBookmarkPromoShownCount";
constexpr char kAvatarButtonBatchUploadBookmarkPromoUsedCount[] =
    "AvatarButtonBatchUploadBookmarkPromoUsedCount";
constexpr char kAvatarButtonBatchUploadWindows10DepreciationPromoShownCount[] =
    "AvatarButtonBatchUploadWindows10DepreciationPromoShownCount";
constexpr char kAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount[] =
    "AvatarButtonBatchUploadWindows10DepreciationPromoUsedCount";

const char* GetAvatarButtonPromoShownKey(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  switch (promo_type) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      return kAvatarButtonHistorySyncPromoShownCount;
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      return kAvatarButtonBatchUploadPromoShownCount;
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
      return kAvatarButtonBatchUploadBookmarkPromoShownCount;
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
      CHECK(switches::IsSigninWindows10DepreciationState());
      return kAvatarButtonBatchUploadWindows10DepreciationPromoShownCount;
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      NOTREACHED() << "SyncPromo uses the SigninPrefs values directly";
  }
}

const char* GetAvatarButtonPromoUsedKey(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  switch (promo_type) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      return kAvatarButtonHistorySyncPromoUsedCount;
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      return kAvatarButtonBatchUploadPromoUsedCount;
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
      return kAvatarButtonBatchUploadBookmarkPromoUsedCount;
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
      CHECK(switches::IsSigninWindows10DepreciationState());
      return kAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount;
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      NOTREACHED() << "SyncPromo uses the SigninPrefs values directly";
  }
}

// Returns the Shown/Used count pair for `promo_type`.
std::pair<int, int> GetPromoUsageCounts(
    SigninPrefs signin_prefs,
    ProfileMenuAvatarButtonPromoInfo::Type promo_type,
    GaiaId gaia) {
  if (promo_type == ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo) {
    CHECK(switches::IsAvatarSyncPromoFeatureEnabled());
    return {signin_prefs.GetSyncPromoIdentityPillShownCount(gaia),
            signin_prefs.GetSyncPromoIdentityPillUsedCount(gaia)};
  }

  base::DictValue& promo_counts =
      signin_prefs.GetOrCreateAvatarButtonPromoCountDictionary(gaia);

  return {promo_counts.FindInt(GetAvatarButtonPromoShownKey(promo_type))
              .value_or(0),
          promo_counts.FindInt(GetAvatarButtonPromoUsedKey(promo_type))
              .value_or(0)};
}

bool WasPreviouslySyncingWithPrimaryAccount(Profile* profile) {
  const GaiaId last_syncing_gaia_id(
      profile->GetPrefs()->GetString(prefs::kGoogleServicesLastSyncingGaiaId));
  if (last_syncing_gaia_id.empty()) {
    return false;
  }

  const GaiaId primary_account_gaia_id =
      IdentityManagerFactory::GetForProfile(profile)
          ->GetPrimaryAccountInfo(ConsentLevel::kSignin)
          .gaia;
  if (primary_account_gaia_id.empty()) {
    return false;
  }

  return last_syncing_gaia_id == primary_account_gaia_id;
}

void ComputeProfileMenuAvatarButtonPromoInfoWithBatchUploadResult(
    Profile* profile,
    base::OnceCallback<void(ProfileMenuAvatarButtonPromoInfo)> result_callback,
    std::map<syncer::DataType, syncer::LocalDataDescription> local_map_result) {
  CHECK(
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos));

  size_t local_data_count = std::accumulate(
      local_map_result.begin(), local_map_result.end(), 0u,
      [](size_t current_count,
         std::pair<syncer::DataType, syncer::LocalDataDescription> local_data) {
        return current_count + local_data.second.local_data_models.size();
      });

  // Batch Upload promo: Windows 10 depreciation promo.
  if (local_data_count > 0 && switches::IsSigninWindows10DepreciationState()) {
    std::move(result_callback)
        .Run(ProfileMenuAvatarButtonPromoInfo{
            .type = ProfileMenuAvatarButtonPromoInfo::Type::
                kBatchUploadWindows10DepreciationPromo,
            .local_data_count = local_data_count});
    return;
  }

  // Batch Upload Bookmarks promo: for users that have local bookmarks and were
  // previously syncing with the current primary account.
  if (WasPreviouslySyncingWithPrimaryAccount(profile)) {
    if (auto it = local_map_result.find(syncer::BOOKMARKS);
        it != local_map_result.end() && !it->second.local_data_models.empty()) {
      std::move(result_callback)
          .Run(ProfileMenuAvatarButtonPromoInfo{
              .type = ProfileMenuAvatarButtonPromoInfo::Type::
                  kBatchUploadBookmarksPromo,
              .local_data_count = local_data_count});
      return;
    }
  }
  // History sync promo.
  if (signin_util::ShouldShowHistorySyncOptinScreen(*profile) ==
          signin_util::ShouldShowHistorySyncOptinResult::kShow &&
      !signin_util::HasExplicitlyDisabledHistorySync(
          SyncServiceFactory::GetForProfile(profile),
          IdentityManagerFactory::GetForProfile(profile))) {
    std::move(result_callback)
        .Run(ProfileMenuAvatarButtonPromoInfo{
            .type = ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
            .local_data_count = local_data_count});
    return;
  }

  // Regular Batch Upload promo: for users that have any local data type.
  if (local_data_count > 0) {
    std::move(result_callback)
        .Run(ProfileMenuAvatarButtonPromoInfo{
            .type = ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
            .local_data_count = local_data_count});
    return;
  }

  // No promo.
  std::move(result_callback)
      .Run(ProfileMenuAvatarButtonPromoInfo{
          .type = std::nullopt, .local_data_count = local_data_count});
  return;
}

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

int GetAddressPromoShownCount(Profile& profile, const GaiaId& gaia_id) {
  if (!gaia_id.empty()) {
    return SigninPrefs(*profile.GetPrefs())
        .GetAddressSigninPromoImpressionCount(gaia_id);
  }

  return profile.GetPrefs()->GetInteger(
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? prefs::kAddressSignInPromoShownCountPerProfileForLimitsExperiment
          : prefs::kAddressSignInPromoShownCountPerProfile);
}

int GetPasswordPromoShownCount(Profile& profile, const GaiaId& gaia_id) {
  if (!gaia_id.empty()) {
    return SigninPrefs(*profile.GetPrefs())
        .GetPasswordSigninPromoImpressionCount(gaia_id);
  }

  return profile.GetPrefs()->GetInteger(
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? prefs::kPasswordSignInPromoShownCountPerProfileForLimitsExperiment
          : prefs::kPasswordSignInPromoShownCountPerProfile);
}

int GetBookmarkPromoShownCount(Profile& profile, const GaiaId& gaia_id) {
  if (!gaia_id.empty()) {
    return SigninPrefs(*profile.GetPrefs())
        .GetBookmarkSigninPromoImpressionCount(gaia_id);
  }

  return profile.GetPrefs()->GetInteger(
      base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
          ? prefs::kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment
          : prefs::kBookmarkSignInPromoShownCountPerProfile);
}

int GetContextualPromoDismissCountPerSignedOutProfile(Profile& profile,
                                                      SignInPromoType type) {
  if (!base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)) {
    return profile.GetPrefs()->GetInteger(
        prefs::kAutofillSignInPromoDismissCountPerProfile);
  }

  switch (type) {
    case SignInPromoType::kAddress:
      return profile.GetPrefs()->GetInteger(
          prefs::kAddressSignInPromoDismissCountPerProfileForLimitsExperiment);
    case SignInPromoType::kPassword:
      return profile.GetPrefs()->GetInteger(
          prefs::kPasswordSignInPromoDismissCountPerProfileForLimitsExperiment);
    case SignInPromoType::kBookmark:
      return profile.GetPrefs()->GetInteger(
          prefs::kBookmarkSignInPromoDismissCountPerProfileForLimitsExperiment);
    case SignInPromoType::kExtension:
      NOTREACHED();
  }
}

int GetContextualPromoDismissCountPerAccount(Profile& profile,
                                             SignInPromoType type,
                                             const GaiaId& gaia_id) {
  if (!base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)) {
    return SigninPrefs(*profile.GetPrefs())
        .GetAutofillSigninPromoDismissCount(gaia_id);
  }

  switch (type) {
    case SignInPromoType::kAddress:
      return SigninPrefs(*profile.GetPrefs())
          .GetAddressSigninPromoDismissCount(gaia_id);
    case SignInPromoType::kPassword:
      return SigninPrefs(*profile.GetPrefs())
          .GetPasswordSigninPromoDismissCount(gaia_id);
    case SignInPromoType::kBookmark:
      return SigninPrefs(*profile.GetPrefs())
          .GetBookmarkSigninPromoDismissCount(gaia_id);
    case SignInPromoType::kExtension:
      NOTREACHED();
  }
}

bool ShouldShowPromoBasedOnImpressionOrDismissalCount(Profile& profile,
                                                      SignInPromoType type) {
  // Footer sign in promos are always shown.
  if (type == signin::SignInPromoType::kExtension ||
      (type == signin::SignInPromoType::kBookmark &&
       !base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp))) {
    return true;
  }

  AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(&profile));

  int show_count = 0;
  switch (type) {
    case SignInPromoType::kAddress:
      show_count = GetAddressPromoShownCount(profile, account.gaia);
      break;
    case SignInPromoType::kPassword:
      show_count = GetPasswordPromoShownCount(profile, account.gaia);
      break;
    case SignInPromoType::kBookmark:
      if (!base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
        NOTREACHED();
      }
      show_count = GetBookmarkPromoShownCount(profile, account.gaia);
      break;
    case SignInPromoType::kExtension:
      NOTREACHED();
  }

  int dismiss_count =
      account.gaia.empty()
          ? GetContextualPromoDismissCountPerSignedOutProfile(profile, type)
          : GetContextualPromoDismissCountPerAccount(profile, type,
                                                     account.gaia);

  if (base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)) {
    return show_count < switches::kContextualSigninPromoShownThreshold.Get() &&
           dismiss_count <
               switches::kContextualSigninPromoDismissedThreshold.Get();
  }

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
  if (profile.IsOffTheRecord()) {
    return false;
  }

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

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

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

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(original_profile);
  AccountInfo promo_account =
      profile.IsOffTheRecord()
          ? AccountInfo()  // Incognito profiles do not personalize promos.
          : signin_ui_util::GetSingleAccountForPromos(identity_manager);

  // Don't show if sign in can't be offered (ex: signin disallowed).
  if (!CanOfferSignin(original_profile, promo_account.gaia, promo_account.email,
                      /*allow_account_from_other_profile=*/true)
           .IsOk()) {
    return false;
  }

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

  if (!base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
    // `ShouldShowSyncPromo()` does not check if extensions are syncing in
    // transport mode. That's why `IsSyncingExtensionsEnabled()` is added so the
    // sign in promo is not shown in that case.
    if (extensions::sync_util::IsSyncingExtensionsEnabled(&profile)) {
      return false;
    }

    // The promo is not shown to users that have explicitly signed in through
    // the browser (even if extensions are not syncing).
    if (profile.GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin)) {
      return false;
    }
  }

  return true;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool ShouldShowExtensionSignInPromo(Profile& profile,
                                    const extensions::Extension& extension) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!switches::IsExtensionsExplicitBrowserSigninEnabled()) {
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

bool IsBubbleSigninPromo(signin_metrics::AccessPoint access_point) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  return access_point == signin_metrics::AccessPoint::kPasswordBubble ||
         access_point == signin_metrics::AccessPoint::kAddressBubble ||
         (base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp) &&
          access_point == signin_metrics::AccessPoint::kBookmarkBubble);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool IsSignInPromo(signin_metrics::AccessPoint access_point) {
  if (IsBubbleSigninPromo(access_point)) {
    return true;
  }

  if (access_point == signin_metrics::AccessPoint::kExtensionInstallBubble) {
    return switches::IsExtensionsExplicitBrowserSigninEnabled();
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
        pref_name =
            base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
                ? prefs::
                      kPasswordSignInPromoShownCountPerProfileForLimitsExperiment
                : prefs::kPasswordSignInPromoShownCountPerProfile;
        break;
      case SignInPromoType::kAddress:
        pref_name =
            base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
                ? prefs::
                      kAddressSignInPromoShownCountPerProfileForLimitsExperiment
                : prefs::kAddressSignInPromoShownCountPerProfile;
        break;
      case SignInPromoType::kBookmark:
        if (!base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
          return;
        }
        pref_name =
            base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)
                ? prefs::
                      kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment
                : prefs::kBookmarkSignInPromoShownCountPerProfile;
        break;
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
      if (base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
        SigninPrefs(*profile->GetPrefs())
            .IncrementBookmarkSigninPromoImpressionCount(account.gaia);
      }
      return;
    case SignInPromoType::kExtension:
      return;
  }
}

void RecordAvatarButtonPromoAcceptedAtPromoShownCount(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type,
    signin::IdentityManager* identity_manager,
    PrefService& prefs) {
  GaiaId primary_gaia =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  CHECK(!primary_gaia.empty());

  constexpr char kAvatarPillPromoAcceptedAtShownCountBaseHistogram[] =
      "Signin.AvatarPillPromo.AcceptedAtShownCount.";

  std::string_view promo_type_suffix;
  switch (promo_type) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      CHECK(switches::IsAvatarSyncPromoFeatureEnabled());
      promo_type_suffix = "Sync";
      break;
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      promo_type_suffix = "HistorySync";
      break;
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      promo_type_suffix = "BatchUpload";
      break;
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
      promo_type_suffix = "BatchUploadBookmarks";
      break;
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
      promo_type_suffix = "BatchUploadWindows10Depreciation";
      break;
  }

  int promo_shown_count =
      GetPromoUsageCounts(SigninPrefs(prefs), promo_type, primary_gaia).first;
  base::UmaHistogramExactLinear(
      base::StrCat({kAvatarPillPromoAcceptedAtShownCountBaseHistogram,
                    promo_type_suffix}),
      promo_shown_count,
      /*exclusive_max=*/user_education::features::GetNewBadgeShowCount() + 1);
}

void ComputeProfileMenuAvatarButtonPromoInfo(
    Profile& profile,
    base::OnceCallback<void(ProfileMenuAvatarButtonPromoInfo)>
        result_callback) {
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    // Note: `GetLocalDataDescriptionsForAvailableTypes()` will return no data
    // if the SyncService is not initialized.
    BatchUploadServiceFactory::GetForProfile(&profile)
        ->GetLocalDataDescriptionsForAvailableTypes(base::BindOnce(
            &ComputeProfileMenuAvatarButtonPromoInfoWithBatchUploadResult,
            &profile, std::move(result_callback)));
    return;
  }

  // This promo is only possible if `syncer::kReplaceSyncPromosWithSignInPromos`
  // is disabled, as it promotes Sync.
  if (switches::IsAvatarSyncPromoFeatureEnabled() &&
      signin_util::ShouldShowAvatarSyncPromo(&profile)) {
    std::move(result_callback)
        .Run(ProfileMenuAvatarButtonPromoInfo{
            .type = ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo,
            .local_data_count = 0});
    return;
  }

  // `profile` is not eligible to any promo.
  std::move(result_callback).Run(ProfileMenuAvatarButtonPromoInfo());
}

SyncPromoIdentityPillManager::SyncPromoIdentityPillManager(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service)
    : SyncPromoIdentityPillManager(
          identity_manager,
          pref_service,
          user_education::features::GetNewBadgeShowCount(),
          user_education::features::GetNewBadgeFeatureUsedCount()) {}

SyncPromoIdentityPillManager::SyncPromoIdentityPillManager(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    int max_shown_count,
    int max_used_count)
    : identity_manager_(identity_manager),
      signin_prefs_(std::make_unique<SigninPrefs>(CHECK_DEREF(pref_service))),
      max_shown_count_(max_shown_count),
      max_used_count_(max_used_count) {
  CHECK(identity_manager_);
  identity_manager_scoped_observation_.Observe(identity_manager_);
}

SyncPromoIdentityPillManager::~SyncPromoIdentityPillManager() = default;

bool SyncPromoIdentityPillManager::ShouldShowPromo(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  const AccountInfo account = GetSignedInAccountInfo();
  if (account.gaia.empty()) {
    // If there is no account available, the promo should not be shown (the sync
    // promo should be shown only for signed in users).
    return false;
  }
  if (!ArePromotionsEnabled()) {
    return false;
  }

  CHECK(signin_prefs_);
  auto [promo_shown_count, promo_used_count] =
      GetPromoUsageCounts(*signin_prefs_.get(), promo_type, account.gaia);
  return promo_shown_count < max_shown_count_ &&
         promo_used_count < max_used_count_;
}

void SyncPromoIdentityPillManager::RecordPromoShown(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  const AccountInfo account = GetSignedInAccountInfo();
  if (account.gaia.empty()) {
    // If there is no account available, there is nothing to record (the sync
    // promo should be shown only for signed in users).
    return;
  }

  CHECK(signin_prefs_);
  if (promo_type == ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo) {
    CHECK(switches::IsAvatarSyncPromoFeatureEnabled());
    signin_prefs_->IncrementSyncPromoIdentityPillShownCount(account.gaia);
    return;
  }

  base::DictValue& promo_counts =
      signin_prefs_->GetOrCreateAvatarButtonPromoCountDictionary(account.gaia);
  const char* shown_key = GetAvatarButtonPromoShownKey(promo_type);
  int new_conut = promo_counts.FindInt(shown_key).value_or(0) + 1;
  promo_counts.Set(shown_key, new_conut);
}

void SyncPromoIdentityPillManager::RecordPromoUsed(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  const AccountInfo account = GetSignedInAccountInfo();
  if (account.gaia.empty()) {
    // If there is no account available, there is nothing to record (the sync
    // promo should be shown only for signed in users).
    return;
  }

  CHECK(signin_prefs_);
  if (promo_type == ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo) {
    CHECK(switches::IsAvatarSyncPromoFeatureEnabled());
    signin_prefs_->IncrementSyncPromoIdentityPillUsedCount(account.gaia);
    return;
  }

  base::DictValue& promo_counts =
      signin_prefs_->GetOrCreateAvatarButtonPromoCountDictionary(account.gaia);
  const char* used_key = GetAvatarButtonPromoUsedKey(promo_type);
  int new_conut = promo_counts.FindInt(used_key).value_or(0) + 1;
  promo_counts.Set(used_key, new_conut);
}

bool SyncPromoIdentityPillManager::ArePromotionsEnabled() const {
  PrefService* local_state = g_browser_process->local_state();
  return local_state && local_state->GetBoolean(prefs::kPromotionsEnabled);
}

void SyncPromoIdentityPillManager::OnIdentityManagerShutdown(
    IdentityManager* identity_manager) {
  CHECK_EQ(identity_manager, identity_manager_.get());
  identity_manager_ = nullptr;
  identity_manager_scoped_observation_.Reset();

  // `SyncPromoIdentityPillManager::OnIdentityManagerShutdown()` is called upon
  // profile destruction, which aligns with the need to clear the prefs. Since
  // currently there is reliable way to be notified by the pref service shutting
  // down, we rely on this notification as well.
  // The need to clear the prefs here is primarily for unit tests that combines
  // `Browser` + `TestingProfile` (where the `PrefService` is owned by the
  // profile itself).
  signin_prefs_.reset();
}

AccountInfo SyncPromoIdentityPillManager::GetSignedInAccountInfo() const {
  CHECK(identity_manager_);
  CHECK(identity_manager_->AreRefreshTokensLoaded());
  // Checks for accounts in error as well.
  if (signin_util::GetSignedInState(identity_manager_.get()) !=
      signin_util::SignedInState::kSignedIn) {
    return AccountInfo();
  }
  return identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
