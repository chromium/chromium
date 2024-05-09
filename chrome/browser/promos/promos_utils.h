// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROMOS_PROMOS_UTILS_H_
#define CHROME_BROWSER_PROMOS_PROMOS_UTILS_H_

#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/segmentation_platform/public/result.h"

class Profile;

enum class IOSPromoType;

// IOSPromoPrefsConfig is the structure to configure the promo prefs,
// including the feature, the impressions counter and opt out.
struct IOSPromoPrefsConfig {
  raw_ptr<const base::Feature> promo_feature_;
  std::string promo_impressions_counter_pref_name;
  std::string promo_opt_out_pref_name;
};

// SetUpIOSPromoConfig creates and returns the correct configuration
// for the given iOS promo type.
IOSPromoPrefsConfig SetUpIOSPromoConfig(IOSPromoType promo_type);

namespace promos_utils {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// Enum for different action values possible used in the
// IOS.DesktopPromo.{PromoType}.{Impression}.Action histogram.
// LINT.IfChange
enum class DesktopIOSPromoAction {
  kDismissed = 0,
  kNoThanksClicked = 1,
  kMaxValue = kNoThanksClicked
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml)

// Enum for different impression values possible used in the
// IOS.DesktopPromo.{PromoType}.Shown histogram. This must match enums.xml.
enum class DesktopIOSPromoImpression {
  kFirstImpression = 0,
  kSecondImpression = 1,
  kMaxValue = kSecondImpression
};

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// Enum for different action values possible used in the
// IOS.DesktopPasswordPromo.{Impression}.Action histogram.
// LINT.IfChange
enum class DesktopIOSPasswordPromoAction {
  kDismissed = 0,
  kExplicitlyClosed = 1,
  kGetStartedClicked = 2,
  kMaxValue = kGetStartedClicked
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml)

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
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

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// RecordIOSPasswordPromoUserInteractionHistogram records the action taken
// by the user on the iOS password promo depending on which impression being
// shown.
void RecordIOSPasswordPromoUserInteractionHistogram(
    int impression_count,
    DesktopIOSPasswordPromoAction action);

// RecordIOSPromoUserInteractionHistogram records the action taken
// by the user on the iOS promo depending on the promo type and which
// impression being shown.
void RecordIOSPromoUserInteractionHistogram(IOSPromoType promo_type,
                                            int impression_count,
                                            DesktopIOSPromoAction action);

// RecordIOSDesktopPasswordPromoUserInteractionHistogram records the action
// taken by the user on the iOS desktop password promo depending on which
// impression being shown.
void RecordIOSDesktopPasswordPromoUserInteractionHistogram(
    int impression_count,
    DesktopIOSPromoAction action);

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// ShouldShowIOSPasswordPromo checks if the user should be shown the iOS
// password promo (all criteria are met), and returns true if so.
bool ShouldShowIOSPasswordPromo(Profile* profile);

// ShouldShowIOSDesktopPromo checks if the user should be shown the iOS desktop
// promo (all criteria are met) depending on the given promo type , and returns
// true if so.
bool ShouldShowIOSDesktopPromo(Profile* profile, IOSPromoType promo_type);

// ShouldShowIOSDesktopPasswordPromo checks if the user should be shown the iOS
// desktop password promo (all criteria are met), and returns true if so.
bool ShouldShowIOSDesktopPasswordPromo(Profile* profile);

// Processes the results of the user classification to make sure there were
// no errors and the user is not classified as a switcher from a mobile
// device by the segmentation platform (i.e. return true if the promo should
// be shown).
bool UserNotClassifiedAsMobileDeviceSwitcher(
    const segmentation_platform::ClassificationResult& result);

// TODO(crbug.com/339262105): Clean up the old password promo methods after the
// generic promo launch.
// iOSPasswordPromoShown sets the updated last impression timestamp,
// increments the impression counter for the iOS password promo and records
// the necessary histogram.
void iOSPasswordPromoShown(Profile* profile);

// IOSDesktopPromoShown sets the updated last impression timestamp,
// increments the impression counter for the given iOS promo type and records
// the necessary histogram.
void IOSDesktopPromoShown(Profile* profile, IOSPromoType promo_type);

// IOSDesktopPasswordPromoShown sets the updated last impression timestamp,
// increments the impression counter for the iOS desktop password promo and
// records the necessary histogram.
void IOSDesktopPasswordPromoShown(Profile* profile);

}  // namespace promos_utils

#endif  // CHROME_BROWSER_PROMOS_PROMOS_UTILS_H_
