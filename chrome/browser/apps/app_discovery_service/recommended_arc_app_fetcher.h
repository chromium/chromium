// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APP_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APP_FETCHER_H_

#include "base/values.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"
#include "chrome/browser/ash/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"

namespace ash {
class RecommendAppsFetcher;
}

namespace apps {

class RecommendedArcAppFetcher : public AppFetcher,
                                 public ash::RecommendAppsFetcherDelegate {
 public:
  RecommendedArcAppFetcher();
  RecommendedArcAppFetcher(const RecommendedArcAppFetcher&) = delete;
  RecommendedArcAppFetcher& operator=(const RecommendedArcAppFetcher&) = delete;
  ~RecommendedArcAppFetcher() override;

  // AppFetcher:
  void GetApps(ResultCallback callback) override;

  // RecommendAppsFetcherDelegate:
  void OnLoadSuccess(base::Value app_list) override;
  void OnLoadError() override;
  void OnParseResponseError() override;

  // For Testing:
  void SetCallbackForTesting(ResultCallback callback);

 private:
  apps::ResultCallback callback_;
  std::unique_ptr<ash::RecommendAppsFetcher> recommend_apps_fetcher_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APP_FETCHER_H_
