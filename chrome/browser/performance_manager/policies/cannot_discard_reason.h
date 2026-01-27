// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_CANNOT_DISCARD_REASON_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_CANNOT_DISCARD_REASON_H_

namespace performance_manager::policies {

// LINT.IfChange(CannotDiscardReason)
// List of reasons not to discard a page. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class CannotDiscardReason {
  kNotATab = 0,
  kAlreadyDiscarded = 1,
  kDiscardAttempted = 2,
  kNoMainFrame = 3,
  kVisible = 4,
  kAudible = 5,
  kRecentlyAudible = 6,
  kRecentlyVisible = 7,
  kPictureInPicture = 8,
  kPdf = 9,
  kNotWebOrInternal = 10,
  kInvalidURL = 11,
  kOptedOut = 12,
  kNotificationsEnabled = 13,
  kExtensionProtected = 14,
  kCapturingVideo = 15,
  kCapturingAudio = 16,
  kBeingMirrored = 17,
  kCapturingWindow = 18,
  kCapturingDisplay = 19,
  kConnectedToBluetooth = 20,
  kConnectedToUSB = 21,
  kActiveTab = 22,
  kPinnedTab = 23,
  kDevToolsOpen = 24,
  kBackgroundActivity = 25,
  kFormInteractions = 26,
  kUserEdits = 27,
  kGlicShared = 28,
  kWebApp = 29,

  kMaxValue = kWebApp,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:CannotDiscardReason)

const char* CannotDiscardReasonToString(CannotDiscardReason reason);

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_CANNOT_DISCARD_REASON_H_
