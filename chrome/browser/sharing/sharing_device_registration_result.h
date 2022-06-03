// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_RESULT_H_
#define CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_RESULT_H_

// Result of device registration with Sharing.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingDeviceRegistrationResult" in src/tools/metrics/histograms/enums.xml.
enum class SharingDeviceRegistrationResult {
  // Operation is successful.
  kSuccess = 0,
  // Failed with Sync not ready.
  kSyncServiceError = 1,
  // Failed with encryption related error.
  kEncryptionError = 2,
  // Failed with FCM transient error.
  kFcmTransientError = 3,
  // Failed with FCM fatal error.
  kFcmFatalError = 4,
  // Device has not been registered.
  kDeviceNotRegistered = 5,
  // Other internal error.
  kInternalError = 6,
  // Max value for historgram.
  kMaxValue = kInternalError,
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_RESULT_H_
