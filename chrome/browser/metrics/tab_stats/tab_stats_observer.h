// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_OBSERVER_H_
#define CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_OBSERVER_H_

#include "base/observer_list_types.h"
#include "content/public/browser/visibility.h"

namespace content {
class WebContents;
}  // namespace content

namespace metrics {

// Defines an interface for a class used to handle tab and window related stats.
// Handling the events can be either storing them for logging purposes,
// forwarding them to another class or taking reactive measures when certain
// criteria are met. Subclasses do not have to react to all events so default
// noop functions are provided.
class TabStatsObserver : public base::CheckedObserver {
 public:
  // Functions used to update the window count.
  virtual void OnWindowAdded() {}
  virtual void OnWindowRemoved() {}

  // Functions used to update the tab count.
  // NOTE: It's not guaranteed that the observer methods related to the tab
  // state will be called before receiving a |OnTabRemoved| call. E.g. if
  // an observer is interested in tracking all the visible tabs it should
  // check |web_contents| when receiving a |OnTabRemoved| call to maintain its
  // internal state.
  virtual void OnTabAdded(content::WebContents* web_contents) {}
  virtual void OnTabRemoved(content::WebContents* web_contents) {}
  virtual void OnTabReplaced(content::WebContents* old_contents,
                             content::WebContents* new_contents) {}

  // Called whenever a main frame navigation to a different document is
  // committed in any of the observed tabs.
  virtual void OnPrimaryMainFrameNavigationCommitted(
      content::WebContents* web_contents) {}

  // Records that there's been a direct user interaction with a tab, see the
  // comment for |DidGetUserInteraction| in
  // content/public/browser/web_contents_observer.h for a list of the possible
  // type of interactions.
  virtual void OnTabInteraction(content::WebContents* web_contents) {}

  // Records that a tab's audible state changed.
  virtual void OnTabIsAudibleChanged(content::WebContents* web_contents) {}

  // Records that a tab's visibility changed.
  virtual void OnTabVisibilityChanged(content::WebContents* web_contents) {}

  // Records that a tab has been discarded.
  virtual void OnTabDiscarded(content::WebContents* web_contents) {}

  // Invoked when media enters or exits fullscreen, see
  // WebContentsImpl::MediaEffectivelyFullscreenChanged for more details.
  virtual void OnMediaEffectivelyFullscreenChanged(
      content::WebContents* web_contents,
      bool is_fullscreen) {}

  // Invoked when a media is destroyed. Note: When a fullscreen media is
  // destroyed, this will be invoked but not necessarily
  // OnMediaEffectivelyFullscreenChanged().
  virtual void OnMediaDestroyed(content::WebContents* web_contents) {}

  // Called whenever a tab starts playing video. If this tab has multiple video
  // players this will only be called when the first one starts.
  virtual void OnVideoStartedPlaying(content::WebContents* web_contents) {}

  // Called whenever a tab stops playing video. If this tab has multiple video
  // players this will only be called when there's no more player playing a
  // video.
  virtual void OnVideoStoppedPlaying(content::WebContents* web_contents) {}

  // NOTE: TabStatsTracker::AddObserverAndSetInitialState should be updated
  // after adding a new method to this interface.
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_OBSERVER_H_
