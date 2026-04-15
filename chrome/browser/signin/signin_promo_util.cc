// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo_util.h"

#include <optional>
#include <string_view>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
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
#include "components/signin/public/base/signin_switches.h"
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

// Profile based dictionary for AvatarButton promos.
constexpr char kAvatarButtonPromoProfileDictionary[] =
    "signin.avatar_button_promo_dict";

// The following prefs are mapped to the used and shown counts of the promos
// listed in `ProfileMenuAvatarButtonPromoInfo::Type` (Except for
// `ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo`). Some promos can be
// tied to an account/`GaiaId` (through
// `SigninPrefs::GetOrCreateAvatarButtonPromoCountDictionary()` dictionary), or
// to the Profile directly through `kAvatarButtonPromoProfileDictionary`
// dictionary). Some prefs can also be mapped to both at the same time, in this
// case we use the same pref, but it will depend on which dictionary it will be
// stored.
//
// The following prefs are only attached to a `GaiaId`.
constexpr std::string_view kAvatarButtonHistorySyncPromoShownCount =
    "AvatarButtonHistorySyncPromoShownCount";
constexpr std::string_view kAvatarButtonHistorySyncPromoUsedCount =
    "AvatarButtonHistorySyncPromoUsedCount";
constexpr std::string_view kAvatarButtonBatchUploadPromoShownCount =
    "AvatarButtonBatchUploadPromoShownCount";
constexpr std::string_view kAvatarButtonBatchUploadPromoUsedCount =
    "AvatarButtonBatchUploadPromoUsedCount";
constexpr std::string_view kAvatarButtonBatchUploadBookmarkPromoShownCount =
    "AvatarButtonBatchUploadBookmarkPromoShownCount";
constexpr std::string_view kAvatarButtonBatchUploadBookmarkPromoUsedCount =
    "AvatarButtonBatchUploadBookmarkPromoUsedCount";
constexpr std::string_view
    kAvatarButtonBatchUploadWindows10DepreciationPromoShownCount =
        "AvatarButtonBatchUploadWindows10DepreciationPromoShownCount";
constexpr std::string_view
    kAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount =
        "AvatarButtonBatchUploadWindows10DepreciationPromoUsedCount";
// The following prefs can be attached to a `GaiaId` or the Profile.
constexpr std::string_view kAvatarButtonSigninPromoShownCount =
    "AvatarButtonSigninPromoShownCount";
constexpr std::string_view kAvatarButtonSigninPromoUsedCount =
    "AvatarButtonSigninPromoUsedCount";
constexpr std::string_view kAvatarButtonSigninPromoLastShownTime =
    "AvatarButtonSigninPromoLastShownTime";

std::string_view GetAvatarButtonPromoShownKey(
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
    case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      return kAvatarButtonSigninPromoShownCount;
  }
}

std::string_view GetAvatarButtonPromoUsedKey(
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
    case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      return kAvatarButtonSigninPromoUsedCount;
  }
}

// Returns the Gaia tied dictionary or the global profile dictionary for the
// promo prefs.
base::DictValue& GetPromoDictionary(PrefService& pref_service,
                                    SigninPrefs& signin_prefs,
                                    const GaiaId& gaia) {
  if (gaia.empty()) {
    return ScopedDictPrefUpdate(pref_service,
                                kAvatarButtonPromoProfileDictionary)
        .Get();
  }
  return signin_prefs.GetOrCreateAvatarButtonPromoCountDictionary(gaia);
}

// May return `std::nullopt` when the `promo_type` does not depend on the shown
// time of the promo.
std::optional<std::string_view> MaybeGetLastShownTimePref(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  switch (promo_type) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      // Those promos do not need to record the shown time pref as deciding to
      // show the promo does not depend on it.
      return std::nullopt;
    case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      return kAvatarButtonSigninPromoLastShownTime;
  }
}

base::TimeDelta GetMinimumThresholdSinceLastShownTime(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  switch (promo_type) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      NOTREACHED() << "The promo does not support shown time checking.";
    case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      return switches::kSigninPromoOnAvatarPillDelayForNextPromoAllowed.Get();
  }
}

