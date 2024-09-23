// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_history_observer_service.h"

#include <set>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "components/commerce/core/mock_shopping_service.h"
#endif

class BrowsingDataHistoryObserverServiceTest : public testing::Test {
 public:
  BrowsingDataHistoryObserverServiceTest() = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

#if BUILDFLAG(IS_ANDROID)

TEST_F(BrowsingDataHistoryObserverServiceTest,
       TimeRangeHistoryWithRestrictions_ClearCommerceDataCalled) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestingProfile> profile =
      TestingProfile::Builder()
          .AddTestingFactory(
              commerce::ShoppingServiceFactory::GetInstance(),
              base::BindRepeating([](content::BrowserContext* context) {
                std::unique_ptr<KeyedService> service =
                    commerce::MockShoppingService::Build();
                static_cast<commerce::MockShoppingService*>(service.get())
                    ->SetIsMerchantViewerEnabled(true);
                return service;
              }))
          .Build();
  BrowsingDataHistoryObserverService service(profile.get());

  GURL origin_a = GURL("https://a.test");

  std::set<GURL> restrict_urls = {origin_a};

  base::Time begin = base::Time::Now();
  base::Time end = begin + base::Days(1);
  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(begin, end), false /* is_from_expiration */,
      {} /* deleted_rows */, {} /* favicon_urls */,
      restrict_urls /* restrict_urls */);

  service.OnHistoryDeletions(nullptr /* history_service */, deletion_info);
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "MerchantViewer.DataManager.DeleteMerchantViewerDataForTimeRange", 0, 1);
}

TEST_F(BrowsingDataHistoryObserverServiceTest,
       OriginBasedCommerceDataCleared_EmptyList) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestingProfile> profile =
      TestingProfile::Builder()
          .AddTestingFactory(
              commerce::ShoppingServiceFactory::GetInstance(),
              base::BindRepeating([](content::BrowserContext* context) {
                std::unique_ptr<KeyedService> service =
                    commerce::MockShoppingService::Build();
                static_cast<commerce::MockShoppingService*>(service.get())
                    ->SetIsMerchantViewerEnabled(true);
                return service;
              }))
          .Build();
  BrowsingDataHistoryObserverService service(profile.get());

  history::OriginCountAndLastVisitMap origin_map;
  history::DeletionInfo deletion_info = history::DeletionInfo::ForUrls(
      {} /* deleted_rows */, {} /* favicon_urls */);
  deletion_info.set_deleted_urls_origin_map(std::move(origin_map));

  service.OnHistoryDeletions(nullptr /* history_service */, deletion_info);

  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "MerchantViewer.DataManager.DeleteMerchantViewerDataForOrigins", 0, 1);
}

#endif
