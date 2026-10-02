// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace ttc {

// VoiceFocusedContentsTracker::Create() is defined by each platform's
// implementation: voice_focused_contents_tracker_desktop.cc and
// voice_focused_contents_tracker_android.cc.

VoiceFocusedContentsTracker::VoiceFocusedContentsTracker() {
  // Notify async to avoid clients depending on this happening
  // synchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&VoiceFocusedContentsTracker::NotifyActiveTabChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

VoiceFocusedContentsTracker::~VoiceFocusedContentsTracker() = default;

void VoiceFocusedContentsTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VoiceFocusedContentsTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void VoiceFocusedContentsTracker::NotifyActiveTabChanged() {
  if (observers_.empty()) {
    return;
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  content::WebContents* const web_contents = GetActiveWebContents();
  for (Observer& observer : observers_) {
    observer.OnVoiceFocusedContentsChanged(web_contents);
  }
}

}  // namespace ttc
