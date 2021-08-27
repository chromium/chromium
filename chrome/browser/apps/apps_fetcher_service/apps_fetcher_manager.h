// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_MANAGER_H_
#define CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_MANAGER_H_

#include <memory>

#include "chrome/browser/apps/apps_fetcher_service/apps_fetcher_util.h"

namespace apps {

// Interface implemented by app providers.
class AppsFetcher {
 public:
  virtual ~AppsFetcher() = default;

  virtual void GetApps(ResultCallback callback) = 0;
};

// Backend for app fetching requests.
class AppsFetcherManager {
 public:
  AppsFetcherManager();
  ~AppsFetcherManager();
  AppsFetcherManager(const AppsFetcherManager&) = delete;
  AppsFetcherManager& operator=(const AppsFetcherManager&) = delete;

  void GetApps(const ResultType& result_type, ResultCallback callback);

 private:
  std::unique_ptr<AppsFetcher> recommended_arc_apps_fetcher_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_MANAGER_H_
