// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_MEDIA_STATE_H_
#define CHROME_BROWSER_TAB_MEDIA_STATE_H_

namespace tabs {

// LINT.IfChange(AndroidTabMediaState)
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.tab
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: MediaState
// Tracks the media state of the tab.
enum class MediaState {
  kNone = 0,
  kMuted = 1,
  kAudible = 2,
  kPictureInPicture = 3,
  kRecording = 4,
  kSharing = 5,
  kMaxValue = kSharing,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:AndroidTabMediaState)

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_MEDIA_STATE_H_
