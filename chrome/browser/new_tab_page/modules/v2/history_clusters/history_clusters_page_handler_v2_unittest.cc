// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/history_clusters/history_clusters_page_handler_v2.h"

#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_test_support.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/test_history_clusters_service.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class MockCartService : public CartService {
 public:
  explicit MockCartService(Profile* profile) : CartService(profile) {}

  MOCK_METHOD1(LoadAllActiveCarts, void(CartDB::LoadCallback callback));
};

class HistoryClustersPageHandlerV2Test : public BrowserWithTestWindowTest {
 public:
  HistoryClustersPageHandlerV2Test() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    mock_history_clusters_module_service_ =
        static_cast<MockHistoryClustersModuleService*>(
            HistoryClustersModuleServiceFactory::GetForProfile(profile()));
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    mock_history_clusters_tab_helper_ =
        MockHistoryClustersTabHelper::CreateForWebContents(web_contents_.get());
    mock_history_service_ =
        static_cast<MockHistoryService*>(HistoryServiceFactory::GetForProfile(
            profile(), ServiceAccessType::EXPLICIT_ACCESS));
    mock_cart_service_ = static_cast<MockCartService*>(
        CartServiceFactory::GetForProfile(profile()));
    handler_ = std::make_unique<HistoryClustersPageHandlerV2>(
        mojo::PendingReceiver<ntp::history_clusters_v2::mojom::PageHandler>(),
        web_contents_.get());
    ukm_source_id_ = web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId();
  }

  void TearDown() override {
    handler_.reset();
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  MockHistoryClustersModuleService& mock_history_clusters_module_service() {
    return *mock_history_clusters_module_service_;
  }

  MockHistoryClustersTabHelper& mock_history_clusters_tab_helper() {
    return *mock_history_clusters_tab_helper_;
  }

  MockHistoryService& mock_history_service() { return *mock_history_service_; }

  MockCartService& mock_cart_service() { return *mock_cart_service_; }

  HistoryClustersPageHandlerV2& handler() { return *handler_; }

  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }

  void ResetHandler() { handler_.reset(); }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{HistoryClustersModuleServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<MockHistoryClustersModuleService>();
             })},
            {HistoryServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<MockHistoryService>();
             })},
            {TemplateURLServiceFactory::GetInstance(),
             base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)},
            {CartServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<MockCartService>(
                   Profile::FromBrowserContext(context));
             })}};
  }

  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<MockHistoryClustersModuleService, DanglingUntriaged>
      mock_history_clusters_module_service_;
  raw_ptr<MockHistoryClustersTabHelper, DanglingUntriaged>
      mock_history_clusters_tab_helper_;
  raw_ptr<MockHistoryService, DanglingUntriaged> mock_history_service_;
  raw_ptr<MockCartService, DanglingUntriaged> mock_cart_service_;
  std::unique_ptr<HistoryClustersPageHandlerV2> handler_;
  ukm::SourceId ukm_source_id_;
};

TEST_F(HistoryClustersPageHandlerV2Test, GetClusters) {
  const int kSampleClusterCount = 3;
  std::vector<history::Cluster> sample_clusters;
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals;
  for (int i = 0; i < kSampleClusterCount; i++) {
    sample_clusters.push_back(
        SampleCluster(i, /*srp_visits=*/1, /*non_srp_visits=*/2));
    ranking_signals[i] = HistoryClustersModuleRankingSignals();
  }
  history_clusters::QueryClustersFilterParams filter_params;
  EXPECT_CALL(mock_history_clusters_module_service(),
              GetClusters(testing::_, testing::_, testing::_))
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&filter_params),
          testing::Invoke(
              [&sample_clusters, &ranking_signals](
                  const history_clusters::QueryClustersFilterParams
                      filter_params,
                  size_t min_required_related_searches,
                  base::OnceCallback<void(
                      std::vector<history::Cluster>,
                      base::flat_map<int64_t,
                                     HistoryClustersModuleRankingSignals>)>
                      callback) {
                std::move(callback).Run(sample_clusters, ranking_signals);
              })));

  std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
  base::MockCallback<HistoryClustersPageHandlerV2::GetClustersCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&clusters_mojom](
              std::vector<history_clusters::mojom::ClusterPtr> clusters_arg) {
            clusters_mojom = std::move(clusters_arg);
          }));
  handler().GetClusters(callback.Get());

  EXPECT_EQ(filter_params.min_visits_with_images, 1);
  ASSERT_EQ(3u, clusters_mojom.size());

  for (unsigned int i = 0; i < kSampleClusterCount; i++) {
    const auto& cluster_mojom = clusters_mojom[i];
    ASSERT_TRUE(cluster_mojom);
    ASSERT_EQ(i, cluster_mojom->id);
    ASSERT_EQ(3u, cluster_mojom->visits.size());
    for (size_t u = 1; u < cluster_mojom->visits.size(); u++) {
      ASSERT_EQ(kSampleNonSearchUrl, cluster_mojom->visits[u]->url_for_display);
    }

    // Just report them all shown so UKM can get recorded.
    handler().RecordLayoutTypeShown(
        ntp::history_clusters::mojom::LayoutType::kTextOnly, cluster_mojom->id);
  }

  // Reset handler to make sure UKM is recorded.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ResetHandler();
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::NewTabPage_HistoryClusters::kEntryName);
  ASSERT_EQ(entries.size(), 3u);
}

