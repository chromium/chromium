// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/discount/discount_processor.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/discount/discount.mojom.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

history_clusters::mojom::ClusterPtr SampleClusterWithVisits(
    const std::vector<GURL>& urls) {
  auto cluster_mojom = history_clusters::mojom::Cluster::New();
  for (const GURL& url : urls) {
    auto visit_mojom = history_clusters::mojom::URLVisit::New();
    visit_mojom->normalized_url = url;
    cluster_mojom->visits.push_back(std::move(visit_mojom));
  }
  return cluster_mojom;
}
}  // namespace

class DiscountProcessorTest : public testing::Test {
 public:
  DiscountProcessorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    discount_processor_ =
        std::make_unique<DiscountProcessor>(&mock_shopping_service_);
  }

  DiscountProcessor& discount_processor() { return *discount_processor_; }

  commerce::MockShoppingService& mock_shopping_service() {
    return mock_shopping_service_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  commerce::MockShoppingService mock_shopping_service_;
  std::unique_ptr<DiscountProcessor> discount_processor_;
};

TEST_F(DiscountProcessorTest, TestGetDiscountsForCluster) {
  const GURL url_foo = GURL("https://www.foo.com");
  const GURL url_bar = GURL("https://www.bar.com");
  const std::vector<GURL> urls = {url_foo, url_bar};
  auto cluster_mojom = SampleClusterWithVisits(urls);

  auto discount_map = commerce::DiscountsMap();
  auto discount_foo = commerce::DiscountInfo();
  discount_foo.value_in_text = "15% off";
  discount_foo.discount_code = "discount_123";
  discount_map[url_foo] = {discount_foo};

  mock_shopping_service().SetResponseForGetDiscountInfoForUrls(
      std::move(discount_map));

  // Capture the discount mojom that is finally returned.
  base::flat_map<
      GURL, std::vector<ntp::history_clusters::discount::mojom::DiscountPtr>>
      map;
  base::MockCallback<
      ntp::history_clusters::mojom::PageHandler::GetDiscountsForClusterCallback>
      callback;
  EXPECT_CALL(mock_shopping_service(), GetDiscountInfoForUrls(urls, testing::_))
      .Times(1);
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&map](base::flat_map<
                 GURL, std::vector<
                           ntp::history_clusters::discount::mojom::DiscountPtr>>
                     discount_map) { map = std::move(discount_map); }));

  discount_processor().GetDiscountsForCluster(std::move(cluster_mojom),
                                              callback.Get());
  task_environment_.RunUntilIdle();

  ASSERT_EQ(map.size(), 1u);
  ASSERT_TRUE(map.contains(url_foo));
  ASSERT_EQ(map[url_foo].size(), 1u);
  ASSERT_EQ(map[url_foo][0]->value_in_text, "15% off");
  const GURL annotated_url = GURL(
      "https://www.foo.com/"
      "?utm_source=chrome&utm_medium=app&utm_campaign=chrome-history-cluster-"
      "with-discount");
  ASSERT_EQ(map[url_foo][0]->annotated_visit_url, annotated_url);
}
