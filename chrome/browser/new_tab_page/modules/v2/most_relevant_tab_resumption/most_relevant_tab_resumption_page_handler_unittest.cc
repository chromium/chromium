// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"

#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/search/ntp_features.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using visited_url_ranking::Fetcher;
using visited_url_ranking::FetchOptions;
using visited_url_ranking::ResultStatus;
using visited_url_ranking::URLVisit;
using visited_url_ranking::URLVisitAggregate;
using visited_url_ranking::VisitedURLRankingService;
using visited_url_ranking::VisitedURLRankingServiceFactory;

namespace {

class MostRelevantTabResumptionPageHandlerTest
    : public BrowserWithTestWindowTest {
 public:
  MostRelevantTabResumptionPageHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    InitializeHandler();
  }

  void InitializeHandler() {
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<MostRelevantTabResumptionPageHandler>(
        mojo::PendingReceiver<
            ntp::most_relevant_tab_resumption::mojom::PageHandler>(),
        web_contents_.get());
  }

  void ClearHandler() {
    handler_.reset();
    web_contents_.reset();
  }

  std::vector<history::mojom::TabPtr> RunGetTabs() {
    std::vector<history::mojom::TabPtr> tabs_mojom;
    base::RunLoop wait_loop;
    handler_->GetTabs(base::BindOnce(
        [](base::OnceClosure stop_waiting,
           std::vector<history::mojom::TabPtr>* tabs,
           std::vector<history::mojom::TabPtr> tabs_arg) {
          *tabs = std::move(tabs_arg);
          std::move(stop_waiting).Run();
        },
        wait_loop.QuitClosure(), &tabs_mojom));
    wait_loop.Run();
    return tabs_mojom;
  }

  void TearDown() override {
    ClearHandler();
    BrowserWithTestWindowTest::TearDown();
  }

  MostRelevantTabResumptionPageHandler* Handler() { return handler_.get(); }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {
        TestingProfile::TestingFactory{
            VisitedURLRankingServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  visited_url_ranking::MockVisitedURLRankingService>();
            })},
    };
  }

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MostRelevantTabResumptionPageHandler> handler_;
};

void ExpectURLTypesInFetchOptions(
    const FetchOptions& options,
    const std::set<FetchOptions::URLType>& expected_url_types) {
  std::set<FetchOptions::URLType> url_type_set;
  for (const auto& kv : options.result_sources) {
    url_type_set.insert(kv.first);
  }

  EXPECT_EQ(expected_url_types, url_type_set);
}

}  // namespace

using testing::_;

TEST_F(MostRelevantTabResumptionPageHandlerTest, GetFakeTabs) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpMostRelevantTabResumptionModule,
           {{ntp_features::kNtpMostRelevantTabResumptionModuleDataParam,
             "Fake Data"}}},
      },
      {});

  auto tabs_mojom = RunGetTabs();
  ASSERT_EQ(3u, tabs_mojom.size());
  for (const auto& tab_mojom : tabs_mojom) {
    ASSERT_EQ("Test Session", tab_mojom->session_name);
    ASSERT_EQ("5 mins ago", tab_mojom->relative_time_text);
    ASSERT_EQ(GURL("https://www.google.com"), tab_mojom->url);
  }
}

TEST_F(MostRelevantTabResumptionPageHandlerTest, GetTabs_TabURLTypesOnly) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpMostRelevantTabResumptionModule,
           {{ntp_features::kNtpMostRelevantTabResumptionModuleDataParam,
             base::StringPrintf(
                 "%d,%d",
                 static_cast<int>(FetchOptions::URLType::kActiveLocalTab),
                 static_cast<int>(FetchOptions::URLType::kActiveRemoteTab))}}},
      },
      {});
  ClearHandler();
  InitializeHandler();

  visited_url_ranking::MockVisitedURLRankingService*
      mock_visited_url_ranking_service =
          static_cast<visited_url_ranking::MockVisitedURLRankingService*>(
              VisitedURLRankingServiceFactory::GetForProfile(profile()));

  EXPECT_CALL(*mock_visited_url_ranking_service, FetchURLVisitAggregates(_, _))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](const FetchOptions& options,
             VisitedURLRankingService::GetURLVisitAggregatesCallback callback) {
            ExpectURLTypesInFetchOptions(
                options, {FetchOptions::URLType::kActiveLocalTab,
                          FetchOptions::URLType::kActiveRemoteTab});

            std::vector<URLVisitAggregate> url_visit_aggregates = {};
            url_visit_aggregates.emplace_back(
                visited_url_ranking::CreateSampleURLVisitAggregate(
                    GURL(visited_url_ranking::kSampleSearchUrl), 1.0f,
                    base::Time::Now(), {Fetcher::kSession}));
            url_visit_aggregates.emplace_back(
                visited_url_ranking::CreateSampleURLVisitAggregate(
                    GURL(visited_url_ranking::kSampleSearchUrl), 1.0f,
                    base::Time::Now(), {Fetcher::kHistory}));

            std::move(callback).Run(ResultStatus::kSuccess,
                                    std::move(url_visit_aggregates));
          }));

  EXPECT_CALL(*mock_visited_url_ranking_service,
              RankURLVisitAggregates(_, _, _))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](const visited_url_ranking::Config& config,
             std::vector<URLVisitAggregate> visits,
             VisitedURLRankingService::RankURLVisitAggregatesCallback
                 callback) {
            std::move(callback).Run(ResultStatus::kSuccess, std::move(visits));
          }));

  auto tabs_mojom = RunGetTabs();
  ASSERT_EQ(2u, tabs_mojom.size());
}

