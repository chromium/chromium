// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROMOS_PROMOS_UTILS_H_
#define CHROME_BROWSER_PROMOS_PROMOS_UTILS_H_

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/segmentation_platform/public/result.h"

class Profile;

namespace promos_utils {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// Enum for different action values possible used in the
// IOS.DesktopPasswordPromo.{Impression}.Action histogram. This must
// match enums.xml.
enum class DesktopIOSPasswordPromoAction {
  kDismissed = 0,
  kExplicitlyClosed = 1,
  kGetStartedClicked = 2,
  kMaxValue = kGetStartedClicked
};

// Enum for different impression values possible used in the
// IOS.DesktopPasswordPromo.Shown histogram. This must match enums.xml.
enum class DesktopIOSPasswordPromoImpression {
  kFirstImpression = 0,
  kSecondImpression = 1,
  kMaxValue = kSecondImpression
};

// Amount of days of data to look back on for the segmentation platform model's
// input data.
constexpr int kiOSPasswordPromoLookbackWindow = 60;

// RegisterProfilePrefs is a helper to register the synced profile prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// RecordIOSPasswordPromoUserInteractionHistogram records the action taken by
// the user on the iOS password promo depending on which impression being shown.
void RecordIOSPasswordPromoUserInteractionHistogram(
    int impression_count,
    DesktopIOSPasswordPromoAction action);

// IsActivationCriteriaOverriddenIOSPasswordPromo returns true if the activation
// method of the flag is set to overridden (always show).
bool IsActivationCriteriaOverriddenIOSPasswordPromo();

// ShouldShowIOSPasswordPromo checks if the user should be shown the iOS
// password promo (all criteria are met), and returns true if so.
bool ShouldShowIOSPasswordPromo(Profile* profile);

// Processes the results of the user classification to make sure there were no
// errors and the user is not classified as a switcher from a mobile device by
// the segmentation platform (i.e. return true if the promo should be shown).
bool UserNotClassifiedAsMobileDeviceSwitcher(
    const segmentation_platform::ClassificationResult& result);

// iOSPasswordPromoShown sets the updated last impression timestamp,
// increments the impression counter for the iOS password promo and records the
// necessary histogram.
void iOSPasswordPromoShown(Profile* profile);

// IsDirectVariantIOSPasswordPromo returns true if the user is in one of the
// "direct" variant groups (QR code promo).
bool IsDirectVariantIOSPasswordPromo();

// IsIndirectVariantIOSPasswordPromo returns true if the user is in one of the
// "indirect" variant groups (get started button).
bool IsIndirectVariantIOSPasswordPromo();
}  // namespace promos_utils

#endif  // CHROME_BROWSER_PROMOS_PROMOS_UTILS_H_