base::TimeDelta GetMinimumThresholdSinceLastEventTime(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  switch (promo_type) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      NOTREACHED() << "The promo does not support last event time checking.";
    case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      // Explicitly uses the same value as the threshold last shown time to
      // simulate bumping the delay because of a similar event.
      return GetMinimumThresholdSinceLastShownTime(promo_type);
  }
}

std::optional<base::Time> MaybeGetLastExternalEventTime(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type,
    const SigninPrefs& signin_prefs,
    GaiaId gaia) {
  switch (promo_type) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      // These promos does not support external event time checking.
      return std::nullopt;
    case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      return gaia.empty()
                 ? std::nullopt
                 : signin_prefs
                       .GetChromeSigninInterceptionLastBubbleDeclineTime(gaia);
  }
}

struct PromoUsageInfo {
  int shown_count = 0;
  int used_count = 0;
  // Optional as not every promo type supports it.
  std::optional<base::Time> last_shown_time = std::nullopt;
  // Optional as not every promo type supports it.
  std::optional<base::Time> last_external_event_time = std::nullopt;
};

PromoUsageInfo GetPromoUsageInfo(
    PrefService& pref_service,
    SigninPrefs& signin_prefs,
    ProfileMenuAvatarButtonPromoInfo::Type promo_type,
    GaiaId gaia) {
  if (promo_type == ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo) {
    CHECK(switches::IsAvatarSyncPromoFeatureEnabled());
    return {
        .shown_count = signin_prefs.GetSyncPromoIdentityPillShownCount(gaia),
        .used_count = signin_prefs.GetSyncPromoIdentityPillUsedCount(gaia)};
  }

  std::string_view shown_key = GetAvatarButtonPromoShownKey(promo_type);
  std::string_view used_key = GetAvatarButtonPromoUsedKey(promo_type);
  base::DictValue& promo_dict =
      GetPromoDictionary(pref_service, signin_prefs, gaia);

  PromoUsageInfo usage_info{
      .shown_count = promo_dict.FindInt(shown_key).value_or(0),
      .used_count = promo_dict.FindInt(used_key).value_or(0)};

  if (std::optional<std::string_view> last_shown_time_pref =
          MaybeGetLastShownTimePref(promo_type);
      last_shown_time_pref.has_value()) {
    if (base::Value* last_shown_time =
            promo_dict.Find(last_shown_time_pref.value())) {
      usage_info.last_shown_time = base::ValueToTime(*last_shown_time);
    }
  }

  usage_info.last_external_event_time =
      MaybeGetLastExternalEventTime(promo_type, signin_prefs, gaia);

  return usage_info;
}

