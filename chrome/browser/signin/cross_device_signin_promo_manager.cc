// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cross_device_signin_promo_manager.h"

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace {

// Sub-dictionary serialization keys to be used per data type, defined in
// `GetEntryPointPrefKey()` below.
constexpr char kShownCountKey[] = "shown_count";
constexpr char kLastDismissedTimeKey[] = "last_dismissed_time";
constexpr char kShownAfterDismissalKey[] = "shown_after_dismissal";

// Dictionary keys for data type specific promo data.
constexpr char kHistoryDictionaryKey[] = "history";

// Limits for the dismissible promo.
constexpr int kMaxShownCount = 5;
constexpr base::TimeDelta kDismissiblePromoCooldownPeriod = base::Days(7);

struct CrossDeviceSigninPromoData {
  int shown_count = 0;
  base::Time last_dismissed_time;
  bool shown_after_dismissal = false;
};

std::string_view GetEntryPointHistogramSuffix(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  // LINT.IfChange(CrossDeviceSigninPromoEntryPointVariant)
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return "HistoryPage";
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
      return "ProfileMenu";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/histograms.xml:CrossDeviceSigninPromoEntryPointVariant)
}

void RecordShouldShowResult(CrossDeviceSigninPromoEntryPoint entry_point,
                            CrossDeviceSigninPromoShouldShowResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Signin.CrossDeviceSigninPromo.ShouldShowResult.",
                    GetEntryPointHistogramSuffix(entry_point)}),
      result);
}

// Key used for the data type specific sub-dictionary that holds the promo data.
std::string_view GetEntryPointPrefKey(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return kHistoryDictionaryKey;
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
      NOTREACHED() << "Entry point does not have any promo data";
  }
}

// Gets the main promo pref dictionary for the primary account.
base::DictValue& GetCrossDevicePromoPrefsForPrimaryAccount(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);
  GaiaId gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  CHECK(!gaia_id.empty());

  SigninPrefs signin_prefs(*profile->GetPrefs());
  return signin_prefs.GetOrCreateCrossDevicePromoPrefs(gaia_id);
}

// Returns empty/defaulted data if the sub-dictionary is not set yet.
CrossDeviceSigninPromoData ReadDismissiblePromoData(
    Profile* profile,
    CrossDeviceSigninPromoEntryPoint entry_point) {
  CrossDeviceSigninPromoData data;
  base::DictValue& promo_prefs =
      GetCrossDevicePromoPrefsForPrimaryAccount(profile);
  const base::DictValue* entry_dict =
      promo_prefs.FindDict(GetEntryPointPrefKey(entry_point));
  if (!entry_dict) {
    return data;
  }
  data.shown_count = entry_dict->FindInt(kShownCountKey).value_or(0);
  data.shown_after_dismissal =
      entry_dict->FindBool(kShownAfterDismissalKey).value_or(false);
  const base::Value* time_val = entry_dict->Find(kLastDismissedTimeKey);
  if (time_val) {
    data.last_dismissed_time =
        base::ValueToTime(time_val).value_or(base::Time());
  }
  return data;
}

void WriteDismissiblePromoData(Profile* profile,
                               CrossDeviceSigninPromoEntryPoint entry_point,
                               const CrossDeviceSigninPromoData& data) {
  base::DictValue& promo_prefs =
      GetCrossDevicePromoPrefsForPrimaryAccount(profile);
  base::DictValue entry_dict;
  entry_dict.Set(kShownCountKey, data.shown_count);
  entry_dict.Set(kShownAfterDismissalKey, data.shown_after_dismissal);
  if (!data.last_dismissed_time.is_null()) {
    entry_dict.Set(kLastDismissedTimeKey,
                   base::TimeToValue(data.last_dismissed_time));
  }
  promo_prefs.Set(GetEntryPointPrefKey(entry_point), std::move(entry_dict));
}

// Returns std::nullopt for non-dismissible promo entry points.
std::optional<CrossDeviceSigninPromoData> GetPromoDataForDismissibleEntryPoint(
    Profile* profile,
    CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return ReadDismissiblePromoData(profile, entry_point);
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
      return std::nullopt;
  }
}

// Limits for the dismissible promo:
// - Promo can be shown at most 5 times.
// - Promo can be shown at most once after dismissal.
// - Cooldown period of 7 days after dismissal.
bool IsDismissiblePromoLimitReached(
    const CrossDeviceSigninPromoData& data,
    CrossDeviceSigninPromoEntryPoint entry_point) {
  if (data.shown_count >= kMaxShownCount) {
    RecordShouldShowResult(
        entry_point,
        CrossDeviceSigninPromoShouldShowResult::kShownLimitReached);
    return true;
  }
  if (data.shown_after_dismissal) {
    RecordShouldShowResult(entry_point,
                           CrossDeviceSigninPromoShouldShowResult::
                               kAlreadyShownAfterDismissalLimitReached);
    return true;
  }
  if (!data.last_dismissed_time.is_null()) {
    if (base::Time::Now() <
        data.last_dismissed_time + kDismissiblePromoCooldownPeriod) {
      RecordShouldShowResult(
          entry_point, CrossDeviceSigninPromoShouldShowResult::kCooldownActive);
      return true;
    }
  }
  return false;
}

