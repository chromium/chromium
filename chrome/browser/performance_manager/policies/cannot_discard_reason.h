// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_CANNOT_DISCARD_REASON_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_CANNOT_DISCARD_REASON_H_

namespace performance_manager::policies {

// List of reasons not to discard a page.
enum class CannotDiscardReason {
  kNotATab,
  kAlreadyDiscarded,
  kDiscardAttempted,
  kNoMainFrame,
  kVisible,
  kAudible,
  kRecentlyAudible,
  kRecentlyVisible,
  kPictureInPicture,
  kPdf,
  kNotWebOrInternal,
  kInvalidURL,
  kOptedOut,
  kNotificationsEnabled,
  kExtensionProtected,
  kCapturingVideo,
  kCapturingAudio,
  kBeingMirrored,
  kCapturingWindow,
  kCapturingDisplay,
  kConnectedToBluetooth,
  kConnectedToUSB,
  kActiveTab,
  kPinnedTab,
  kDevToolsOpen,
  kBackgroundActivity,
  kFormInteractions,
  kUserEdits,
  kGlicShared,
  kWebApp
};

const char* CannotDiscardReasonToString(CannotDiscardReason reason);

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_CANNOT_DISCARD_REASON_H_
