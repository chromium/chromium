// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_AUDIBLE_CONTENTS_TRACKER_H_
#define CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_AUDIBLE_CONTENTS_TRACKER_H_

#include <set>

#include "base/callback.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace metrics {

// BrowserList / TabStripModelObserver used for tracking audio status.
// TODO(chrisha): Migrate this entire thing to use RecentlyAudibleHelper
// notifications rather then TabStripModel notifications.
// https://crbug.com/846374
class AudibleContentsTracker : public BrowserListObserver,
                               public TabStripModelObserver {
 public:
  // Interface for an observer of the AudibleContentsTracker. The only client
  // of this class is the DesktopSessionDurationTracker, but an observer
  // interface has been created for ease of testing.
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    // Invoked when a first audio source starts playing after a period of no
    // audio sources.
    virtual void OnAudioStart() = 0;

    // Invoked when all audio sources stop playing.
    virtual void OnAudioEnd() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Creates an audible contents tracker that dispatches its messages to the
  // provided |observer|.
  explicit AudibleContentsTracker(Observer* observer);
  ~AudibleContentsTracker() override;

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* web_contents,
                    int index,
                    TabChangeType change_type) override;

  // Used for managing audible_contents_, and invoking OnAudioStart and
  // OnAudioEnd callbacks.
  void AddAudibleWebContents(content::WebContents* web_contents);
  void RemoveAudibleWebContents(content::WebContents* web_contents);

  Observer* observer_;

  // The set of WebContents that are currently playing audio.
  std::set<content::WebContents*> audible_contents_;

  DISALLOW_COPY_AND_ASSIGN(AudibleContentsTracker);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_AUDIBLE_CONTENTS_TRACKER_H_