bool IsUserSignedInWithNoError(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin_util::SignedInState state =
      signin_util::GetSignedInState(identity_manager);
  return state == signin_util::SignedInState::kSignedIn ||
         state == signin_util::SignedInState::kSyncing;
}

bool HasOtherSignedInDevices(Profile* profile) {
  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  CHECK(device_info_sync_service);
  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service->GetDeviceInfoTracker();
  CHECK(device_info_tracker);

  for (const syncer::DeviceInfo* device_info :
       device_info_tracker->GetAllDeviceInfo()) {
    if (!device_info_tracker->IsRecentLocalCacheGuid(device_info->guid())) {
      return true;
    }
  }
  return false;
}

bool IsHistorySyncEnabled(Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return false;
  }
  return sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory);
}

}  // namespace

bool ShouldShowCrossDeviceSigninPromo(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  if (!base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop)) {
    return false;
  }

  // 1. General eligibility: Signed in with no errors, and has NO other devices.
  if (!IsUserSignedInWithNoError(profile)) {
    RecordShouldShowResult(
        entry_point, CrossDeviceSigninPromoShouldShowResult::kNotSignedIn);
    return false;
  }
  if (HasOtherSignedInDevices(profile)) {
    RecordShouldShowResult(
        entry_point, CrossDeviceSigninPromoShouldShowResult::kHasOtherDevices);
    return false;
  }

  // 2. Data-type eligibility check.
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      if (!IsHistorySyncEnabled(profile)) {
        RecordShouldShowResult(
            entry_point,
            CrossDeviceSigninPromoShouldShowResult::kDataTypeNotEnabled);
        return false;
      }
      break;
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
      // Permanent entry point, no data-type constraints.
      break;
  }

  // 3. Dismissible limit checking - only for dismissible entry points.
  std::optional<CrossDeviceSigninPromoData> data =
      GetPromoDataForDismissibleEntryPoint(profile, entry_point);
  if (data.has_value() && IsDismissiblePromoLimitReached(*data, entry_point)) {
    return false;
  }

  RecordShouldShowResult(entry_point,
                         CrossDeviceSigninPromoShouldShowResult::kCanShow);
  return true;
}

void OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint entry_point,
                                   Profile* profile) {
  std::optional<CrossDeviceSigninPromoData> data =
      GetPromoDataForDismissibleEntryPoint(profile, entry_point);
  if (!data.has_value()) {
    NOTREACHED() << "Shown tracking should not be called for "
                    "non-dismissible entry point";
  }

  data->shown_count++;
  if (!data->last_dismissed_time.is_null()) {
    data->shown_after_dismissal = true;
  }
  WriteDismissiblePromoData(profile, entry_point, *data);

  base::UmaHistogramExactLinear(
      base::StrCat({"Signin.CrossDeviceSigninPromo.ShownCount.",
                    GetEntryPointHistogramSuffix(entry_point)}),
      data->shown_count, kMaxShownCount + 1);
}

void OnCrossDeviceSigninPromoDismissed(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  std::optional<CrossDeviceSigninPromoData> data =
      GetPromoDataForDismissibleEntryPoint(profile, entry_point);
  if (!data.has_value()) {
    NOTREACHED() << "Dismissal tracking should not be called for "
                    "non-dismissible entry point";
  }

  data->last_dismissed_time = base::Time::Now();
  WriteDismissiblePromoData(profile, entry_point, *data);

  base::UmaHistogramExactLinear(
      base::StrCat({"Signin.CrossDeviceSigninPromo.DismissedAtShownCount.",
                    GetEntryPointHistogramSuffix(entry_point)}),
      data->shown_count, kMaxShownCount + 1);
}

void OpenSigninToPhoneQrCodeBubble(BrowserWindowInterface* browser_window,
                                   CrossDeviceSigninPromoEntryPoint entry_point,
                                   base::OnceClosure closing_callback) {
  CHECK(base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop));
  base::UmaHistogramEnumeration(
      "Signin.CrossDeviceSigninPromo.OpenedQrCodeBubble", entry_point);
  // TODO(crbug.com/527889253): Implement the actual logic.
  std::move(closing_callback).Run();
}
