// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_SERVICE_OBSERVER_H_

// Observer for PromoService.
class PromoServiceObserver {
 public:
  // Called when the PromoData is updated, usually as the result of a
  // Refresh() call on the service. Note that this is called after each
  // Refresh(), even if the network request failed, or if it didn't result in an
  // actual change to the cached data. You can get the new data via
  // PromoService::promo_data().
  virtual void OnPromoDataUpdated() = 0;

  // Called when the PromoService is shutting down. Observers that might
  // outlive the service should use this to unregister themselves, and clear out
  // any pointers to the service they might hold.
  virtual void OnPromoServiceShuttingDown() {}
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_SERVICE_OBSERVER_H_
