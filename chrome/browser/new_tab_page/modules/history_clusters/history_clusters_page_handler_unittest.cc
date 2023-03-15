// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_page_handler.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/test_history_clusters_service.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class MockCartService : public CartService {
 public:
  explicit MockCartService(Profile* profile) : CartService(profile) {}

  MOCK_METHOD2(HasActiveCartForURL,
               void(const GURL& url, base::OnceCallback<void(bool)> callback));
};

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      &HistoryClustersServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* browser_context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<history_clusters::TestHistoryClustersService>();
      }));
  profile_builder.AddTestingFactory(
      CartServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<MockCartService>(
            Profile::FromBrowserContext(context));
      }));
  profile_builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
    HistoryServiceFactory::GetDefaultFactory());
  return profile_builder.Build();
}

}  // namespace

class HistoryClustersPageHandlerTest : public testing::Test {
 public:
  HistoryClustersPageHandlerTest()
      : profile_(MakeTestingProfile()),
        test_history_clusters_service_(
            static_cast<history_clusters::TestHistoryClustersService*>(
                HistoryClustersServiceFactory::GetForBrowserContext(
                    profile_.get()))),
        mock_cart_service_(static_cast<MockCartService*>(
            CartServiceFactory::GetForProfile(profile_.get()))),
        handler_(std::make_unique<HistoryClustersPageHandler>(
            mojo::PendingReceiver<ntp::history_clusters::mojom::PageHandler>(),
            profile_.get())) {}

  history_clusters::TestHistoryClustersService&
  test_history_clusters_service() {
    return *test_history_clusters_service_;
  }

  MockCartService& mock_cart_service() { return *mock_cart_service_; }

  HistoryClustersPageHandler& handler() { return *handler_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  raw_ptr<history_clusters::TestHistoryClustersService>
      test_history_clusters_service_;
  raw_ptr<MockCartService> mock_cart_service_;
  std::unique_ptr<HistoryClustersPageHandler> handler_;
};

history::ClusterVisit SampleVisitForURL(GURL url) {
  history::VisitRow visit_row;
  visit_row.visit_id = 1;
  visit_row.visit_time = base::Time::Now();
  auto content_annotations = history::VisitContentAnnotations();
  content_annotations.has_url_keyed_image = true;
  content_annotations.related_searches = {"fruits", "red fruits",
                                          "healthy fruits"};
  history::AnnotatedVisit annotated_visit;
  annotated_visit.visit_row = std::move(visit_row);
  annotated_visit.content_annotations = std::move(content_annotations);
  std::string kSampleUrl = url.spec();
  history::ClusterVisit sample_visit;
  sample_visit.url_for_display = base::UTF8ToUTF16(kSampleUrl);
  sample_visit.normalized_url = url;
  sample_visit.annotated_visit = std::move(annotated_visit);
  sample_visit.score = 1.0f;
  return sample_visit;
}

history::Cluster SampleCluster() {
  history::ClusterVisit sample_visit =
      SampleVisitForURL(GURL("https://www.google.com"));
  std::string kSampleLabel = "LabelOne";
  return history::Cluster(1, {3, sample_visit},
                          {{u"apples", history::ClusterKeywordData()},
                           {u"Red Oranges", history::ClusterKeywordData()}},
                          /*should_show_on_prominent_ui_surfaces=*/true,
                          /*label=*/
                          l10n_util::GetStringFUTF16(
                              IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS,
                              base::UTF8ToUTF16(kSampleLabel)));
}

TEST_F(HistoryClustersPageHandlerTest, GetCluster) {
  base::HistogramTester histogram_tester;

  std::string kSampleLabel = "LabelOne";
  std::string kSampleUrl = "www.google.com";
  history::ClusterVisit kSampleVisit;
  kSampleVisit.url_for_display = base::UTF8ToUTF16(kSampleUrl);
  const history::Cluster kSampleCluster =
      history::Cluster(1, {kSampleVisit},
                       {{u"apples", history::ClusterKeywordData()},
                        {u"Red Oranges", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/false,
                       /*label=*/base::UTF8ToUTF16(kSampleLabel));

  const std::vector<history::Cluster> kSampleClusters = {kSampleCluster};
  test_history_clusters_service().SetClustersToReturn(kSampleClusters);

  history_clusters::mojom::ClusterPtr cluster_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClusterCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cluster_mojom](history_clusters::mojom::ClusterPtr cluster_arg) {
            cluster_mojom = std::move(cluster_arg);
          }));
  handler().GetCluster(callback.Get());
  ASSERT_TRUE(cluster_mojom);
  ASSERT_EQ(1u, cluster_mojom->id);
  ASSERT_EQ(kSampleLabel, cluster_mojom->label.value());
  ASSERT_EQ(1u, cluster_mojom->visits.size());
  ASSERT_EQ(kSampleUrl, cluster_mojom->visits[0]->url_for_display);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", true, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 1, 1);
}

