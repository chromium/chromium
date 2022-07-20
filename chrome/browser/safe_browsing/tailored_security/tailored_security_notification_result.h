// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_NOTIFICATION_RESULT_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_NOTIFICATION_RESULT_H_

// This represents the result of trying to show a notification to the user when
// the state of the account tailored security bit changes. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class TailoredSecurityNotificationResult {
  kUnknownResult = 0,
  kShown = 1,
  // All other results are the reason for not being shown.
  kAccountNotConsented = 2,
  kEnhancedProtectionAlreadyEnabled = 3,
  kNoWebContentsAvailable = 4,
  kSafeBrowsingControlledByPolicy = 5,
  kNoBrowserAvailable = 6,
  kMaxValue = kNoBrowserAvailable,
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_NOTIFICATION_RESULT_H_
