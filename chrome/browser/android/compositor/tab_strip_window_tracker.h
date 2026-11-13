// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_STRIP_WINDOW_TRACKER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_STRIP_WINDOW_TRACKER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class Profile;

namespace android {

// Counts, per Profile, the live tab strip windows that registered. Creating a
// tracker registers one window; destroying it unregisters it. The count lives
// on the Profile, so it goes away with the Profile.
class TabStripWindowTracker {
 public:
  explicit TabStripWindowTracker(Profile* profile);
  TabStripWindowTracker(const TabStripWindowTracker&) = delete;
  TabStripWindowTracker& operator=(const TabStripWindowTracker&) = delete;
  ~TabStripWindowTracker();

  // Whether another window of the same profile was registered when this
  // tracker was created.
  bool had_existing_window() const { return had_existing_window_; }

 private:
  // Weak because the Profile (e.g. an OTR profile) can be destroyed before the
  // tracker's owner (e.g. the scene layer of an incognito window).
  base::WeakPtr<Profile> profile_;
  const bool had_existing_window_;
};

// Records Android.TabStrip.NewTabButton.ActivityCreationToFirstPaint and its
// With/WithoutExistingWindow partition.
void RecordNewTabButtonActivityCreationToFirstPaint(base::TimeDelta duration,
                                                    bool had_existing_window);

// Records the Android.TabStrip.NewTabButton.WebUI.{Phase} startup phase
// histograms of the WebUI new tab button and their With/WithoutExistingWindow
// partition.
void RecordNewTabButtonWebUiStartupPhases(
    base::TimeDelta navigation_start_to_commit,
    base::TimeDelta commit_to_dom_content_loaded,
    base::TimeDelta dom_content_loaded_to_first_paint,
    bool had_existing_window);

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_STRIP_WINDOW_TRACKER_H_
