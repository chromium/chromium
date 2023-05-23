// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/test_history_clusters_service.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
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

constexpr char kSampleNonSearchUrl[] = "https://www.foo.com/";
constexpr char kSampleSearchUrl[] = "https://default-engine.com/search?q=foo";

const TemplateURLService::Initializer kTemplateURLData[] = {
    {"default-engine.com", "http://default-engine.com/search?q={searchTerms}",
     "Default"},
    {"non-default-engine.com", "http://non-default-engine.com?q={searchTerms}",
     "Not Default"},
};

class HistoryClustersModuleServiceTest : public testing::Test {
 public:
  HistoryClustersModuleServiceTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    testing_profile_ = profile_builder.Build();

    test_history_clusters_service_ =
        std::make_unique<history_clusters::TestHistoryClustersService>();
    mock_cart_service_ =
        std::make_unique<MockCartService>(testing_profile_.get());
    template_url_service_ = std::make_unique<TemplateURLService>(
        kTemplateURLData, std::size(kTemplateURLData));
    history_clusters_module_service_ =
        std::make_unique<HistoryClustersModuleService>(
            test_history_clusters_service_.get(), mock_cart_service_.get(),
            template_url_service_.get(),
            /*optimization_guide_keyed_service=*/nullptr);
  }

  history_clusters::TestHistoryClustersService&
  test_history_clusters_service() {
    return *test_history_clusters_service_;
  }

  MockCartService& mock_cart_service() { return *mock_cart_service_; }

  HistoryClustersModuleService& service() {
    return *history_clusters_module_service_;
  }

  std::vector<history::Cluster> GetClusters() {
    std::vector<history::Cluster> clusters;

    base::RunLoop run_loop;
    auto task = service().GetClusters(base::BindOnce(
        [](base::RunLoop* run_loop, std::vector<history::Cluster>* out_clusters,
           std::vector<history::Cluster> clusters,
           base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
               ranking_signals) {
          *out_clusters = std::move(clusters);
          run_loop->Quit();
        },
        &run_loop, &clusters));

    run_loop.Run();

    return clusters;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<history_clusters::TestHistoryClustersService>
      test_history_clusters_service_;
  std::unique_ptr<MockCartService> mock_cart_service_;
  std::unique_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<HistoryClustersModuleService>
      history_clusters_module_service_;
};