bool IsAllowedByPromoFrequency(Profile& profile,
                               SignInPromoType type,
                               const GaiaId& gaia_id) {
  switch (type) {
    case SignInPromoType::kPassword:
    case SignInPromoType::kAddress:
    case SignInPromoType::kBookmark:
    case SignInPromoType::kExtension:
      // No specific frequency exists for this promo type.
      return true;
    case SignInPromoType::kSearchAIMode:
      break;
  }
  // For the Search AI Mode there should be a gap between impressions,
  // configured in `kSearchAIModePromoFrequency`.
  std::optional<base::Time> last_impression_time;
  if (gaia_id.empty()) {
    last_impression_time = profile.GetPrefs()->GetTime(
        prefs::kSearchAIModeSignInPromoLastImpressionTimestampPerProfile);
  } else {
    SigninPrefs signin_prefs(*profile.GetPrefs());
    last_impression_time =
        signin_prefs.GetSearchAIModeSigninPromoLastImpressionTime(gaia_id);
  }
  if (!last_impression_time.has_value()) {
    return true;
  }
  base::TimeDelta gap = base::Time::Now() - last_impression_time.value();
  return (gap >= switches::kSearchAIModePromoFrequency.Get());
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

ProfileMenuAvatarButtonPromoInfo
ComputeProfileMenuAvatarButtonPromoInfoWithBatchUploadResult(
    Profile* profile,
    std::map<syncer::DataType, syncer::LocalDataDescription> local_map_result) {
  CHECK(syncer::IsReplaceSyncPromosWithSignInPromosEnabled());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (base::FeatureList::IsEnabled(switches::kSigninPromoOnAvatarPill) &&
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return {.type = ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo,
            .local_data_count = 0u};
  }

  size_t local_data_count = std::accumulate(
      local_map_result.begin(), local_map_result.end(), 0u,
      [](size_t current_count,
         std::pair<syncer::DataType, syncer::LocalDataDescription> local_data) {
        return current_count + local_data.second.local_data_models.size();
      });

  // Batch Upload promo: Windows 10 depreciation promo.
  if (local_data_count > 0 && switches::IsSigninWindows10DepreciationState()) {
    return {.type = ProfileMenuAvatarButtonPromoInfo::Type::
                kBatchUploadWindows10DepreciationPromo,
            .local_data_count = local_data_count};
  }

  // Batch Upload Bookmarks promo: for users that have local bookmarks and were
  // previously syncing with the current primary account.
  if (WasPreviouslySyncingWithPrimaryAccount(profile)) {
    if (auto it = local_map_result.find(syncer::BOOKMARKS);
        it != local_map_result.end() && !it->second.local_data_models.empty()) {
      return {.type = ProfileMenuAvatarButtonPromoInfo::Type::
                  kBatchUploadBookmarksPromo,
              .local_data_count = local_data_count};
    }
  }
  // History sync promo.
  if (signin_util::ShouldShowHistorySyncOptinScreen(*profile) ==
          signin_util::ShouldShowHistorySyncOptinResult::kShow &&
      !signin_util::HasExplicitlyDisabledHistorySync(
          SyncServiceFactory::GetForProfile(profile), identity_manager)) {
    return {.type = ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
            .local_data_count = local_data_count};
  }

  // Regular Batch Upload promo: for users that have any local data type.
  if (local_data_count > 0) {
    return {.type = ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
            .local_data_count = local_data_count};
  }

  // No promo.
  return {.type = std::nullopt, .local_data_count = local_data_count};
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
    case SignInPromoType::kSearchAIMode:
      // Search AI Mode sign-in promo is not related to any
      // synced data type.
      NOTREACHED();
  }
}

