// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_H_
#define CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_H_

#include <map>

#include "base/observer_list.h"
#include "chrome/browser/permissions/last_tab_standing_tracker_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

// This class keep tracks of all open tabs. And notifies its observers when
// all tabs of a particular origin have been closed or navigated away from.
class LastTabStandingTracker : public KeyedService {
 public:
  LastTabStandingTracker();
  ~LastTabStandingTracker() override;

  LastTabStandingTracker(const LastTabStandingTracker&) = delete;
  LastTabStandingTracker& operator=(const LastTabStandingTracker&) = delete;

  void WebContentsLoadedOrigin(const url::Origin& origin);
  void WebContentsUnloadedOrigin(const url::Origin& origin);
  void AddObserver(LastTabStandingTrackerObserver* observer);
  void RemoveObserver(LastTabStandingTrackerObserver* observer);

  void Shutdown() override;

 private:
  base::ObserverList<LastTabStandingTrackerObserver> observer_list_;
  // Tracks how many tabs of a particular origin are open at any given time.
  std::map<url::Origin, int> tab_counter_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_H_
