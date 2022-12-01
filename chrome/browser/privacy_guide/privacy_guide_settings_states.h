// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_GUIDE_PRIVACY_GUIDE_SETTINGS_STATES_H_
#define CHROME_BROWSER_PRIVACY_GUIDE_PRIVACY_GUIDE_SETTINGS_STATES_H_

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
  kMaxValue = kSafeBrowsingStandardToStandard,
};

}  // namespace privacy_guide_metrics

#endif  // CHROME_BROWSER_PRIVACY_GUIDE_PRIVACY_GUIDE_SETTINGS_STATES_H_
