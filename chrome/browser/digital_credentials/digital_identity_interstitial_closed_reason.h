// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_INTERSTITIAL_CLOSED_REASON_H_
#define CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_INTERSTITIAL_CLOSED_REASON_H_

// Do not reorder or change the values because the enum values are being
// recorded in metrics.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.digital_credentials
//
// The reason that the digital identity interstitial close dialog was closed.
enum class DigitalIdentityInterstitialClosedReason {
  kOther = 0,
  kOkButton = 1,
  kCancelButton = 2,
  kPageNavigated = 3,
  kMaxValue = kPageNavigated,
};

#endif  // CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_INTERSTITIAL_CLOSED_REASON_H_
