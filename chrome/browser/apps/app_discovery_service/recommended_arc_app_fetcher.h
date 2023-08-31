// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APP_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APP_FETCHER_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher_delegate.h"

class Profile;

namespace apps {
class RecommendAppsFetcher;

class RecommendedArcAppFetcher : public AppFetcher,
                                 public RecommendAppsFetcherDelegate {
 public:
  explicit RecommendedArcAppFetcher(Profile* profile);
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
  raw_ptr<Profile> profile_;
  apps::ResultCallback callback_;
  std::unique_ptr<RecommendAppsFetcher> recommend_apps_fetcher_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APP_FETCHER_H_
