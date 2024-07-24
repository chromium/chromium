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
// histograms/enums.xml and PrivacyGuideSettingsStates in
// resources/settings/metrics_browser_proxy.ts.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_guide
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
  kAdTopicsOnToOn = 20,
  kAdTopicsOnToOff = 21,
  kAdTopicsOffToOn = 22,
  kAdTopicsOffToOff = 23,
  kMaxValue = kAdTopicsOffToOff,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Must be kept in sync with SettingsPrivacyGuideInteractions in
// histograms/enums.xml and SettingsPrivacyGuideInteractions in
// resources/settings/metrics_browser_proxy.ts.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_guide
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
  kPrivacySandboxCompletionLink = 9,
  kSearchSuggestionsNextButton = 10,
  kTrackingProtectionCompletionLink = 11,
  kAdTopicsNextButton = 12,
  kMaxValue = kAdTopicsNextButton,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Must be kept in sync with SettingsPrivacyGuideStepsEligibleAndReached in
// histograms/enums.xml and SettingsPrivacyGuideStepsEligibleAndReached in
// resources/settings/metrics_browser_proxy.ts.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_guide
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
  kAdTopicsEligible = 12,
  kAdTopicsReached = 13,
};

}  // namespace privacy_guide_metrics

#endif  // CHROME_BROWSER_PRIVACY_GUIDE_PRIVACY_GUIDE_H_