TEST_F(MostRelevantTabResumptionPageHandlerTest, GetTabs) {
  base::HistogramTester histogram_tester;
  visited_url_ranking::MockVisitedURLRankingService*
      mock_visited_url_ranking_service =
          static_cast<visited_url_ranking::MockVisitedURLRankingService*>(
              VisitedURLRankingServiceFactory::GetForProfile(profile()));

  EXPECT_CALL(*mock_visited_url_ranking_service, FetchURLVisitAggregates(_, _))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](const FetchOptions& options,
             VisitedURLRankingService::GetURLVisitAggregatesCallback callback) {
            ExpectURLTypesInFetchOptions(
                options, {FetchOptions::URLType::kActiveRemoteTab,
                          FetchOptions::URLType::kRemoteVisit});

            std::vector<URLVisitAggregate> url_visit_aggregates = {};
            url_visit_aggregates.emplace_back(
                visited_url_ranking::CreateSampleURLVisitAggregate(
                    GURL(visited_url_ranking::kSampleSearchUrl), 1.0f,
                    base::Time::Now(), {Fetcher::kSession}));
            url_visit_aggregates.emplace_back(
                visited_url_ranking::CreateSampleURLVisitAggregate(
                    GURL(visited_url_ranking::kSampleSearchUrl), 1.0f,
                    base::Time::Now(), {Fetcher::kHistory}));

            std::move(callback).Run(ResultStatus::kSuccess,
                                    std::move(url_visit_aggregates));
          }));

  EXPECT_CALL(*mock_visited_url_ranking_service,
              RankURLVisitAggregates(_, _, _))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](const visited_url_ranking::Config& config,
             std::vector<URLVisitAggregate> visits,
             VisitedURLRankingService::RankURLVisitAggregatesCallback
                 callback) {
            std::move(callback).Run(ResultStatus::kSuccess, std::move(visits));
          }));

  auto tabs_mojom = RunGetTabs();
  ASSERT_EQ(2u, tabs_mojom.size());
  for (const auto& tab_mojom : tabs_mojom) {
    ASSERT_EQ(history::mojom::DeviceType::kUnknown, tab_mojom->device_type);
    ASSERT_EQ("sample_title", tab_mojom->title);
    ASSERT_EQ(GURL(visited_url_ranking::kSampleSearchUrl), tab_mojom->url);
  }

  histogram_tester.ExpectBucketCount("NewTabPage.Modules.DataRequest",
                                     base::PersistentHash("tab_resumption"), 1);
}

TEST_F(MostRelevantTabResumptionPageHandlerTest, DismissAndRestoreTab) {
  visited_url_ranking::MockVisitedURLRankingService*
      mock_visited_url_ranking_service =
          static_cast<visited_url_ranking::MockVisitedURLRankingService*>(
              VisitedURLRankingServiceFactory::GetForProfile(profile()));

  EXPECT_CALL(*mock_visited_url_ranking_service, FetchURLVisitAggregates(_, _))
      .Times(3)
      .WillRepeatedly(testing::Invoke(
          [](const FetchOptions& options,
             VisitedURLRankingService::GetURLVisitAggregatesCallback callback) {
            std::vector<URLVisitAggregate> url_visit_aggregates = {};
            url_visit_aggregates.emplace_back(
                visited_url_ranking::CreateSampleURLVisitAggregate(
                    GURL(visited_url_ranking::kSampleSearchUrl), 1.0f,
                    base::Time::FromDeltaSinceWindowsEpoch(
                        base::Microseconds(12345)),
                    {Fetcher::kSession}));
            url_visit_aggregates.emplace_back(
                visited_url_ranking::CreateSampleURLVisitAggregate(
                    GURL(visited_url_ranking::kSampleSearchUrl), 1.0f,
                    base::Time::Now(), {Fetcher::kHistory}));

            std::move(callback).Run(ResultStatus::kSuccess,
                                    std::move(url_visit_aggregates));
          }));

  EXPECT_CALL(*mock_visited_url_ranking_service,
              RankURLVisitAggregates(_, _, _))
      .Times(3)
      .WillRepeatedly(testing::Invoke(
          [](const visited_url_ranking::Config& config,
             std::vector<URLVisitAggregate> visits,
             VisitedURLRankingService::RankURLVisitAggregatesCallback
                 callback) {
            std::move(callback).Run(ResultStatus::kSuccess, std::move(visits));
          }));

  visited_url_ranking::ScoredURLUserAction expected_action;
  EXPECT_CALL(*mock_visited_url_ranking_service, RecordAction(_, _, _))
      .Times(2)
      .WillRepeatedly(testing::Invoke(
          [&expected_action](
              visited_url_ranking::ScoredURLUserAction action,
              const std::string& visit_id,
              segmentation_platform::TrainingRequestId visit_request_id) {
            expected_action = action;
          }));

  auto tabs_mojom = RunGetTabs();
  ASSERT_EQ(2u, tabs_mojom.size());
  Handler()->DismissTab(mojo::Clone(tabs_mojom[0]));
  ASSERT_EQ(visited_url_ranking::ScoredURLUserAction::kDismissed,
            expected_action);
  auto dismissed_tabs_mojom = RunGetTabs();
  ASSERT_EQ(1u, dismissed_tabs_mojom.size());
  Handler()->RestoreTab(mojo::Clone(tabs_mojom[0]));
  ASSERT_EQ(visited_url_ranking::ScoredURLUserAction::kSeen, expected_action);
  auto restored_tabs_mojom = RunGetTabs();
  ASSERT_EQ(2u, restored_tabs_mojom.size());
}