bool PromoTypeHasSyncableData(SignInPromoType type) {
  switch (type) {
    case SignInPromoType::kPassword:
    case SignInPromoType::kAddress:
    case SignInPromoType::kBookmark:
    case SignInPromoType::kExtension:
      return true;
    case SignInPromoType::kSearchAIMode:
      // Search AI Mode sign-in promo is not related to any
      // synced data type.
      return false;
  }
  NOTREACHED();
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

int GetSearchAIModePromoShownCount(Profile& profile, const GaiaId& gaia_id) {
  if (!gaia_id.empty()) {
    return SigninPrefs(*profile.GetPrefs())
        .GetSearchAIModeSigninPromoImpressionCount(gaia_id);
  }

  return profile.GetPrefs()->GetInteger(
      prefs::kSearchAIModeSignInPromoShownCountPerProfile);
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
  if (ShouldUseAutofillSignInPromoLimits(type)) {
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
    case SignInPromoType::kSearchAIMode:
      return profile.GetPrefs()->GetInteger(
          prefs::kSearchAIModeSignInPromoDismissCountPerProfile);
  }
}

int GetContextualPromoDismissCountPerAccount(Profile& profile,
                                             SignInPromoType type,
                                             const GaiaId& gaia_id) {
  if (ShouldUseAutofillSignInPromoLimits(type)) {
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
    case SignInPromoType::kSearchAIMode:
      return SigninPrefs(*profile.GetPrefs())
          .GetSearchAIModeSigninPromoDismissCount(gaia_id);
      NOTREACHED();
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
    case SignInPromoType::kSearchAIMode:
      show_count = GetSearchAIModePromoShownCount(profile, account.gaia);
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

  if (base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment) &&
      type != SignInPromoType::kSearchAIMode) {
    return show_count < switches::kContextualSigninPromoShownThreshold.Get() &&
           dismiss_count <
               switches::kContextualSigninPromoDismissedThreshold.Get();
  }

  // Don't show the promo again if:
  // - it has already been shown `kSigninPromoShownThreshold` times for its
  // autofill bubble promo type.
  // - it has already been dismissed `kSigninPromoDismissedThreshold` times,
  // regardless of autofill bubble promo type.
  // - the promo type has a minimum required frequency between impressions
  // which is currently not met.
  return show_count < kSigninPromoShownThreshold &&
         dismiss_count < kSigninPromoDismissedThreshold &&
         IsAllowedByPromoFrequency(profile, type, account.gaia);
}

// Common eligibility checks for signin promos relating to the syncing of an
// underlying syncable data type.
bool CanShowPromoForSyncableDataType(SignInPromoType type, Profile& profile) {
  syncer::SyncPrefs prefs(profile.GetPrefs());
  // Don't show if sync is not allowed to start or is running in local mode.
  if (!SyncServiceFactory::IsSyncAllowed(&profile) ||
      prefs.IsLocalSyncEnabled()) {
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
  return true;
}

// Performs base checks for whether the sign in promos should be shown.
// Needs additional checks depending on the type of the promo.
// `profile` is the profile of the tab the promo would be shown on.
bool ShouldShowSignInPromoCommon(Profile& profile, SignInPromoType type) {
  if (profile.IsOffTheRecord()) {
    return false;
  }

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
      signin_ui_util::GetSingleAccountForPromos(identity_manager);

  // Don't show if sign in can't be offered (ex: signin disallowed).
  if (!CanOfferSignin(original_profile, promo_account.gaia, promo_account.email,
                      /*allow_account_from_other_profile=*/true)
           .IsOk()) {
    return false;
  }

  if (PromoTypeHasSyncableData(type) &&
      !CanShowPromoForSyncableDataType(type, profile)) {
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool ShouldShowExtensionSignInPromo(Profile& profile,
                                    const extensions::Extension& extension) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!extensions::sync_util::ShouldSync(&profile, &extension)) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
    // `ShouldShowSignInPromoCommon()` does not check if extensions are syncing
    // in transport mode. That's why `IsSyncingExtensionsEnabled()` is added so
    // the sign in promo is not shown in that case.
    if (extensions::sync_util::IsSyncingExtensionsEnabled(&profile)) {
      return false;
    }

    if (const signin::IdentityManager* identity_manager =
            IdentityManagerFactory::GetForProfile(&profile);
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      // The promo is not shown to users that have explicitly signed in through
      // the browser (even if extensions are not syncing).
      return false;
    }
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

bool ShouldShowSearchAIModeSignInPromo(Profile& profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  return ShouldShowSignInPromoCommon(profile, SignInPromoType::kSearchAIMode);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool ShouldShowBookmarkSignInPromo(Profile& profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
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
  // Uno Phase 2 Follow up: Always display the promotion.
  return base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp) ||
         !signin_util::IsSigninPending(identity_manager) ||
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
         (base::FeatureList::IsEnabled(
              switches::kEnableSearchAIModeSigninPromo) &&
          access_point == signin_metrics::AccessPoint::kSearchAIModeBubble) ||
         (base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp) &&
          access_point == signin_metrics::AccessPoint::kBookmarkBubble);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

bool IsSignInPromo(signin_metrics::AccessPoint access_point) {
  if (
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      // Remove this condition when `syncer::kUnoPhase2FollowUp` is launched as
      // it is already checked in `IsBubbleSigninPromo()`.
      access_point == signin_metrics::AccessPoint::kBookmarkBubble ||
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
      IsBubbleSigninPromo(access_point)) {
    return true;
  }

  if (access_point == signin_metrics::AccessPoint::kExtensionInstallBubble) {
    return switches::IsExtensionsExplicitBrowserSigninEnabled();
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
    case signin_metrics::AccessPoint::kSearchAIModeBubble:
      return SignInPromoType::kSearchAIMode;
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
      case SignInPromoType::kSearchAIMode:
        pref_name = prefs::kSearchAIModeSignInPromoShownCountPerProfile;
        profile->GetPrefs()->SetTime(
            prefs::kSearchAIModeSignInPromoLastImpressionTimestampPerProfile,
            base::Time::Now());
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
    case SignInPromoType::kSearchAIMode:
      SigninPrefs(*profile->GetPrefs())
          .IncrementSearchAIModeSigninPromoImpressionCount(account.gaia);
      SigninPrefs(*profile->GetPrefs())
          .SetSearchAIModeSigninPromoLastImpressionTime(account.gaia,
                                                        base::Time::Now());
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

bool ShouldUseAutofillSignInPromoLimits(signin::SignInPromoType promo_type) {
  return promo_type != signin::SignInPromoType::kSearchAIMode &&
         !base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment);
}

void RecordAvatarButtonPromoAcceptedAtPromoShownCount(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type,
    const GaiaId& gaia_id,
    PrefService& prefs) {
  constexpr char kAvatarPillPromoAcceptedAtShownCountBaseHistogram[] =
      "Signin.AvatarPillPromo.AcceptedAtShownCount.";

  std::string_view promo_type_suffix;
  // LINT.IfChange(AvatarPillPromoType)
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
    case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      promo_type_suffix = "Signin";
      break;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/histograms.xml:AvatarPillPromoType)

  SigninPrefs signin_prefs(prefs);
  int promo_shown_count =
      GetPromoUsageInfo(prefs, signin_prefs, promo_type, gaia_id).shown_count;
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
  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    BatchUploadService* batch_upload =
        BatchUploadServiceFactory::GetForProfile(&profile);
    if (!batch_upload) {
      std::move(result_callback).Run(ProfileMenuAvatarButtonPromoInfo{});
      return;
    }

    // Note: `GetLocalDataDescriptionsForAvailableTypes()` will return no data
    // if the SyncService is not initialized.
    batch_upload->GetLocalDataDescriptionsForAvailableTypes(
        base::BindOnce(
            &ComputeProfileMenuAvatarButtonPromoInfoWithBatchUploadResult,
            &profile)
            .Then(std::move(result_callback)));
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

AvatarButtonPromoManager::AvatarButtonPromoManager(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service)
    : AvatarButtonPromoManager(
          identity_manager,
          pref_service,
          user_education::features::GetNewBadgeShowCount(),
          user_education::features::GetNewBadgeFeatureUsedCount()) {}

AvatarButtonPromoManager::AvatarButtonPromoManager(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    int max_shown_count,
    int max_used_count)
    : identity_manager_(identity_manager),
      signin_prefs_(std::make_unique<SigninPrefs>(CHECK_DEREF(pref_service))),
      pref_service_(pref_service),
      max_shown_count_(max_shown_count),
      max_used_count_(max_used_count) {
  CHECK(identity_manager_);
  identity_manager_scoped_observation_.Observe(identity_manager_);
}

AvatarButtonPromoManager::~AvatarButtonPromoManager() = default;

// static
void AvatarButtonPromoManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kAvatarButtonPromoProfileDictionary);
}

bool AvatarButtonPromoManager::ShouldShowPromo(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  if (!ArePromotionsEnabled()) {
    return false;
  }

  if (!IsSigninStateAlignedWithPromoType(promo_type)) {
    return false;
  }

  CHECK(pref_service_);
  CHECK(signin_prefs_);
  CHECK(identity_manager_);

  const AccountInfo account =
      signin_ui_util::GetSingleAccountForPromos(identity_manager_);
  auto [promo_shown_count, promo_used_count, promo_last_shown_time,
        last_external_event_time] =
      GetPromoUsageInfo(*pref_service_.get(), *signin_prefs_.get(), promo_type,
                        account.gaia);

  // Only check the `promo_last_shown_time` for eligible `promo_type`.
  if (promo_last_shown_time.has_value() &&
      (base::Time::Now() - promo_last_shown_time.value()) <
          GetMinimumThresholdSinceLastShownTime(promo_type)) {
    return false;
  }

  // Only check the `last_external_event_time` for eligible `promo_type`.
  if (last_external_event_time.has_value() &&
      (base::Time::Now() - last_external_event_time.value() <
       GetMinimumThresholdSinceLastEventTime(promo_type))) {
    return false;
  }

  return promo_shown_count < max_shown_count_ &&
         promo_used_count < max_used_count_;
}

void AvatarButtonPromoManager::RecordPromoShown(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  CHECK(pref_service_);
  CHECK(signin_prefs_);
  CHECK(identity_manager_);
  CHECK(IsSigninStateAlignedWithPromoType(promo_type));

  const AccountInfo account =
      signin_ui_util::GetSingleAccountForPromos(identity_manager_);
  if (promo_type == ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo) {
    CHECK(switches::IsAvatarSyncPromoFeatureEnabled());
    signin_prefs_->IncrementSyncPromoIdentityPillShownCount(account.gaia);
    return;
  }

  base::DictValue& promo_dict = GetPromoDictionary(
      *pref_service_.get(), *signin_prefs_.get(), account.gaia);

  // Only update the last shown time if the `promo_type` supports it.
  if (std::optional<std::string_view> last_shown_time_pref =
          MaybeGetLastShownTimePref(promo_type);
      last_shown_time_pref.has_value()) {
    promo_dict.Set(last_shown_time_pref.value(),
                   base::TimeToValue(base::Time::Now()));
  }

  std::string_view shown_key = GetAvatarButtonPromoShownKey(promo_type);
  int new_conut = promo_dict.FindInt(shown_key).value_or(0) + 1;
  promo_dict.Set(shown_key, new_conut);
}

GaiaId AvatarButtonPromoManager::RecordPromoUsed(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
  CHECK(pref_service_);
  CHECK(signin_prefs_);
  CHECK(identity_manager_);
  CHECK(IsSigninStateAlignedWithPromoType(promo_type));

  const AccountInfo account =
      signin_ui_util::GetSingleAccountForPromos(identity_manager_);
  if (promo_type == ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo) {
    CHECK(switches::IsAvatarSyncPromoFeatureEnabled());
    signin_prefs_->IncrementSyncPromoIdentityPillUsedCount(account.gaia);
    return account.gaia;
  }

  base::DictValue& promo_dict = GetPromoDictionary(
      *pref_service_.get(), *signin_prefs_.get(), account.gaia);
  std::string_view used_key = GetAvatarButtonPromoUsedKey(promo_type);
  int new_conut = promo_dict.FindInt(used_key).value_or(0) + 1;
  promo_dict.Set(used_key, new_conut);
  return account.gaia;
}

bool AvatarButtonPromoManager::ArePromotionsEnabled() const {
  PrefService* local_state = g_browser_process->local_state();
  return local_state && local_state->GetBoolean(prefs::kPromotionsEnabled);
}

bool AvatarButtonPromoManager::IsSigninStateAlignedWithPromoType(
    ProfileMenuAvatarButtonPromoInfo::Type promo_type) const {
  signin_util::SignedInState signed_in_state =
      signin_util::GetSignedInState(identity_manager_);
  switch (promo_type) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      return signed_in_state == signin_util::SignedInState::kSignedIn;
    case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      return signed_in_state == signin_util::SignedInState::kSignedOut ||
             signed_in_state == signin_util::SignedInState::kWebOnlySignedIn;
  }
}

void AvatarButtonPromoManager::OnIdentityManagerShutdown(
    IdentityManager* identity_manager) {
  CHECK_EQ(identity_manager, identity_manager_.get());
  identity_manager_ = nullptr;
  identity_manager_scoped_observation_.Reset();

  // `AvatarButtonPromoManager::OnIdentityManagerShutdown()` is called upon
  // profile destruction, which aligns with the need to clear the prefs. Since
  // currently there is no reliable way to be notified by the pref service
  // shutting down, we rely on this notification as well.
  // The need to clear the prefs here is primarily for unit tests that combines
  // `Browser` + `TestingProfile` (where the `PrefService` is owned by the
  // profile itself).
  pref_service_ = nullptr;
  signin_prefs_.reset();
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
