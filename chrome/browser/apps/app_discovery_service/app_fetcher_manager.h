// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_FETCHER_MANAGER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_FETCHER_MANAGER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"

class Profile;

namespace apps {

// Interface implemented by app providers.
class AppFetcher {
 public:
  virtual ~AppFetcher() = default;

  virtual void GetApps(ResultCallback callback) = 0;
  virtual base::CallbackListSubscription RegisterForAppUpdates(
      RepeatingResultCallback callback);
  virtual void GetIcon(const std::string& icon_id,
                       int32_t size_hint_in_dip,
                       GetIconCallback callback);
};

// Backend for app fetching requests.
class AppFetcherManager {
 public:
  explicit AppFetcherManager(Profile* profile);
  AppFetcherManager(const AppFetcherManager&) = delete;
  AppFetcherManager& operator=(const AppFetcherManager&) = delete;
  ~AppFetcherManager();

  void GetApps(ResultType result_type, ResultCallback callback);
  base::CallbackListSubscription RegisterForAppUpdates(
      ResultType result_type,
      RepeatingResultCallback callback);
  void GetIcon(const std::string& icon_id,
               int32_t size_hint_in_dip,
               ResultType result_type,
               GetIconCallback callback);

  static void SetOverrideFetcherForTesting(AppFetcher* fetcher);

 private:
  raw_ptr<Profile> profile_;

  std::unique_ptr<AppFetcher> recommended_arc_app_fetcher_;
  std::unique_ptr<AppFetcher> almanac_fetcher_;

  static AppFetcher* g_test_fetcher_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_FETCHER_MANAGER_H_
