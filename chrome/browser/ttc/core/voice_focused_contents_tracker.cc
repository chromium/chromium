// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"

namespace ttc {

// VoiceFocusedContentsTracker::Create() is defined by each platform's
// implementation: voice_focused_contents_tracker_desktop.cc and
// voice_focused_contents_tracker_android.cc.

VoiceFocusedContentsTracker::VoiceFocusedContentsTracker() = default;

VoiceFocusedContentsTracker::~VoiceFocusedContentsTracker() = default;

void VoiceFocusedContentsTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VoiceFocusedContentsTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void VoiceFocusedContentsTracker::NotifyActiveTabChanged() {
  content::WebContents* web_contents = GetActiveWebContents();
  for (Observer& observer : observers_) {
    observer.OnActiveTabChanged(web_contents);
  }
}

}  // namespace ttc
