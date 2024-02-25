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
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/public/features.h"

// Max impression count per user for the iOS password promo on desktop.
constexpr int kiOSPasswordPromoMaxImpressionCount = 2;

// Minimum time threshold between impressions for a given user to see the iOS
// password promo on desktop.
constexpr base::TimeDelta kiOSPasswordPromoCooldownTime = base::Days(60);

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
}  // namespace promos_utils
