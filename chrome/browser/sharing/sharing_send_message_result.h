// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_SEND_MESSAGE_RESULT_H_
#define CHROME_BROWSER_SHARING_SHARING_SEND_MESSAGE_RESULT_H_

// Result of sending SharingMessage via sharing service.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.sharing
enum class SharingSendMessageResult {
  kSuccessful = 0,
  kDeviceNotFound = 1,
  kNetworkError = 2,
  kPayloadTooLarge = 3,
  kAckTimeout = 4,
  kInternalError = 5,
  kMaxValue = kInternalError,
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SEND_MESSAGE_RESULT_H_