TEST_F(HistoryClustersPageHandlerV2Test, GetClustersTextOnlyLayout) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpHistoryClustersModuleTextOnly},
      /*disabled_features=*/{});

  history_clusters::QueryClustersFilterParams filter_params;
  EXPECT_CALL(mock_history_clusters_module_service(),
              GetClusters(testing::_, testing::_, testing::_))
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&filter_params),
          testing::Invoke(
              [](const history_clusters::QueryClustersFilterParams
                     filter_params,
                 size_t min_required_related_searches,
                 base::OnceCallback<void(
                     std::vector<history::Cluster>,
                     base::flat_map<int64_t,
                                    HistoryClustersModuleRankingSignals>)>
                     callback) {
                std::vector<history::Cluster> sample_clusters;
                base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
                    ranking_signals;
                std::move(callback).Run(sample_clusters, ranking_signals);
              })));

  base::MockCallback<HistoryClustersPageHandlerV2::GetClustersCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_)).Times(1);
  handler().GetClusters(callback.Get());

  EXPECT_EQ(filter_params.min_visits_with_images, 0);
}

TEST_F(HistoryClustersPageHandlerV2Test, GetFakeClusters) {
  const unsigned long kNumClusters = 3;
  const unsigned long kNumVisits = 2;
  const unsigned long kNumVisitsWithImages = 2;
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpHistoryClustersModule,
           {{ntp_features::kNtpHistoryClustersModuleDataParam,
             base::StringPrintf("%lu,%lu,%lu", kNumClusters, kNumVisits,
                                kNumVisitsWithImages)}}},
      },
      {});

  std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
  base::MockCallback<HistoryClustersPageHandlerV2::GetClustersCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&clusters_mojom](
              std::vector<history_clusters::mojom::ClusterPtr> clusters_arg) {
            clusters_mojom = std::move(clusters_arg);
          }));
  handler().GetClusters(callback.Get());
  ASSERT_EQ(kNumClusters, clusters_mojom.size());
  ASSERT_EQ(0u, clusters_mojom[0]->id);
  // The cluster visits should include an additional entry for the SRP visit.
  ASSERT_EQ(kNumVisits + 1, clusters_mojom[0]->visits.size());
}

TEST_F(HistoryClustersPageHandlerV2Test,
       NoClusterReturnedForInvalidModuleDataParam) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpHistoryClustersModule,
           {{ntp_features::kNtpHistoryClustersModuleDataParam, "0"}}},
      },
      {});

  const history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/1, /*non_srp_visits=*/2);
  EXPECT_CALL(mock_history_clusters_module_service(),
              GetClusters(testing::_, testing::_, testing::_))
      .Times(0);

  std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
  base::MockCallback<HistoryClustersPageHandlerV2::GetClustersCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&clusters_mojom](
              std::vector<history_clusters::mojom::ClusterPtr> clusters_arg) {
            clusters_mojom = std::move(clusters_arg);
          }));
  handler().GetClusters(callback.Get());
  ASSERT_EQ(0u, clusters_mojom.size());
}

TEST_F(HistoryClustersPageHandlerV2Test, NoClusters) {
  EXPECT_CALL(mock_history_clusters_module_service(),
              GetClusters(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&](const history_clusters::QueryClustersFilterParams filter_params,
              size_t min_required_related_searches,
              base::OnceCallback<void(
                  std::vector<history::Cluster>,
                  base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>
                  callback) { std::move(callback).Run({}, {}); }));

  std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
  base::MockCallback<HistoryClustersPageHandlerV2::GetClustersCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&clusters_mojom](
              std::vector<history_clusters::mojom::ClusterPtr> clusters_arg) {
            clusters_mojom = std::move(clusters_arg);
          }));
  handler().GetClusters(callback.Get());
  ASSERT_EQ(0u, clusters_mojom.size());
}

TEST_F(HistoryClustersPageHandlerV2Test, ShowJourneysSidePanel) {
  std::string kSampleQuery = "safest cars";
  std::string query;
  EXPECT_CALL(mock_history_clusters_tab_helper(), ShowJourneysSidePanel)
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&query)));

  handler().ShowJourneysSidePanel(kSampleQuery);

  EXPECT_EQ(kSampleQuery, query);
}

