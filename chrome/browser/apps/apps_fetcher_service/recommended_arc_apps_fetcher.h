// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_RECOMMENDED_ARC_APPS_FETCHER_H_
#define CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_RECOMMENDED_ARC_APPS_FETCHER_H_

#include "chrome/browser/apps/apps_fetcher_service/apps_fetcher_manager.h"
#include "chrome/browser/apps/apps_fetcher_service/apps_fetcher_util.h"

namespace apps {

class RecommendedArcAppsFetcher : public AppsFetcher {
 public:
  RecommendedArcAppsFetcher() = default;
  ~RecommendedArcAppsFetcher() override = default;
  RecommendedArcAppsFetcher(const RecommendedArcAppsFetcher&) = delete;
  RecommendedArcAppsFetcher& operator=(const RecommendedArcAppsFetcher&) =
      delete;

  // AppsFetcher:
  void GetApps(ResultCallback callback) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APPS_FETCHER_SERVICE_RECOMMENDED_ARC_APPS_FETCHER_H_
