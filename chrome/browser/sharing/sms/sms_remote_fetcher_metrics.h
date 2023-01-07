// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_METRICS_H_
#define CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_METRICS_H_

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WebOTPCrossDeviceFailure {
  kNoFailure = 0,
  kFeatureDisabled = 1,
  kNoRemoteDevice = 2,
  kNoSharingService = 3,
  kSharingMessageFailure = 4,
  kAPIFailureOnAndroid = 5,
  kAndroidToAndroidNotSupported = 6,
  kMaxValue = kAndroidToAndroidNotSupported,
};

// Records why using WebOTP API on desktop failed on the sharing path.
void RecordWebOTPCrossDeviceFailure(WebOTPCrossDeviceFailure failure);

#endif  // CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_METRICS_H_
