// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/cannot_discard_reason.h"

#include "base/notreached.h"

namespace performance_manager::policies {

// Converts a CannotDiscardReason to a string.
const char* CannotDiscardReasonToString(CannotDiscardReason reason) {
  switch (reason) {
    case CannotDiscardReason::kNotATab:
      return "The Page is not a tab";
    case CannotDiscardReason::kAlreadyDiscarded:
      return "Tab is already discarded";
    case CannotDiscardReason::kDiscardAttempted:
      return "Tab discarding has already been attempted";
    case CannotDiscardReason::kNoMainFrame:
      return "Tab doesn't have a main frame";
    case CannotDiscardReason::kVisible:
      return "Tab is currently visible";
    case CannotDiscardReason::kAudible:
      return "Tab is playing audio";
    case CannotDiscardReason::kRecentlyAudible:
      return "Tab has recently played audio";
    case CannotDiscardReason::kRecentlyVisible:
      return "Tab is recently visible";
    case CannotDiscardReason::kPictureInPicture:
      return "Tab is displaying content in picture-in-picture";
    case CannotDiscardReason::kPdf:
      return "Tab is hosting a PDF";
    case CannotDiscardReason::kNotWebOrInternal:
      return "URL scheme is not http, https, chrome or data";
    case CannotDiscardReason::kInvalidURL:
      return "URL is invalid";
    case CannotDiscardReason::kOptedOut:
      return "Tab was opted out via a setting or enterprise policy";
    case CannotDiscardReason::kNotificationsEnabled:
      return "Notification permission is granted";
    case CannotDiscardReason::kExtensionProtected:
      return "Tab is protected by an extension";
    case CannotDiscardReason::kCapturingVideo:
      return "Tab is capturing video";
    case CannotDiscardReason::kCapturingAudio:
      return "Tab is capturing audio";
    case CannotDiscardReason::kBeingMirrored:
      return "Tab is currently being mirrored (casting, etc)";
    case CannotDiscardReason::kCapturingWindow:
      return "Tab is currently capturing a window";
    case CannotDiscardReason::kCapturingDisplay:
      return "Tab is currently capturing a display";
    case CannotDiscardReason::kConnectedToBluetooth:
      return "Tab is currently connected to a bluetooth device";
    case CannotDiscardReason::kConnectedToUSB:
      return "Tab is currently connected to a USB device";
    case CannotDiscardReason::kActiveTab:
      return "Tab is currently active";
    case CannotDiscardReason::kPinnedTab:
      return "Tab was pinned";
    case CannotDiscardReason::kDevToolsOpen:
      return "Tab is currently using DevTools";
    case CannotDiscardReason::kBackgroundActivity:
      return "Tab is updating favicon or title in the background";
    case CannotDiscardReason::kFormInteractions:
      return "Tab has form interactions";
    case CannotDiscardReason::kUserEdits:
      return "The user has edited the tab's content";
    case CannotDiscardReason::kGlicShared:
      return "Tab is currently shared with Gemini";
    case CannotDiscardReason::kWebApp:
      return "Tab is a web application";
  }
  NOTREACHED();
}

}  // namespace performance_manager::policies
