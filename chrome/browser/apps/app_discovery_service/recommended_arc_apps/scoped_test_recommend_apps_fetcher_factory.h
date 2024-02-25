// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_SCOPED_TEST_RECOMMEND_APPS_FETCHER_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_SCOPED_TEST_RECOMMEND_APPS_FETCHER_FACTORY_H_

#include "base/functional/callback.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher.h"

namespace apps {

// Test helper class for registering a recommend apps fetcher factory callback.
class ScopedTestRecommendAppsFetcherFactory {
 public:
  explicit ScopedTestRecommendAppsFetcherFactory(
      const RecommendAppsFetcher::FactoryCallback& factory_callback);
  ~ScopedTestRecommendAppsFetcherFactory();

 private:
  RecommendAppsFetcher::FactoryCallback factory_callback_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_SCOPED_TEST_RECOMMEND_APPS_FETCHER_FACTORY_H_
