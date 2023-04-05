// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_page_handler.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/side_panel/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/test_history_clusters_service.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#include <vector>

namespace {

class MockHistoryClustersTabHelper
    : public side_panel::HistoryClustersTabHelper {
 public:
  static MockHistoryClustersTabHelper* CreateForWebContents(
      content::WebContents* contents) {
    DCHECK(contents);
    DCHECK(!contents->GetUserData(UserDataKey()));
    contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new MockHistoryClustersTabHelper(contents)));
    return static_cast<MockHistoryClustersTabHelper*>(
        contents->GetUserData(UserDataKey()));
  }

  MOCK_METHOD(void, ShowJourneysSidePanel, (const std::string&), (override));

 private:
  explicit MockHistoryClustersTabHelper(content::WebContents* web_contents)
      : HistoryClustersTabHelper(web_contents) {}
};

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() : HistoryService() {}

  MOCK_METHOD1(ClearCachedDataForContextID,
               void(history::ContextID context_id));
  MOCK_METHOD3(HideVisits,
               base::CancelableTaskTracker::TaskId(
                   const std::vector<history::VisitID>& visit_ids,
                   base::OnceClosure callback,
                   base::CancelableTaskTracker* tracker));
};

class MockCartService : public CartService {
 public:
  explicit MockCartService(Profile* profile) : CartService(profile) {}

  MOCK_METHOD2(HasActiveCartForURL,
               void(const GURL& url, base::OnceCallback<void(bool)> callback));
};

constexpr char kSampleNonSearchUrl[] = "https://www.foo.com/";
constexpr char kSampleSearchUrl[] = "https://www.google.com/search?q=foo";

}  // namespace

class HistoryClustersPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  HistoryClustersPageHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    test_history_clusters_service_ =
        static_cast<history_clusters::TestHistoryClustersService*>(
            HistoryClustersServiceFactory::GetForBrowserContext(profile()));
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    mock_history_clusters_tab_helper_ =
        MockHistoryClustersTabHelper::CreateForWebContents(web_contents_.get());
    mock_history_service_ =
        static_cast<MockHistoryService*>(HistoryServiceFactory::GetForProfile(
            profile(), ServiceAccessType::EXPLICIT_ACCESS));
    mock_cart_service_ = static_cast<MockCartService*>(
        CartServiceFactory::GetForProfile(profile()));
    handler_ = std::make_unique<HistoryClustersPageHandler>(
        mojo::PendingReceiver<ntp::history_clusters::mojom::PageHandler>(),
        web_contents_.get());
  }

  void TearDown() override {
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  history_clusters::TestHistoryClustersService&
  test_history_clusters_service() {
    return *test_history_clusters_service_;
  }

  MockHistoryClustersTabHelper& mock_history_clusters_tab_helper() {
    return *mock_history_clusters_tab_helper_;
  }

  MockHistoryService& mock_history_service() { return *mock_history_service_; }

  MockCartService& mock_cart_service() { return *mock_cart_service_; }

  HistoryClustersPageHandler& handler() { return *handler_; }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{&HistoryClustersServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<
                   history_clusters::TestHistoryClustersService>();
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

  raw_ptr<history_clusters::TestHistoryClustersService>
      test_history_clusters_service_;
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<MockHistoryClustersTabHelper> mock_history_clusters_tab_helper_;
  raw_ptr<MockHistoryService> mock_history_service_;
  raw_ptr<MockCartService> mock_cart_service_;
  std::unique_ptr<HistoryClustersPageHandler> handler_;
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

history::Cluster SampleCluster(int srp_visits,
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
  return history::Cluster(1, std::move(visits),
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

  const history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/1, /*non_srp_visits=*/2);
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
  ASSERT_EQ(base::UTF16ToUTF8(kSampleCluster.label.value()),
            cluster_mojom->label);
  ASSERT_EQ(3u, cluster_mojom->visits.size());
  ASSERT_EQ(base::UTF16ToUTF8(kSampleCluster.visits[0].url_for_display),
            cluster_mojom->visits[0]->url_for_display);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", true, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 1, 1);
  histogram_tester.ExpectUniqueSample("NewTabPage.HistoryClusters.NumVisits", 3,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumRelatedSearches", 3, 1);
}

TEST_F(HistoryClustersPageHandlerTest, ClusterVisitsCulled) {
  base::HistogramTester histogram_tester;

  const history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/3, /*non_srp_visits=*/3);
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
  ASSERT_EQ(base::UTF16ToUTF8(kSampleCluster.label.value()),
            cluster_mojom->label);
  ASSERT_EQ(4u, cluster_mojom->visits.size());
  ASSERT_EQ(kSampleSearchUrl, cluster_mojom->visits[0]->url_for_display);
  for (size_t i = 1; i < cluster_mojom->visits.size(); i++) {
    ASSERT_EQ(kSampleNonSearchUrl, cluster_mojom->visits[i]->url_for_display);
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

TEST_F(HistoryClustersPageHandlerTest, IneligibleClusterNonProminent) {
  base::HistogramTester histogram_tester;

  const history::Cluster kSampleCluster = history::Cluster(
      1, {},
      {{u"apples", history::ClusterKeywordData()},
       {u"Red Oranges", history::ClusterKeywordData()}},
      /*should_show_on_prominent_ui_surfaces=*/false,
      /*label=*/
      l10n_util::GetStringFUTF16(
          IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS, u"Red fruits"));
  test_history_clusters_service().SetClustersToReturn({kSampleCluster});

  history_clusters::mojom::ClusterPtr cluster_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClusterCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cluster_mojom](history_clusters::mojom::ClusterPtr cluster_arg) {
            cluster_mojom = std::move(cluster_arg);
          }));
  handler().GetCluster(callback.Get());
  ASSERT_FALSE(cluster_mojom);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersPageHandlerTest, IneligibleClusterNoSRPVisit) {
  base::HistogramTester histogram_tester;

  const history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/0, /*non_srp_visits=*/3);
  test_history_clusters_service().SetClustersToReturn({kSampleCluster});

  history_clusters::mojom::ClusterPtr cluster_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClusterCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cluster_mojom](history_clusters::mojom::ClusterPtr cluster_arg) {
            cluster_mojom = std::move(cluster_arg);
          }));
  handler().GetCluster(callback.Get());
  ASSERT_FALSE(cluster_mojom);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersPageHandlerTest, IneligibleClusterInsufficientVisits) {
  base::HistogramTester histogram_tester;

  const history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/1, /*non_srp_visits=*/1);
  test_history_clusters_service().SetClustersToReturn({kSampleCluster});

  history_clusters::mojom::ClusterPtr cluster_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClusterCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cluster_mojom](history_clusters::mojom::ClusterPtr cluster_arg) {
            cluster_mojom = std::move(cluster_arg);
          }));
  handler().GetCluster(callback.Get());
  ASSERT_FALSE(cluster_mojom);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 4, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersPageHandlerTest, IneligibleClusterInsufficientImages) {
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

  history_clusters::mojom::ClusterPtr cluster_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClusterCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cluster_mojom](history_clusters::mojom::ClusterPtr cluster_arg) {
            cluster_mojom = std::move(cluster_arg);
          }));
  handler().GetCluster(callback.Get());
  ASSERT_FALSE(cluster_mojom);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersPageHandlerTest,
       IneligibleClusterInsufficientRelatedSearches) {
  base::HistogramTester histogram_tester;

  const history::Cluster kSampleCluster = SampleCluster(
      /*srp_visits=*/1, /*non_srp_visits=*/2, /*related_searches=*/{});
  test_history_clusters_service().SetClustersToReturn({kSampleCluster});

  history_clusters::mojom::ClusterPtr cluster_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClusterCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cluster_mojom](history_clusters::mojom::ClusterPtr cluster_arg) {
            cluster_mojom = std::move(cluster_arg);
          }));
  handler().GetCluster(callback.Get());
  ASSERT_FALSE(cluster_mojom);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 6, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersPageHandlerTest, GetFakeCluster) {
  const unsigned long kNumVisits = 2;
  const unsigned long kNumVisitsWithImages = 2;
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpHistoryClustersModule,
           {{ntp_features::kNtpHistoryClustersModuleDataParam,
             base::StringPrintf("%lu,%lu", kNumVisits, kNumVisitsWithImages)}}},
      },
      {});

  test_history_clusters_service().SetClustersToReturn({});
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
  ASSERT_EQ(0u, cluster_mojom->id);
  // The cluster visits should include an additional entry for the SRP visit.
  ASSERT_EQ(kNumVisits + 1, cluster_mojom->visits.size());
}

