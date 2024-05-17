// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/promos/promos_utils.h"

#include <string>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_types.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/public/features.h"

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Max impression count per user for the iOS password promo on desktop.
constexpr int kiOSPasswordPromoMaxImpressionCount = 2;

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Minimum time threshold between impressions for a given user to see the iOS
// password promo on desktop.
constexpr base::TimeDelta kiOSPasswordPromoCooldownTime = base::Days(60);

// Max impression count per user for the iOS desktop password promo on desktop.
constexpr int kiOSDesktopPasswordPromoMaxImpressionCount = 2;

// Minimum time threshold between impressions for a given user to see the iOS
// desktop password promo on desktop.
constexpr base::TimeDelta kiOSDesktopPasswordPromoCooldownTime = base::Days(60);

IOSPromoPrefsConfig SetUpIOSPromoConfig(IOSPromoType promo_type) {
  IOSPromoPrefsConfig ios_promo_prefs_config;
  switch (promo_type) {
    case IOSPromoType::kPassword:
      // This feature isn't defined with those buildflags.
#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
      ios_promo_prefs_config.promo_feature_ =
          &feature_engagement::kIPHiOSPasswordPromoDesktopFeature;
#endif
      ios_promo_prefs_config.promo_impressions_counter_pref_name =
          promos_prefs::kiOSPasswordPromoImpressionsCounter;
      ios_promo_prefs_config.promo_opt_out_pref_name =
          promos_prefs::kiOSPasswordPromoOptOut;
      break;
    // TODO(crbug.com/331408937): Add IOS Address Promo for Desktop.
    default:
      NOTREACHED_NORETURN();
  }
  return ios_promo_prefs_config;
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
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
    NOTREACHED_NORETURN();
  }
}

// RecordIOSDesktopPasswordPromoShownHistogram records which impression (count)
// was shown to the user.
void RecordIOSDesktopPasswordPromoShownHistogram(int impression_count) {
  if (impression_count == 1) {
    base::UmaHistogramEnumeration(
        "IOS.Desktop.PasswordPromo.Shown",
        promos_utils::DesktopIOSPromoImpression::kFirstImpression);
  } else if (impression_count == 2) {
    base::UmaHistogramEnumeration(
        "IOS.Desktop.PasswordPromo.Shown",
        promos_utils::DesktopIOSPromoImpression::kSecondImpression);
  } else {
    NOTREACHED_NORETURN();
  }
}

// RecordIOSPromoShownHistogram records which impression (count) was
// shown to the user depending on the given promo type.
void RecordIOSPromoShownHistogram(IOSPromoType promo_type,
                                  int impression_count) {
  switch (promo_type) {
    case IOSPromoType::kPassword:
      RecordIOSDesktopPasswordPromoShownHistogram(impression_count);
      break;
    // TODO(crbug.com/331408937): Add IOS Address Promo for Desktop.
    default:
      NOTREACHED_NORETURN();
  }
}

