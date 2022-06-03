// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APP_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APP_FETCHER_H_

#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

namespace apps {

class RecommendedArcAppFetcher : public AppFetcher {
 public:
  RecommendedArcAppFetcher() = default;
  RecommendedArcAppFetcher(const RecommendedArcAppFetcher&) = delete;
  RecommendedArcAppFetcher& operator=(const RecommendedArcAppFetcher&) = delete;
  ~RecommendedArcAppFetcher() override = default;

  // AppFetcher:
  void GetApps(ResultCallback callback) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APP_FETCHER_H_
