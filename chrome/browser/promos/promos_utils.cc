// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/promos/promos_utils.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"

namespace promos_utils {
// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Max impression count per user for the iOS password promo on desktop.
constexpr int kiOSPasswordPromoMaxImpressionCount = 2;

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Minimum time threshold between impressions for a given user to see the iOS
// password promo on desktop.
constexpr base::TimeDelta kiOSPasswordPromoCooldownTime = base::Days(60);

// Max impression count per user, per promo for the iOS desktop promos on
// desktop.
constexpr int kiOSDesktopPromoMaxImpressionCount = 3;

// Total impression count per user in their lifetime, for all iOS desktop
// promos.
constexpr int kiOSDesktopPromoTotalImpressionCount = 10;

// Total amount of opt-outs across any Desktop to iOS promo to block impressions
// of other instances of Desktop to iOS promos, per user.
constexpr int kiOSDesktopPromoTotalOptOuts = 2;

// Minimum time threshold between impressions for a given user to see the iOS
// desktop promo on desktop.
constexpr base::TimeDelta kiOSDesktopPromoCooldownTime = base::Days(90);

// IOSDesktopPromoHistogramType returns the promo histogram type for the given
// promo type. New promos should add themselves to this check.
std::string IOSDesktopPromoHistogramType(IOSPromoType promo_type) {
  switch (promo_type) {
    case IOSPromoType::kPassword:
      return "PasswordPromo";
    case IOSPromoType::kAddress:
      return "AddressPromo";
    case IOSPromoType::kPayment:
      return "PaymentPromo";
  }
}

// VerifyIOSDesktopPromoTotalImpressions ensures that each individual user sees
// no more than a maximum of these promos total in their lifetime. New promos
// should add themselves to this check.
bool VerifyIOSDesktopPromoTotalImpressions(Profile* profile) {
  int total_desktop_promo_impressions =
      profile->GetPrefs()->GetInteger(
          promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter) +
      profile->GetPrefs()->GetInteger(
          promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter) +
      profile->GetPrefs()->GetInteger(
          promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter);

  return total_desktop_promo_impressions <=
         kiOSDesktopPromoTotalImpressionCount;
}

// VerifyIOSDesktopPromoTotalOptOuts verifies that a user hasn't opted-out of
// seeing more than the allowed amount of opt-outs for all iOS Desktop promos.
// New promos should add themselves to this check.
bool VerifyIOSDesktopPromoTotalOptOuts(Profile* profile) {
  std::vector<bool> promo_opt_outs = {
      profile->GetPrefs()->GetBoolean(
          promos_prefs::kDesktopToiOSPasswordPromoOptOut),
      profile->GetPrefs()->GetBoolean(
          promos_prefs::kDesktopToiOSAddressPromoOptOut),
      profile->GetPrefs()->GetBoolean(
          promos_prefs::kDesktopToiOSPaymentPromoOptOut)};

  int total_desktop_promo_opt_outs_counter =
      std::count(promo_opt_outs.begin(), promo_opt_outs.end(), true);

  return total_desktop_promo_opt_outs_counter < kiOSDesktopPromoTotalOptOuts;
}

// VerifyMostRecentPromoTimestamp ensures that each individual user sees a
// iOS to Desktop promo a maximum of once per cooldown period. New promos should
// add themselves to this check.
bool VerifyMostRecentPromoTimestamp(Profile* profile) {
  std::vector<base::Time> promos_timestamps = {
      profile->GetPrefs()->GetTime(
          promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp),
      profile->GetPrefs()->GetTime(
          promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp),
      profile->GetPrefs()->GetTime(
          promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp)};

  auto most_recent_promo_timestamp =
      std::max_element(promos_timestamps.begin(), promos_timestamps.end());

  return *most_recent_promo_timestamp + kiOSDesktopPromoCooldownTime <
         base::Time::Now();
}

// Verify that the user is syncing preferences (for impressions and opt-out
// tracking), and that they are syncing the specific datatype needed for a given
// promo type.
bool VerifySyncingDatatypes(const syncer::SyncService& sync_service,
                            IOSPromoType promo_type) {
  if (!sync_service.GetActiveDataTypes().Has(syncer::PREFERENCES)) {
    return false;
  }

  switch (promo_type) {
    case IOSPromoType::kPassword:
      return sync_service.GetActiveDataTypes().Has(syncer::PASSWORDS);
    case IOSPromoType::kAddress:
      return sync_service.GetActiveDataTypes().Has(syncer::CONTACT_INFO);
    case IOSPromoType::kPayment:
      return sync_service.GetActiveDataTypes().Has(
          syncer::AUTOFILL_WALLET_DATA);
  }
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after
// the generic promo launch.
// RecordIOSPasswordPromoShownHistogram records which impression (count) was
// shown to the user.
void RecordIOSPasswordPromoShownHistogram(int impression_count) {
  if (impression_count == 1) {
    base::UmaHistogramEnumeration(
        "IOS.DesktopPasswordPromo.Shown",
        promos_utils::DesktopIOSPasswordPromoImpression::kFirstImpression);
  } else if (impression_count == 2) {
    base::UmaHistogramEnumeration(
        "IOS.DesktopPasswordPromo.Shown",
        promos_utils::DesktopIOSPasswordPromoImpression::kSecondImpression);
  } else {
    NOTREACHED();
  }
}

// RecordIOSDesktopPromoShownHistogram records which impression (count) was
// shown to the user depending on the given promo type.
void RecordIOSDesktopPromoShownHistogram(IOSPromoType promo_type,
                                         int impression_count) {
  std::string promo_histogram_type = IOSDesktopPromoHistogramType(promo_type);
  DesktopIOSPromoImpression promo_impression;
  switch (impression_count) {
    case 1:
      promo_impression = DesktopIOSPromoImpression::kFirstImpression;
      break;
    case 2:
      promo_impression = DesktopIOSPromoImpression::kSecondImpression;
      break;
    case 3:
      promo_impression = DesktopIOSPromoImpression::kThirdImpression;
      break;
    default:
      NOTREACHED();
  }
  base::UmaHistogramEnumeration(
      "IOS.Desktop." + promo_histogram_type + ".Shown", promo_impression);
}

// IOSPromoPrefsConfig is a complex struct that needs definition of a
// constructor, an explicit out-of-line copy constructor and a destructor. New
// promos should add themselves to this function.
IOSPromoPrefsConfig::IOSPromoPrefsConfig() = default;
IOSPromoPrefsConfig::IOSPromoPrefsConfig(const IOSPromoPrefsConfig&) = default;
IOSPromoPrefsConfig::~IOSPromoPrefsConfig() = default;

IOSPromoPrefsConfig::IOSPromoPrefsConfig(IOSPromoType promo_type) {
  switch (promo_type) {
    case IOSPromoType::kPassword:
      // This feature isn't defined without the following buildflags.
#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
      promo_feature = &feature_engagement::kIPHiOSPasswordPromoDesktopFeature;
#endif
      promo_impressions_counter_pref_name =
          promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter;
      promo_opt_out_pref_name = promos_prefs::kDesktopToiOSPasswordPromoOptOut;
      promo_last_impression_timestamp_pref_name =
          promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp;
      break;
    case IOSPromoType::kAddress:
      // This feature isn't defined without the following buildflags.
#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
      promo_feature = &feature_engagement::kIPHiOSAddressPromoDesktopFeature;
#endif
      promo_impressions_counter_pref_name =
          promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter;
      promo_opt_out_pref_name = promos_prefs::kDesktopToiOSAddressPromoOptOut;
      promo_last_impression_timestamp_pref_name =
          promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp;
      break;
    case IOSPromoType::kPayment:
      // This feature isn't defined without the following buildflags.
#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
      promo_feature = &feature_engagement::kIPHiOSPaymentPromoDesktopFeature;
#endif
      promo_impressions_counter_pref_name =
          promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter;
      promo_opt_out_pref_name = promos_prefs::kDesktopToiOSPaymentPromoOptOut;
      promo_last_impression_timestamp_pref_name =
          promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp;
      break;
  }
}

// Registers profile prefs. New promos should add themselves to this function.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(
      promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp,
      base::Time(), user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      promos_prefs::kDesktopToiOSPasswordPromoOptOut, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterTimePref(
      promos_prefs::kDesktopToiOSAddressPromoLastImpressionTimestamp,
      base::Time(), user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      promos_prefs::kDesktopToiOSAddressPromoImpressionsCounter, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      promos_prefs::kDesktopToiOSAddressPromoOptOut, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterTimePref(
      promos_prefs::kDesktopToiOSPaymentPromoLastImpressionTimestamp,
      base::Time(), user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      promos_prefs::kDesktopToiOSPaymentPromoImpressionsCounter, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      promos_prefs::kDesktopToiOSPaymentPromoOptOut, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

const base::Feature& GetIOSDesktopPromoFeatureEngagement(
    IOSPromoType promo_type) {
  IOSPromoPrefsConfig promo_prefs(promo_type);
  return *promo_prefs.promo_feature;
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after
// the generic promo launch.
void RecordIOSPasswordPromoUserInteractionHistogram(
    int impression_count,
    DesktopIOSPasswordPromoAction action) {
  if (impression_count == 1) {
    base::UmaHistogramEnumeration(
        "IOS.DesktopPasswordPromo.FirstImpression.Action", action);
  } else if (impression_count == 2) {
    base::UmaHistogramEnumeration(
        "IOS.DesktopPasswordPromo.SecondImpression.Action", action);
  } else {
    NOTREACHED();
  }
}

// RecordIOSDesktopPromoUserInteractionHistogram records which impression
// (count) depending on the promo type.
void RecordIOSDesktopPromoUserInteractionHistogram(
    IOSPromoType promo_type,
    int impression_count,
    DesktopIOSPromoAction action) {
  std::string promo_histogram_type = IOSDesktopPromoHistogramType(promo_type);
  if (impression_count == 1) {
    base::UmaHistogramEnumeration(
        "IOS.Desktop." + promo_histogram_type + ".FirstImpression.Action",
        action);
  } else if (impression_count == 2) {
    base::UmaHistogramEnumeration(
        "IOS.Desktop." + promo_histogram_type + ".SecondImpression.Action",
        action);
  } else if (impression_count == 3) {
    base::UmaHistogramEnumeration(
        "IOS.Desktop." + promo_histogram_type + ".ThirdImpression.Action",
        action);
  } else {
    NOTREACHED();
  }
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after
// the generic promo launch.
bool ShouldShowIOSPasswordPromo(Profile* profile) {
  // Show the promo if the user hasn't opted out, is not in the cooldown
  // period and is within the impression limit for this promo.
  if (profile->GetPrefs()->GetInteger(
          promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter) <
          kiOSPasswordPromoMaxImpressionCount &&
      profile->GetPrefs()->GetTime(
          promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp) +
              kiOSPasswordPromoCooldownTime <
          base::Time::Now() &&
      !profile->GetPrefs()->GetBoolean(
          promos_prefs::kDesktopToiOSPasswordPromoOptOut)) {
    return true;
  }

  return false;
}

bool ShouldShowIOSDesktopPromo(Profile* profile,
                               const syncer::SyncService* sync_service,
                               IOSPromoType promo_type) {
  // Don't show the promo if the local state exists and `kPromotionsEnabled` is
  // false (likely overridden by policy).
#if !BUILDFLAG(IS_ANDROID)
  PrefService* local_state = g_browser_process->local_state();
  if (local_state && !local_state->GetBoolean(prefs::kPromotionsEnabled)) {
    return false;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  IOSPromoPrefsConfig promo_prefs(promo_type);

  // Show the promo if the user hasn't opted out, is not in the cooldown
  // period and is within the impression limit for this promo.
  return sync_service && VerifySyncingDatatypes(*sync_service, promo_type) &&
         profile->GetPrefs()->GetInteger(
             promo_prefs.promo_impressions_counter_pref_name) <
             kiOSDesktopPromoMaxImpressionCount &&
         VerifyMostRecentPromoTimestamp(profile) &&
         VerifyIOSDesktopPromoTotalImpressions(profile) &&
         VerifyIOSDesktopPromoTotalOptOuts(profile) &&
         !profile->GetPrefs()->GetBoolean(promo_prefs.promo_opt_out_pref_name);
}

bool UserNotClassifiedAsMobileDeviceSwitcher(
    const segmentation_platform::ClassificationResult& result) {
  return result.status == segmentation_platform::PredictionStatus::kSucceeded &&
         !base::Contains(
             result.ordered_labels,
             segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel) &&
         !base::Contains(result.ordered_labels,
                         segmentation_platform::DeviceSwitcherModel::
                             kIosPhoneChromeLabel) &&
         !base::Contains(
             result.ordered_labels,
             segmentation_platform::DeviceSwitcherModel::kAndroidTabletLabel) &&
         !base::Contains(
             result.ordered_labels,
             segmentation_platform::DeviceSwitcherModel::kIosTabletLabel);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after
// the generic promo launch.
void iOSPasswordPromoShown(Profile* profile) {
  int new_impression_count =
      profile->GetPrefs()->GetInteger(
          promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter) +
      1;

  profile->GetPrefs()->SetInteger(
      promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter,
      new_impression_count);
  profile->GetPrefs()->SetTime(
      promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp,
      base::Time::Now());

  RecordIOSPasswordPromoShownHistogram(new_impression_count);
}

void IOSDesktopPromoShown(Profile* profile, IOSPromoType promo_type) {
  IOSPromoPrefsConfig promo_prefs(promo_type);
  int new_impression_count =
      profile->GetPrefs()->GetInteger(
          promo_prefs.promo_impressions_counter_pref_name) +
      1;

  profile->GetPrefs()->SetInteger(
      promo_prefs.promo_impressions_counter_pref_name, new_impression_count);
  profile->GetPrefs()->SetTime(
      promo_prefs.promo_last_impression_timestamp_pref_name, base::Time::Now());

  RecordIOSDesktopPromoShownHistogram(promo_type, new_impression_count);
}

}  // namespace promos_utils