TEST_F(MostRelevantTabResumptionPageHandlerTest, DismissAndRestoreAll) {
  visited_url_ranking::MockVisitedURLRankingService*
      mock_visited_url_ranking_service =
          static_cast<visited_url_ranking::MockVisitedURLRankingService*>(
              VisitedURLRankingServiceFactory::GetForProfile(profile()));

  EXPECT_CALL(*mock_visited_url_ranking_service, FetchURLVisitAggregates(_, _))
      .Times(3)
      .WillRepeatedly(testing::Invoke(
          [](const FetchOptions& options,
             VisitedURLRankingService::GetURLVisitAggregatesCallback callback) {
            std::vector<URLVisitAggregate> url_visit_aggregates = {};
            url_visit_aggregates.emplace_back(
                visited_url_ranking::CreateSampleURLVisitAggregate(
                    GURL(visited_url_ranking::kSampleSearchUrl), 1.0f,
                    base::Time::FromDeltaSinceWindowsEpoch(
                        base::Microseconds(12345)),
                    {Fetcher::kSession}));
            url_visit_aggregates.emplace_back(
                visited_url_ranking::CreateSampleURLVisitAggregate(
                    GURL(visited_url_ranking::kSampleSearchUrl), 1.0f,
                    base::Time::FromDeltaSinceWindowsEpoch(
                        base::Microseconds(123456)),
                    {Fetcher::kHistory}));

            std::move(callback).Run(ResultStatus::kSuccess,
                                    std::move(url_visit_aggregates));
          }));

  EXPECT_CALL(*mock_visited_url_ranking_service,
              RankURLVisitAggregates(_, _, _))
      .Times(3)
      .WillRepeatedly(testing::Invoke(
          [](const visited_url_ranking::Config& config,
             std::vector<URLVisitAggregate> visits,
             VisitedURLRankingService::RankURLVisitAggregatesCallback
                 callback) {
            std::move(callback).Run(ResultStatus::kSuccess, std::move(visits));
          }));

  std::vector<visited_url_ranking::ScoredURLUserAction> expected_actions;
  EXPECT_CALL(*mock_visited_url_ranking_service, RecordAction(_, _, _))
      .Times(4)
      .WillRepeatedly(testing::Invoke(
          [&expected_actions](
              visited_url_ranking::ScoredURLUserAction action,
              const std::string& visit_id,
              segmentation_platform::TrainingRequestId visit_request_id) {
            expected_actions.push_back(action);
          }));

  auto tabs_mojom = RunGetTabs();
  ASSERT_EQ(2u, tabs_mojom.size());
  Handler()->DismissModule(mojo::Clone(tabs_mojom));
  ASSERT_EQ(visited_url_ranking::ScoredURLUserAction::kDismissed,
            expected_actions[0]);
  ASSERT_EQ(visited_url_ranking::ScoredURLUserAction::kDismissed,
            expected_actions[1]);
  auto dismissed_tabs_mojom = RunGetTabs();
  ASSERT_EQ(0u, dismissed_tabs_mojom.size());
  Handler()->RestoreModule(mojo::Clone(tabs_mojom));
  ASSERT_EQ(visited_url_ranking::ScoredURLUserAction::kSeen,
            expected_actions[2]);
  ASSERT_EQ(visited_url_ranking::ScoredURLUserAction::kSeen,
            expected_actions[3]);
  auto restored_tabs_mojom = RunGetTabs();
  ASSERT_EQ(2u, restored_tabs_mojom.size());
}
