// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_ROLL_BACK_3PCD_NOTICE_ACTION_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_ROLL_BACK_3PCD_NOTICE_ACTION_H_

// Enum representing all possible actions taken on the 3PCD rollback notice.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
// LINT.IfChange(RollBack3pcdNoticeAction)
enum class RollBack3pcdNoticeAction {
  // Clicked the "Got it" button.
  kGotIt = 0,
  // Clicked the "Settings" button.
  kSettings = 1,
  // Closed the notice via gesture or close button - Android only.
  kClosed = 2,

  kMaxValue = kClosed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:RollBack3pcdNoticeAction)

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_ROLL_BACK_3PCD_NOTICE_ACTION_H_
