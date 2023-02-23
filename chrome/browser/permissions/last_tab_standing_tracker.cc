// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/last_tab_standing_tracker.h"

#include "base/observer_list.h"
#include "url/gurl.h"

LastTabStandingTracker::LastTabStandingTracker() = default;

LastTabStandingTracker::~LastTabStandingTracker() = default;

void LastTabStandingTracker::Shutdown() {
  for (auto& observer : observer_list_) {
    observer.OnShutdown();
  }
  observer_list_.Clear();
}

void LastTabStandingTracker::AddObserver(
    LastTabStandingTrackerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void LastTabStandingTracker::RemoveObserver(
    LastTabStandingTrackerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void LastTabStandingTracker::WebContentsLoadedOrigin(
    const url::Origin& origin) {
  if (origin.opaque())
    return;
  // There are cases where chrome://newtab/ and chrome://new-tab-page/ are
  // used synonymously causing inconsistencies in the map. So we just ignore
  // them.
  if (origin == url::Origin::Create(GURL("chrome://newtab/")) ||
      origin == url::Origin::Create(GURL("chrome://new-tab-page/")))
    return;
  tab_counter_[origin]++;
}

void LastTabStandingTracker::WebContentsUnloadedOrigin(
    const url::Origin& origin) {
  if (origin.opaque())
    return;
  if (origin == url::Origin::Create(GURL("chrome://newtab/")) ||
      origin == url::Origin::Create(GURL("chrome://new-tab-page/")))
    return;
  DCHECK(tab_counter_.find(origin) != tab_counter_.end());
  tab_counter_[origin]--;
  if (tab_counter_[origin] <= 0) {
    tab_counter_.erase(origin);
    for (auto& observer : observer_list_)
      observer.OnLastPageFromOriginClosed(origin);
  }
}
