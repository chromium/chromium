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
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/ui/side_panel/history_clusters/history_clusters_tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/test_history_clusters_service.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      &HistoryClustersServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* browser_context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<history_clusters::TestHistoryClustersService>();
      }));
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
        web_contents_(factory_.CreateWebContents(profile_.get())),
        mock_history_clusters_tab_helper_(
            MockHistoryClustersTabHelper::CreateForWebContents(
                web_contents_.get())),
        handler_(std::make_unique<HistoryClustersPageHandler>(
            mojo::PendingReceiver<ntp::history_clusters::mojom::PageHandler>(),
            web_contents_)) {}

  history_clusters::TestHistoryClustersService&
  test_history_clusters_service() {
    return *test_history_clusters_service_;
  }

  MockHistoryClustersTabHelper& mock_history_clusters_tab_helper() {
    return *mock_history_clusters_tab_helper_;
  }

  HistoryClustersPageHandler& handler() { return *handler_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  raw_ptr<history_clusters::TestHistoryClustersService>
      test_history_clusters_service_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
  raw_ptr<MockHistoryClustersTabHelper> mock_history_clusters_tab_helper_;
  std::unique_ptr<HistoryClustersPageHandler> handler_;
};

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

TEST_F(HistoryClustersPageHandlerTest, ShowJourneysSidePanel) {
  std::string kSampleQuery = "safest cars";
  std::string query;
  EXPECT_CALL(mock_history_clusters_tab_helper(), ShowJourneysSidePanel)
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&query)));

  handler().ShowJourneysSidePanel(kSampleQuery);

  EXPECT_EQ(kSampleQuery, query);
}
