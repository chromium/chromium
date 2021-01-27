// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_STATS_OBSERVER_H_
#define CHROME_BROWSER_METRICS_TAB_STATS_OBSERVER_H_

#include "base/observer_list_types.h"
#include "content/public/browser/visibility.h"

namespace content {
class WebContents;
}  // namespace content

namespace metrics {

// Defines an interface for a class used to handle tab and window related stats.
// Handling the events can be either storing them for logging purposes,
// forwarding them to another class or taking reactive measures when certain
// criteria are met.
class TabStatsObserver : public base::CheckedObserver {
 public:
  // Functions used to update the window/tab count.
  virtual void OnWindowAdded() = 0;
  virtual void OnWindowRemoved() = 0;
  virtual void OnTabAdded(content::WebContents* web_contents) = 0;
  virtual void OnTabRemoved(content::WebContents* web_contents) = 0;
  virtual void OnTabReplaced(content::WebContents* old_contents,
                             content::WebContents* new_contents) = 0;

  // Records that there's been a direct user interaction with a tab, see the
  // comment for |DidGetUserInteraction| in
  // content/public/browser/web_contents_observer.h for a list of the possible
  // type of interactions.
  virtual void OnTabInteraction(content::WebContents* web_contents) = 0;

  // Records that a tab became audible.
  virtual void OnTabAudible(content::WebContents* web_contents) = 0;

  // Records that a tab's visibility changed.
  virtual void OnTabVisibilityChanged(content::WebContents* web_contents,
                                      content::Visibility visibility) = 0;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_TAB_STATS_OBSERVER_H_
