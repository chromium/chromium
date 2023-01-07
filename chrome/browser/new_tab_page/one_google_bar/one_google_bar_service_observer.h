// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_OBSERVER_H_

// Observer for OneGoogleBarService.
class OneGoogleBarServiceObserver {
 public:
  // Called when the OneGoogleBarData is updated, usually as the result of a
  // Refresh() call on the service. Note that this is called after each
  // Refresh(), even if the network request failed, or if it didn't result in an
  // actual change to the cached data. You can get the new data via
  // OneGoogleBarService::one_google_bar_data().
  virtual void OnOneGoogleBarDataUpdated() = 0;

  // Called when the OneGoogleBarService is shutting down. Observers that might
  // outlive the service should use this to unregister themselves, and clear out
  // any pointers to the service they might hold.
  virtual void OnOneGoogleBarServiceShuttingDown() {}
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_OBSERVER_H_