TEST_F(HistoryClustersPageHandlerV2Test, RecordClick) {
  // Send down some clusters so we have a logger.
  int64_t cluster_id = 123;
  std::vector<history::Cluster> sample_clusters = {SampleCluster(0, 1, 2)};
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals =
      {{cluster_id, HistoryClustersModuleRankingSignals()}};
  EXPECT_CALL(mock_history_clusters_module_service(),
              GetClusters(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&sample_clusters, &ranking_signals](
              const history_clusters::QueryClustersFilterParams filter_params,
              size_t min_required_related_searches,
              base::OnceCallback<void(
                  std::vector<history::Cluster>,
                  base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>
                  callback) {
            std::move(callback).Run(sample_clusters, ranking_signals);
          }));
  base::MockCallback<HistoryClustersPageHandlerV2::GetClustersCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_));
  handler().GetClusters(callback.Get());

  // Simulate a click and layout type.
  handler().RecordLayoutTypeShown(
      ntp::history_clusters::mojom::LayoutType::kImages, cluster_id);
  handler().RecordClick(cluster_id);

  // Reset handler to make sure UKM is recorded.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ResetHandler();
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::NewTabPage_HistoryClusters::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::NewTabPage_HistoryClusters::kLayoutTypeShownName, 5);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::NewTabPage_HistoryClusters::kDidEngageWithModuleName, 1);
}

TEST_F(HistoryClustersPageHandlerV2Test, RecordLayoutTypeShown) {
  // Send down some clusters so we have a logger.
  int64_t cluster_id = 123;
  std::vector<history::Cluster> sample_clusters = {SampleCluster(0, 1, 2)};
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals =
      {{cluster_id, HistoryClustersModuleRankingSignals()}};
  EXPECT_CALL(mock_history_clusters_module_service(),
              GetClusters(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&sample_clusters, &ranking_signals](
              const history_clusters::QueryClustersFilterParams filter_params,
              size_t min_required_related_searches,
              base::OnceCallback<void(
                  std::vector<history::Cluster>,
                  base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>
                  callback) {
            std::move(callback).Run(sample_clusters, ranking_signals);
          }));
  base::MockCallback<HistoryClustersPageHandlerV2::GetClustersCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_));
  handler().GetClusters(callback.Get());

  // Simulate a layout type being chosen for the cluster.
  ntp::history_clusters::mojom::LayoutType layout_type =
      ntp::history_clusters::mojom::LayoutType::kImages;
  handler().RecordLayoutTypeShown(layout_type, cluster_id);

  // Reset handler to make sure UKM is recorded.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ResetHandler();
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::NewTabPage_HistoryClusters::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::NewTabPage_HistoryClusters::kLayoutTypeShownName, 5);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::NewTabPage_HistoryClusters::kDidEngageWithModuleName, 0);
}

TEST_F(HistoryClustersPageHandlerV2Test, UpdateClusterVisitsInteractionState) {
  std::vector<history::VisitID> visit_ids;
  history::ClusterVisit::InteractionState state;
  EXPECT_CALL(mock_history_service(), UpdateVisitsInteractionState)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&visit_ids, &state](
              const std::vector<history::VisitID>& visit_ids_arg,
              const history::ClusterVisit::InteractionState state_arg,
              base::OnceClosure callback_arg,
              base::CancelableTaskTracker* tracker_arg)
              -> base::CancelableTaskTracker::TaskId {
            visit_ids = visit_ids_arg;
            state = state_arg;
            return 0;
          }));

  const int kSampleClusterVisitCount = 3;
  std::vector<history_clusters::mojom::URLVisitPtr> sample_cluster_visits;
  for (int i = 0; i < kSampleClusterVisitCount; i++) {
    auto visit_mojom = history_clusters::mojom::URLVisit::New();
    visit_mojom->visit_id = i;
    sample_cluster_visits.push_back(std::move(visit_mojom));
  }

  handler().UpdateClusterVisitsInteractionState(
      std::move(sample_cluster_visits),
      history_clusters::mojom::InteractionState::kDone);
  ASSERT_EQ(static_cast<size_t>(kSampleClusterVisitCount), visit_ids.size());
  for (size_t i = 0; i < visit_ids.size(); i++) {
    ASSERT_EQ(static_cast<history::VisitID>(i), visit_ids[i]);
  }
}

TEST_F(HistoryClustersPageHandlerV2Test, NotLoadCartWithoutFeature) {
  history_clusters::mojom::ClusterPtr cluster_mojom;
  EXPECT_CALL(mock_cart_service(), LoadAllActiveCarts(testing::_)).Times(0);
  handler().GetCartForCluster(std::move(cluster_mojom), base::DoNothing());
}

class HistoryClustersPageHandlerV2CartInQuestTest
    : public HistoryClustersPageHandlerV2Test {
 public:
  HistoryClustersPageHandlerV2CartInQuestTest() {
    features_.InitAndEnableFeature(
        ntp_features::kNtpChromeCartInHistoryClusterModule);
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(HistoryClustersPageHandlerV2CartInQuestTest, LoadCartWithFeature) {
  history_clusters::mojom::ClusterPtr cluster_mojom;
  EXPECT_CALL(mock_cart_service(), LoadAllActiveCarts(testing::_)).Times(1);
  handler().GetCartForCluster(std::move(cluster_mojom), base::DoNothing());
}

}  // namespace
