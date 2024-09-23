// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ANDROID_AUTOFILL_AVAILABILITY_STATUS_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ANDROID_AUTOFILL_AVAILABILITY_STATUS_H_

namespace autofill {

// Indicates the status of Android Autofill availability. If multiple reasons
// prevent autofill, the first that applies is used. The initial set of values
// is ordered by strength of the set (e.g. a policy prevents version checks).
//
// This metric is recorded in metrics. Don't reorder or reuse values.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.autofill
enum class AndroidAutofillAvailabilityStatus {

  // Indicates that Android autofill can be used.
  kAvailable = 0,

  // The ThirdPartyPasswordManagersAllowed policy disallows android autofill.
  kNotAllowedByPolicy = 1,

  // The Android version doesn't provide a compatible Autofill framework.
  kAndroidVersionTooOld = 2,

  // The Autofill Manager is not available or even provided by the OEM.
  kAndroidAutofillManagerNotAvailable = 3,

  // Android Autofill is not supported, e.g. due to a device policy.
  kAndroidAutofillNotSupported = 4,

  // No Autofill Service is set (or an error prevented fetching it).
  kUnknownAndroidAutofillService = 5,

  // The Autofill Service is Autofill With Google. Should not fill Chrome.
  kAndroidAutofillServiceIsGoogle = 6,

  // The user did not enable Android autofill in settings.
  kSettingTurnedOff = 7,

  kMaxValue = kSettingTurnedOff
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ANDROID_AUTOFILL_AVAILABILITY_STATUS_H_
