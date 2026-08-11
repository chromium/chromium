// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_GUIDE_PRIVACY_GUIDE_H_
#define CHROME_BROWSER_PRIVACY_GUIDE_PRIVACY_GUIDE_H_

namespace privacy_guide_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Must be kept in sync with SettingsPrivacyGuideSettingsStates in
// histograms/metadata/settings/enums.xml and PrivacyGuideSettingsStates in
// resources/settings/metrics_browser_proxy.ts.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_guide
// LINT.IfChange(PrivacyGuideSettingsStates)
enum class PrivacyGuideSettingsStates {
  kMSBBOnToOn = 0,
  kMSBBOnToOff = 1,
  kMSBBOffToOn = 2,
  kMSBBOffToOff = 3,
  kBlock3PIncognitoTo3PIncognito = 4,
  kBlock3PIncognitoTo3P = 5,
  kBlock3PTo3PIncognito = 6,
  kBlock3PTo3P = 7,
  kHistorySyncOnToOn = 8,
  kHistorySyncOnToOff = 9,
  kHistorySyncOffToOn = 10,
  kHistorySyncOffToOff = 11,
  kSafeBrowsingEnhancedToEnhanced = 12,
  kSafeBrowsingEnhancedToStandard = 13,
  kSafeBrowsingStandardToEnhanced = 14,
  kSafeBrowsingStandardToStandard = 15,
  kSearchSuggestionsOnToOn = 16,
  kSearchSuggestionsOnToOff = 17,
  kSearchSuggestionsOffToOn = 18,
  kSearchSuggestionsOffToOff = 19,
  // kAdTopicsOnToOn = 20, // Obsolete
  // kAdTopicsOnToOff = 21, // Obsolete
  // kAdTopicsOffToOn = 22, // Obsolete
  // kAdTopicsOffToOff = 23, // Obsolete
  kMaxValue = kSearchSuggestionsOffToOff,
};
// LINT.ThenChange(
//   //chrome/browser/resources/settings/metrics_browser_proxy.ts:PrivacyGuideSettingsStates,
//   //tools/metrics/histograms/metadata/settings/enums.xml:SettingsPrivacyGuideSettingsStates
// )

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Must be kept in sync with SettingsPrivacyGuideInteractions in
// histograms/metadata/settings/enums.xml and SettingsPrivacyGuideInteractions
// in resources/settings/metrics_browser_proxy.ts.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_guide
// LINT.IfChange(PrivacyGuideInteractions)
enum class PrivacyGuideInteractions {
  kWelcomeNextButton = 0,
  kMSBBNextButton = 1,
  kHistorySyncNextButton = 2,
  kSafeBrowsingNextButton = 3,
  kCookiesNextButton = 4,
  kCompletionNextButton = 5,
  kSettingsLinkRowEntry = 6,
  kPromoEntry = 7,
  kSWAACompletionLink = 8,
  // kPrivacySandboxCompletionLink = 9, // Obsolete
  kSearchSuggestionsNextButton = 10,
  // kTrackingProtectionCompletionLink = 11, // Obsolete
  // kAdTopicsNextButton = 12, // Obsolete
  kAiSettingsCompletionLink = 13,
  kMaxValue = kAiSettingsCompletionLink,
};
// LINT.ThenChange(
//   //chrome/browser/resources/settings/metrics_browser_proxy.ts:PrivacyGuideInteractions,
//   //tools/metrics/histograms/metadata/settings/enums.xml:SettingsPrivacyGuideInteractions
// )

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Must be kept in sync with SettingsPrivacyGuideStepsEligibleAndReached in
// histograms/enums.xml and SettingsPrivacyGuideStepsEligibleAndReached in
// resources/settings/metrics_browser_proxy.ts.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_guide
// LINT.IfChange(PrivacyGuideStepsEligibleAndReached)
enum class PrivacyGuideStepsEligibleAndReached {
  kMSBBEligible = 0,
  kMSBBReached = 1,
  kHistorySyncEligible = 2,
  kHistorySyncReached = 3,
  kSafeBrowsingEligible = 4,
  kSafeBrowsingReached = 5,
  kCookiesEligible = 6,
  kCookiesReached = 7,
  kCompletionEligible = 8,
  kCompletionReached = 9,
  kSearchSuggestionsEligible = 10,
  kSearchSuggestionsReached = 11,
  // kAdTopicsEligible = 12, // Obsolete
  // kAdTopicsReached = 13, // Obsolete
};
// LINT.ThenChange(
//   //chrome/browser/resources/settings/metrics_browser_proxy.ts:PrivacyGuideStepsEligibleAndReached,
//   //tools/metrics/histograms/metadata/settings/enums.xml:SettingsPrivacyGuideStepsEligibleAndReached
// )

}  // namespace privacy_guide_metrics

#endif  // CHROME_BROWSER_PRIVACY_GUIDE_PRIVACY_GUIDE_H_