TEST_F(HistoryClustersPageHandlerTest, MultipleClusters) {
  base::HistogramTester histogram_tester;

  std::string kSampleLabel = "LabelOne";
  std::string kSampleUrl = "www.google.com";
  history::ClusterVisit kSampleVisit;
  kSampleVisit.url_for_display = base::UTF8ToUTF16(kSampleUrl);
  const history::Cluster kSampleCluster =
      history::Cluster(1, {kSampleVisit},
                       {{u"apples", history::ClusterKeywordData()},
                        {u"Red Oranges", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/false,
                       /*label=*/base::UTF8ToUTF16(kSampleLabel));

  const std::vector<history::Cluster> kSampleClusters = {kSampleCluster,
                                                         kSampleCluster};
  test_history_clusters_service().SetClustersToReturn(kSampleClusters);

  history_clusters::mojom::ClusterPtr cluster_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClusterCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cluster_mojom](history_clusters::mojom::ClusterPtr cluster_arg) {
            cluster_mojom = std::move(cluster_arg);
          }));
  handler().GetCluster(callback.Get());
  ASSERT_TRUE(cluster_mojom);
  ASSERT_EQ(1u, cluster_mojom->id);
  ASSERT_EQ(kSampleLabel, cluster_mojom->label.value());
  ASSERT_EQ(1u, cluster_mojom->visits.size());
  ASSERT_EQ(kSampleUrl, cluster_mojom->visits[0]->url_for_display);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", true, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 2, 1);
}

TEST_F(HistoryClustersPageHandlerTest, NoClusters) {
  base::HistogramTester histogram_tester;

  history_clusters::mojom::ClusterPtr cluster_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClusterCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cluster_mojom](history_clusters::mojom::ClusterPtr cluster_arg) {
            cluster_mojom = std::move(cluster_arg);
          }));
  handler().GetCluster(callback.Get());
  // Callback should be invoked with a nullptr.
  ASSERT_FALSE(cluster_mojom);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

class HistoryClustersPageHandlerCartTest
    : public HistoryClustersPageHandlerTest {
 public:
  HistoryClustersPageHandlerCartTest() {
    features_.InitAndEnableFeature(ntp_features::kNtpChromeCartModule);
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(HistoryClustersPageHandlerCartTest, CheckClusterHasCart) {
  base::HistogramTester histogram_tester;
  std::string kSampleLabel = "LabelOne";
  const GURL url_A = GURL("https://www.foo.com");
  const GURL url_B = GURL("https://www.bar.com");
  const GURL url_C = GURL("https://www.baz.com");
  MockCartService& cart_service = mock_cart_service();

  const history::Cluster cluster =
      history::Cluster(1,
                       {SampleVisitForURL(url_A), SampleVisitForURL(url_B),
                        SampleVisitForURL(url_C)},
                       {{u"apples", history::ClusterKeywordData()},
                        {u"Red Oranges", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/true,
                       /*label=*/base::UTF8ToUTF16(kSampleLabel));
  test_history_clusters_service().SetClustersToReturn({cluster});

  // Vectors to capture mocked method args.
  std::vector<GURL> urls;
  std::vector<base::OnceCallback<void(bool)>> callbacks;
  EXPECT_CALL(cart_service, HasActiveCartForURL(testing::_, testing::_))
      .Times(cluster.visits.size())
      .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
          [&urls, &callbacks](GURL url,
                              base::OnceCallback<void(bool)> callback) -> void {
            urls.push_back(url);
            callbacks.push_back(std::move(callback));
          })));
  handler().GetCluster(base::DoNothing());
  // Simulate one URL being identified as having a cart.
  std::move(callbacks[0]).Run(true);
  for (size_t i = 1; i < callbacks.size(); i++) {
    std::move(callbacks[i]).Run(false);
  }

  for (size_t i = 0; i < urls.size(); i++) {
    EXPECT_EQ(urls[i], cluster.visits[i].normalized_url);
  }
  histogram_tester.ExpectBucketCount(
      "NewTabPage.HistoryClusters.HasCartForTopCluster", true, 1);

  urls.clear();
  callbacks.clear();
  EXPECT_CALL(cart_service, HasActiveCartForURL(testing::_, testing::_))
      .Times(cluster.visits.size())
      .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
          [&urls, &callbacks](GURL url,
                              base::OnceCallback<void(bool)> callback) -> void {
            urls.push_back(url);
            callbacks.push_back(std::move(callback));
          })));
  handler().GetCluster(base::DoNothing());
  // Simulate none URL being identified as having a cart.
  for (size_t i = 0; i < callbacks.size(); i++) {
    std::move(callbacks[i]).Run(false);
  }

  for (size_t i = 0; i < urls.size(); i++) {
    EXPECT_EQ(urls[i], cluster.visits[i].normalized_url);
  }
  histogram_tester.ExpectBucketCount(
      "NewTabPage.HistoryClusters.HasCartForTopCluster", false, 1);
  histogram_tester.ExpectTotalCount(
      "NewTabPage.HistoryClusters.HasCartForTopCluster", 2);
}
