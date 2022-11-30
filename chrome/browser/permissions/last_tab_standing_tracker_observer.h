// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_OBSERVER_H_
#define CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "url/origin.h"

class LastTabStandingTrackerObserver : public base::CheckedObserver {
 public:
  // Event fired when the last tab in a given Profile whose top-level document
  // is from |origin| is closed or navigated away.
  virtual void OnLastPageFromOriginClosed(const url::Origin&) = 0;

  // Event fired to let the observers know that the BrowserContext is going to
  // shut down.
  // The observers don't need to take care of removing themselves as an
  // observer.
  virtual void OnShutdown() = 0;
};

#endif  // CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_OBSERVER_H_
