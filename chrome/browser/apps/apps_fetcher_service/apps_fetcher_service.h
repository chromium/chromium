// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_SERVICE_H_
#define CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_SERVICE_H_

#include <memory>

#include "chrome/browser/apps/apps_fetcher_service/apps_fetcher_util.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace apps {

class AppsFetcherManager;

// API for consumers to use to fetch apps.
class AppsFetcherService : public KeyedService {
 public:
  explicit AppsFetcherService(Profile* profile);
  ~AppsFetcherService() override;
  AppsFetcherService(const AppsFetcherService&) = delete;
  AppsFetcherService& operator=(const AppsFetcherService&) = delete;

  // Queries for apps of the requested |result_type|.
  // |callback| is called when a response to the request is ready.
  void GetApps(const ResultType& result_type, ResultCallback callback);

 private:
  std::unique_ptr<AppsFetcherManager> apps_fetcher_manager_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_APPS_FETCHER_SERVICE_H_
