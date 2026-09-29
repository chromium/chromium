// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_DESKTOP_H_
#define CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_DESKTOP_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

class BrowserWindowInterface;
class Profile;

namespace ttc {

// Desktop implementation of VoiceFocusedContentsTracker, which binds to a
// BrowserWindowInterface and follows window activation through
// ProfileBrowserCollection.
class VoiceFocusedContentsTrackerDesktop : public VoiceFocusedContentsTracker,
                                           public BrowserCollectionObserver {
 public:
  explicit VoiceFocusedContentsTrackerDesktop(Profile& profile);
  ~VoiceFocusedContentsTrackerDesktop() override;

  // VoiceFocusedContentsTracker:
  content::WebContents* GetActiveWebContents() const override;

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

 private:
  // Binds to `browser` and starts following its active tab.
  void BindToBrowser(BrowserWindowInterface* browser);

  void OnBoundBrowserActiveTabChanged(BrowserWindowInterface* browser);

  // The browser window this tracker is bound to. Null until a window is bound,
  // or after all windows of the profile close.
  raw_ptr<BrowserWindowInterface> bound_browser_ = nullptr;

  base::CallbackListSubscription active_tab_subscription_;

  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_DESKTOP_H_