namespace promos_utils {
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(
      promos_prefs::kiOSPasswordPromoLastImpressionTimestamp, base::Time(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      promos_prefs::kiOSPasswordPromoImpressionsCounter, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      promos_prefs::kiOSPasswordPromoOptOut, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
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
    NOTREACHED_NORETURN();
  }
}

void RecordIOSDesktopPasswordPromoUserInteractionHistogram(
    int impression_count,
    DesktopIOSPromoAction action) {
  if (impression_count == 1) {
    base::UmaHistogramEnumeration(
        "IOS.Desktop.PasswordPromo.FirstImpression.Action", action);
  } else if (impression_count == 2) {
    base::UmaHistogramEnumeration(
        "IOS.Desktop.PasswordPromo.SecondImpression.Action", action);
  } else {
    NOTREACHED_NORETURN();
  }
}

// RecordIOSPromoUserInteraction records which impression (count) depending on
// the promo type.
void RecordIOSPromoUserInteractionHistogram(IOSPromoType promo_type,
                                            int impression_count,
                                            DesktopIOSPromoAction action) {
  switch (promo_type) {
    case IOSPromoType::kPassword:
      RecordIOSDesktopPasswordPromoUserInteractionHistogram(impression_count,
                                                            action);
      break;
    // TODO(crbug.com/331408937): Add IOS Address Promo for Desktop.
    default:
      NOTREACHED_NORETURN();
  }
}

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
bool ShouldShowIOSPasswordPromo(Profile* profile) {
  // Show the promo if the user hasn't opted out, is not in the cooldown period
  // and is within the impression limit for this promo.
  if (profile->GetPrefs()->GetInteger(
          promos_prefs::kiOSPasswordPromoImpressionsCounter) <
          kiOSPasswordPromoMaxImpressionCount &&
      profile->GetPrefs()->GetTime(
          promos_prefs::kiOSPasswordPromoLastImpressionTimestamp) +
              kiOSPasswordPromoCooldownTime <
          base::Time::Now() &&
      !profile->GetPrefs()->GetBoolean(promos_prefs::kiOSPasswordPromoOptOut)) {
    return true;
  }

  return false;
}

bool ShouldShowIOSDesktopPasswordPromo(Profile* profile) {
  // Show the promo if the user hasn't opted out, is not in the cooldown period
  // and is within the impression limit for this promo.
  return profile->GetPrefs()->GetInteger(
             promos_prefs::kiOSPasswordPromoImpressionsCounter) <
             kiOSDesktopPasswordPromoMaxImpressionCount &&
         profile->GetPrefs()->GetTime(
             promos_prefs::kiOSPasswordPromoLastImpressionTimestamp) +
                 kiOSDesktopPasswordPromoCooldownTime <
             base::Time::Now() &&
         !profile->GetPrefs()->GetBoolean(
             promos_prefs::kiOSPasswordPromoOptOut);
}

bool ShouldShowIOSDesktopPromo(Profile* profile, IOSPromoType promo_type) {
  bool show_promo = false;
  switch (promo_type) {
    case IOSPromoType::kPassword:
      show_promo = ShouldShowIOSDesktopPasswordPromo(profile);
      break;
    // TODO(crbug.com/331408937): Add IOS Address Promo for Desktop.
    default:
      NOTREACHED_NORETURN();
  }
  return show_promo;
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

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
void iOSPasswordPromoShown(Profile* profile) {
  int new_impression_count =
      profile->GetPrefs()->GetInteger(
          promos_prefs::kiOSPasswordPromoImpressionsCounter) +
      1;

  profile->GetPrefs()->SetInteger(
      promos_prefs::kiOSPasswordPromoImpressionsCounter, new_impression_count);
  profile->GetPrefs()->SetTime(
      promos_prefs::kiOSPasswordPromoLastImpressionTimestamp,
      base::Time::Now());

  RecordIOSPasswordPromoShownHistogram(new_impression_count);
}

void IOSDesktopPasswordPromoShown(Profile* profile) {
  int new_impression_count =
      profile->GetPrefs()->GetInteger(
          promos_prefs::kiOSPasswordPromoImpressionsCounter) +
      1;

  profile->GetPrefs()->SetInteger(
      promos_prefs::kiOSPasswordPromoImpressionsCounter, new_impression_count);
  profile->GetPrefs()->SetTime(
      promos_prefs::kiOSPasswordPromoLastImpressionTimestamp,
      base::Time::Now());

  RecordIOSDesktopPasswordPromoShownHistogram(new_impression_count);
}

void IOSDesktopPromoShown(Profile* profile, IOSPromoType promo_type) {
  switch (promo_type) {
    case IOSPromoType::kPassword:
      IOSDesktopPasswordPromoShown(profile);
      break;
    // TODO(crbug.com/331408937): Add IOS Address Promo for Desktop.
    default:
      NOTREACHED_NORETURN();
  }
}
}  // namespace promos_utils