history::ClusterVisit SampleVisitForURL(
    GURL url,
    bool has_url_keyed_image = true,
    const std::vector<std::string>& related_searches = {}) {
  history::VisitRow visit_row;
  visit_row.visit_id = 1;
  visit_row.visit_time = base::Time::Now();
  visit_row.is_known_to_sync = true;
  auto content_annotations = history::VisitContentAnnotations();
  content_annotations.has_url_keyed_image = has_url_keyed_image;
  content_annotations.related_searches = related_searches;
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

history::Cluster SampleCluster(int id,
                               int srp_visits,
                               int non_srp_visits,
                               const std::vector<std::string> related_searches =
                                   {"fruits", "red fruits", "healthy fruits"}) {
  history::ClusterVisit sample_srp_visit =
      SampleVisitForURL(GURL(kSampleSearchUrl), false);
  history::ClusterVisit sample_non_srp_visit =
      SampleVisitForURL(GURL(kSampleNonSearchUrl), true, related_searches);

  std::vector<history::ClusterVisit> visits;
  visits.insert(visits.end(), srp_visits, sample_srp_visit);
  visits.insert(visits.end(), non_srp_visits, sample_non_srp_visit);

  std::string kSampleLabel = "LabelOne";
  return history::Cluster(id, std::move(visits),
                          {{u"apples", history::ClusterKeywordData()},
                           {u"Red Oranges", history::ClusterKeywordData()}},
                          /*should_show_on_prominent_ui_surfaces=*/true,
                          /*label=*/
                          l10n_util::GetStringFUTF16(
                              IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS,
                              base::UTF8ToUTF16(kSampleLabel)));
}

history::Cluster SampleCluster(int srp_visits,
                               int non_srp_visits,
                               const std::vector<std::string> related_searches =
                                   {"fruits", "red fruits", "healthy fruits"}) {
  return SampleCluster(1, srp_visits, non_srp_visits, related_searches);
}

TEST_F(HistoryClustersModuleServiceTest, GetClusters) {
  base::HistogramTester histogram_tester;

  const int kSampleClusterCount = 3;
  std::vector<history::Cluster> sample_clusters;
  for (int i = 0; i < kSampleClusterCount; i++) {
    sample_clusters.push_back(
        SampleCluster(i, /*srp_visits=*/1, /*non_srp_visits=*/2));
  }
  test_history_clusters_service().SetClustersToReturn(sample_clusters);

  std::vector<history::Cluster> clusters = GetClusters();
  ASSERT_EQ(3u, clusters.size());

  for (unsigned int i = 0; i < kSampleClusterCount; i++) {
    const auto& cluster = clusters[i];
    ASSERT_EQ(3u, cluster.visits.size());
    for (size_t u = 1; u < cluster.visits.size(); u++) {
      EXPECT_EQ(kSampleNonSearchUrl,
                base::UTF16ToASCII(cluster.visits[u].url_for_display));
    }
  }

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 0, 1);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", true, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 3, 1);

  histogram_tester.ExpectUniqueSample("NewTabPage.HistoryClusters.NumVisits", 3,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumRelatedSearches", 3, 1);
}

TEST_F(HistoryClustersModuleServiceTest, ClusterVisitsCulled) {
  base::HistogramTester histogram_tester;

  const history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/3, /*non_srp_visits=*/3);
  const std::vector<history::Cluster> kSampleClusters = {kSampleCluster};
  test_history_clusters_service().SetClustersToReturn(kSampleClusters);

  std::vector<history::Cluster> clusters = GetClusters();
  ASSERT_EQ(1u, clusters.size());

  auto& cluster = clusters[0];
  ASSERT_EQ(kSampleCluster.label.value(), cluster.label);
  ASSERT_EQ(4u, cluster.visits.size());
  ASSERT_EQ(kSampleSearchUrl,
            base::UTF16ToASCII(cluster.visits[0].url_for_display));
  for (size_t i = 1; i < cluster.visits.size(); i++) {
    ASSERT_EQ(kSampleNonSearchUrl,
              base::UTF16ToASCII(cluster.visits[i].url_for_display));
  }

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", true, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 1, 1);
  histogram_tester.ExpectUniqueSample("NewTabPage.HistoryClusters.NumVisits", 4,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumRelatedSearches", 3, 1);
}

TEST_F(HistoryClustersModuleServiceTest, IneligibleClusterNonProminent) {
  base::HistogramTester histogram_tester;

  history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/0, /*non_srp_visits=*/3);
  kSampleCluster.should_show_on_prominent_ui_surfaces = false;
  test_history_clusters_service().SetClustersToReturn({kSampleCluster});

  std::vector<history::Cluster> clusters = GetClusters();
  ASSERT_TRUE(clusters.empty());

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersModuleServiceTest, IneligibleClusterNoSRPVisit) {
  base::HistogramTester histogram_tester;

  const history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/0, /*non_srp_visits=*/3);
  test_history_clusters_service().SetClustersToReturn({kSampleCluster});

  std::vector<history::Cluster> clusters = GetClusters();
  ASSERT_TRUE(clusters.empty());

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersModuleServiceTest, IneligibleClusterInsufficientVisits) {
  base::HistogramTester histogram_tester;

  const history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/1, /*non_srp_visits=*/1);
  test_history_clusters_service().SetClustersToReturn({kSampleCluster});

  std::vector<history::Cluster> clusters = GetClusters();
  ASSERT_TRUE(clusters.empty());

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 4, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersModuleServiceTest, IneligibleClusterInsufficientImages) {
  base::HistogramTester histogram_tester;

  history::ClusterVisit sample_srp_visit =
      SampleVisitForURL(GURL(kSampleSearchUrl), false);
  history::ClusterVisit sample_non_srp_visit =
      SampleVisitForURL(GURL(kSampleNonSearchUrl), false);

  const history::Cluster kSampleCluster = history::Cluster(
      1, {sample_srp_visit, sample_non_srp_visit, sample_non_srp_visit},
      {{u"apples", history::ClusterKeywordData()},
       {u"Red Oranges", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/true,
      /*label=*/
      l10n_util::GetStringFUTF16(
          IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS, u"Red fruits"));
  test_history_clusters_service().SetClustersToReturn({kSampleCluster});

  std::vector<history::Cluster> clusters = GetClusters();
  ASSERT_TRUE(clusters.empty());

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersModuleServiceTest,
       IneligibleClusterInsufficientRelatedSearches) {
  base::HistogramTester histogram_tester;

  const history::Cluster kSampleCluster = SampleCluster(
      /*id=*/1, /*srp_visits=*/1, /*non_srp_visits=*/2,
      /*related_searches=*/{});
  test_history_clusters_service().SetClustersToReturn({kSampleCluster});

  std::vector<history::Cluster> clusters = GetClusters();
  ASSERT_TRUE(clusters.empty());

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 6, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersModuleServiceTest, NoClusters) {
  base::HistogramTester histogram_tester;

  std::vector<history::Cluster> clusters = GetClusters();
  ASSERT_TRUE(clusters.empty());

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

class HistoryClustersModuleServiceCartTest
    : public HistoryClustersModuleServiceTest {
 public:
  HistoryClustersModuleServiceCartTest() {
    features_.InitAndEnableFeature(ntp_features::kNtpChromeCartModule);
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(HistoryClustersModuleServiceCartTest, CheckClusterHasCart) {
  base::HistogramTester histogram_tester;
  std::string kSampleLabel = "LabelOne";
  const GURL url_A = GURL("https://www.foo.com");
  const GURL url_B = GURL("https://www.bar.com");
  const GURL url_C = GURL("https://www.baz.com");
  MockCartService& cart_service = mock_cart_service();

  const std::vector<std::string> visit_related_searches = {
      "fruits", "red fruits", "healthy fruits"};
  const history::Cluster cluster =
      history::Cluster(1,
                       {SampleVisitForURL(GURL(kSampleSearchUrl), false, {}),
                        SampleVisitForURL(url_A, true, visit_related_searches),
                        SampleVisitForURL(url_B, true, visit_related_searches),
                        SampleVisitForURL(url_C, true, visit_related_searches)},
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
  GetClusters();
  // Simulate one URL being identified as having a cart.
  std::move(callbacks[0]).Run(true);
  for (size_t i = 1; i < callbacks.size(); i++) {
    std::move(callbacks[i]).Run(false);
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
  GetClusters();
  // Simulate none URL being identified as having a cart.
  for (size_t i = 0; i < callbacks.size(); i++) {
    std::move(callbacks[i]).Run(false);
  }

  histogram_tester.ExpectBucketCount(
      "NewTabPage.HistoryClusters.HasCartForTopCluster", false, 1);
  histogram_tester.ExpectTotalCount(
      "NewTabPage.HistoryClusters.HasCartForTopCluster", 2);
}

}  // namespace