TEST_F(HistoryClustersPageHandlerTest, MultipleClusters) {
  base::HistogramTester histogram_tester;

  const history::Cluster kSampleCluster =
      SampleCluster(/*srp_visits=*/1, /*non_srp_visits=*/2);
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
  ASSERT_EQ(base::UTF16ToUTF8(kSampleCluster.label.value()),
            cluster_mojom->label);
  ASSERT_EQ(3u, cluster_mojom->visits.size());
  ASSERT_EQ(base::UTF16ToUTF8(kSampleCluster.visits[0].url_for_display),
            cluster_mojom->visits[0]->url_for_display);

  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.IneligibleReason", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", true, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 2, 1);
  histogram_tester.ExpectUniqueSample("NewTabPage.HistoryClusters.NumVisits", 3,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumRelatedSearches", 3, 1);
}

TEST_F(HistoryClustersPageHandlerTest,
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
  test_history_clusters_service().SetClustersToReturn({kSampleCluster});
  history_clusters::mojom::ClusterPtr cluster_mojom;
  base::MockCallback<HistoryClustersPageHandler::GetClusterCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&cluster_mojom](history_clusters::mojom::ClusterPtr cluster_arg) {
            cluster_mojom = std::move(cluster_arg);
          }));
  handler().GetCluster(callback.Get());
  ASSERT_FALSE(cluster_mojom);
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
      "NewTabPage.HistoryClusters.IneligibleReason", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.HasClusterToShow", false, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.HistoryClusters.NumClusterCandidates", 0, 1);
}

TEST_F(HistoryClustersPageHandlerTest, ShowJourneysSidePanel) {
  std::string kSampleQuery = "safest cars";
  std::string query;
  EXPECT_CALL(mock_history_clusters_tab_helper(), ShowJourneysSidePanel)
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&query)));

  handler().ShowJourneysSidePanel(kSampleQuery);

  EXPECT_EQ(kSampleQuery, query);
}

TEST_F(HistoryClustersPageHandlerTest, OpenUrlsInTabGroup) {
  const std::vector<GURL> urls = {GURL("http://www.google.com/search?q=foo"),
                                  GURL("http://foo/1"), GURL("http://foo/2")};
  handler().OpenUrlsInTabGroup(urls);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(urls.size(), static_cast<size_t>(tab_strip_model->GetTabCount()));
  for (size_t i = 0; i < urls.size(); i++) {
    ASSERT_EQ(urls[i], tab_strip_model->GetWebContentsAt(i)->GetURL());
  }

  TabGroupModel* tab_group_model = tab_strip_model->group_model();
  ASSERT_EQ(1u, tab_group_model->ListTabGroups().size());
  ASSERT_EQ(1, tab_strip_model->GetIndexOfWebContents(
                   tab_strip_model->GetActiveWebContents()));
}

TEST_F(HistoryClustersPageHandlerTest, DismissCluster) {
  std::vector<history::VisitID> visit_ids;
  EXPECT_CALL(mock_history_service(), HideVisits)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&visit_ids](const std::vector<history::VisitID>& visit_ids_arg,
                       base::OnceClosure callback_arg,
                       base::CancelableTaskTracker* tracker_arg)
              -> base::CancelableTaskTracker::TaskId {
            visit_ids = visit_ids_arg;
            return 0;
          }));

  auto visit_mojom = history_clusters::mojom::URLVisit::New();
  visit_mojom->visit_id = 1;
  std::vector<history_clusters::mojom::URLVisitPtr> visits_mojom;
  visits_mojom.push_back(std::move(visit_mojom));

  handler().DismissCluster(std::move(visits_mojom));
  ASSERT_EQ(1u, visit_ids.size());
  ASSERT_EQ(1u, visit_ids.front());
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
