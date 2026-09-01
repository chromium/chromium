// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"

#include <jni.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/check.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::InvokeFuture;
using ::base::test::RunOnceCallback;
using ::page_content_annotations::HistoryVisit;
using ::page_content_annotations::PageContentAnnotationsResult;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::WithArg;
using ::visited_url_ranking::ResultStatus;
using ::visited_url_ranking::URLVisitsMetadata;

page_content_annotations::PageContentAnnotationsResult
CreateAnnotationsResult() {
  return page_content_annotations::PageContentAnnotationsResult::
      CreateContentVisibilityScoreResult(1.0);
}

HistoryVisit CreateLocalVisit() {
  HistoryVisit visit;
  visit.navigation_id = 1;
  return visit;
}

std::vector<visited_url_ranking::URLVisitAggregate> CreateVisitAggregates() {
  return {};
}

ACTION_P(SaveBeginTime, pointer) {
  *pointer = arg0.begin_time;
}

class AuxiliarySearchDonationServiceTest : public testing::Test {
 public:
  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
    CHECK(history_service_);

    page_content_annotations_service_ =
        page_content_annotations::TestPageContentAnnotationsService::Create(
            /*optimization_guide_model_provider=*/nullptr,
            history_service_.get());
    CHECK(page_content_annotations_service_);

    AuxiliarySearchDonationService::RegisterProfilePrefs(
        test_pref_service_.registry());

    ON_CALL(mock_ranking_service_, RankURLVisitAggregates)
        .WillByDefault(
            [](const visited_url_ranking::Config& config,
               std::vector<visited_url_ranking::URLVisitAggregate> visits,
               visited_url_ranking::VisitedURLRankingService::
                   RankURLVisitAggregatesCallback callback) {
              std::move(callback).Run(
                  visited_url_ranking::ResultStatus::kSuccess,
                  std::move(visits));
            });
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  page_content_annotations::TestPageContentAnnotationsService*
  page_content_annotations_service() {
    return page_content_annotations_service_.get();
  }
  visited_url_ranking::MockVisitedURLRankingService* mock_ranking_service() {
    return &mock_ranking_service_;
  }
  TestingPrefServiceSimple* test_pref_service() { return &test_pref_service_; }
  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }
  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<page_content_annotations::TestPageContentAnnotationsService>
      page_content_annotations_service_;
  visited_url_ranking::MockVisitedURLRankingService mock_ranking_service_;
  TestingPrefServiceSimple test_pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
};

class MockDelegate : public AuxiliarySearchDonationService::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              DonateHistoryEntries,
              (std::vector<AuxiliarySearchDonationService::HistoryData>,
               CoreAccountInfo),
              (override));
  MOCK_METHOD(void, SetBrowsingDataDonationEnabled, (bool), (override));
};

TEST_F(AuxiliarySearchDonationServiceTest, BrowsingDataDonationPrefChanged) {
  base::test::TestFuture<bool> future;
  auto delegate = std::make_unique<MockDelegate>();
  EXPECT_CALL(*delegate, SetBrowsingDataDonationEnabled(_))
      .WillRepeatedly(InvokeFuture(future));
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(), std::move(delegate));

  test_pref_service()->SetBoolean(
      prefs::kAuxiliarySearchBrowsingDataDonationEnabled, false);
  EXPECT_FALSE(future.Take());

  test_pref_service()->SetBoolean(
      prefs::kAuxiliarySearchBrowsingDataDonationEnabled, true);
  EXPECT_TRUE(future.Take());
}

TEST_F(AuxiliarySearchDonationServiceTest, IgnoresRemoteVisits) {
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(0);

  HistoryVisit remote_visit;
  remote_visit.navigation_id = 0;
  service.OnPageContentAnnotated(remote_visit, CreateAnnotationsResult());
}

TEST_F(AuxiliarySearchDonationServiceTest,
       IgnoresVisitsWhenBrowsingDataDonationDisabled) {
  test_pref_service()->SetBoolean(
      prefs::kAuxiliarySearchBrowsingDataDonationEnabled, false);
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(0);

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
}

TEST_F(AuxiliarySearchDonationServiceTest,
       DisablingBrowsingDataDonationCancelsDonationTimer) {
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(0);

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  test_pref_service()->SetBoolean(
      prefs::kAuxiliarySearchBrowsingDataDonationEnabled, false);
  task_environment().FastForwardBy(service.GetDonationDelay());
}

TEST_F(AuxiliarySearchDonationServiceTest, FetchesLocalVisitAfterDelay) {
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());
  base::test::TestFuture<void> future;
  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
      .WillOnce(InvokeFuture(future));

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  base::TimeTicks before = task_environment().NowTicks();
  ASSERT_TRUE(future.Wait()) << "Failed to wait for fetch";
  base::TimeTicks after = task_environment().NowTicks();

  EXPECT_EQ(after - before, service.GetDonationDelay());
}

TEST_F(AuxiliarySearchDonationServiceTest,
       MultipleAnnotationsFetchesOnlyOnceAfterDelay) {
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(1);

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay() +
                                   service.GetDonationDelay());
}

TEST_F(AuxiliarySearchDonationServiceTest,
       MultipleAnnotationsFetchesAgainAfterDelay) {
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(2);

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
}

TEST_F(AuxiliarySearchDonationServiceTest, FirstFetchUsesDefaultBeginTime) {
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());

  base::Time begin_time;
  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
      .Times(1)
      .WillOnce(WithArg<0>(SaveBeginTime(&begin_time)));

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  // The begin time for the first fetch is some implementation specific time
  // before donation is triggered.
  EXPECT_EQ(begin_time,
            base::Time::Now() - service.GetHistoryAgeThresholdForTesting());
}

TEST_F(AuxiliarySearchDonationServiceTest, FetchUsesLastTime) {
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());

  // First fetch returns the fake visit time as metadata. The second fetch
  // should use the provided fake visit time (plus 1us to ensure that the same
  // entry isn't fetched twice).
  const base::Time fake_visit_time = base::Time::Now() - base::Hours(1);
  base::Time begin_time;
  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
      .WillOnce(RunOnceCallback<1>(
          ResultStatus::kSuccess,
          URLVisitsMetadata{.most_recent_timestamp = fake_visit_time},
          CreateVisitAggregates()))
      .WillOnce(WithArg<0>(SaveBeginTime(&begin_time)));

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  EXPECT_EQ(begin_time, fake_visit_time + base::Microseconds(1));
}

TEST_F(AuxiliarySearchDonationServiceTest, FetchDoesNotFetchTooFarBack) {
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());
  // First fetch returns the fake visit time as metadata. The second fetch
  // should not use the provided fake visit time because it is too far back.
  const base::Time fake_visit_time = base::Time::Now() - base::Hours(1);
  base::Time begin_time;
  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
      .WillOnce(RunOnceCallback<1>(
          ResultStatus::kSuccess,
          URLVisitsMetadata{.most_recent_timestamp = fake_visit_time},
          CreateVisitAggregates()))
      .WillOnce(WithArg<0>(SaveBeginTime(&begin_time)));

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
  task_environment().FastForwardBy(service.GetHistoryAgeThresholdForTesting());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  EXPECT_EQ(begin_time,
            base::Time::Now() - service.GetHistoryAgeThresholdForTesting());
}

TEST_F(AuxiliarySearchDonationServiceTest, FetchDoesNotUpdateBeginTimeOnError) {
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());

  // First fetch returns a fake visit time as metadata. The second fetch
  // returns an error, but includes a different fake visit time. The third fetch
  // should still use the fake visit time from the first fetch (plus 1us).
  const base::Time fake_visit_time_1 = base::Time::Now() - base::Hours(2);
  const base::Time fake_visit_time_2 = base::Time::Now() - base::Hours(1);
  base::Time begin_time;
  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
      .WillOnce(RunOnceCallback<1>(
          ResultStatus::kSuccess,
          URLVisitsMetadata{.most_recent_timestamp = fake_visit_time_1},
          CreateVisitAggregates()))
      .WillOnce(
          RunOnceCallback<1>(ResultStatus::kError,
                             URLVisitsMetadata{
                                 .most_recent_timestamp = fake_visit_time_2,
                             },
                             CreateVisitAggregates()))
      .WillOnce(WithArg<0>(SaveBeginTime(&begin_time)));

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  EXPECT_EQ(begin_time, fake_visit_time_1 + base::Microseconds(1));
}

TEST_F(AuxiliarySearchDonationServiceTest, LastFetchTimePersistsInPrefs) {
  // First fetch returns the fake visit time as metadata. The second fetch
  // should use the provided fake visit time (plus 1us).
  const base::Time fake_visit_time = base::Time::Now() - base::Hours(1);
  base::Time begin_time;
  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
      .WillOnce(RunOnceCallback<1>(
          ResultStatus::kSuccess,
          URLVisitsMetadata{.most_recent_timestamp = fake_visit_time},
          CreateVisitAggregates()))
      .WillOnce(WithArg<0>(SaveBeginTime(&begin_time)));

  {
    AuxiliarySearchDonationService service(
        page_content_annotations_service(), mock_ranking_service(),
        identity_manager(), test_pref_service(),
        std::make_unique<NiceMock<MockDelegate>>());
    service.OnPageContentAnnotated(CreateLocalVisit(),
                                   CreateAnnotationsResult());
    task_environment().FastForwardBy(service.GetDonationDelay());
  }
  {
    AuxiliarySearchDonationService service(
        page_content_annotations_service(), mock_ranking_service(),
        identity_manager(), test_pref_service(),
        std::make_unique<NiceMock<MockDelegate>>());
    service.OnPageContentAnnotated(CreateLocalVisit(),
                                   CreateAnnotationsResult());
    task_environment().FastForwardBy(service.GetDonationDelay());
  }

  EXPECT_EQ(begin_time, fake_visit_time + base::Microseconds(1));
}

TEST_F(AuxiliarySearchDonationServiceTest,
       PausingApplicationTriggersImmediateDonation) {
  base::test::TestFuture<base::android::ApplicationState> future;
  auto listener = base::android::ApplicationStatusListener::New(
      future.GetRepeatingCallback());
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(1);

  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES);
  EXPECT_TRUE(future.Wait());
}

TEST_F(AuxiliarySearchDonationServiceTest,
       PausingApplicationDoesNothingIfTheresNoAnnotation) {
  base::test::TestFuture<base::android::ApplicationState> future;
  auto listener = base::android::ApplicationStatusListener::New(
      future.GetRepeatingCallback());
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(),
      std::make_unique<NiceMock<MockDelegate>>());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(0);

  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES);
  EXPECT_TRUE(future.Wait());
}

TEST_F(AuxiliarySearchDonationServiceTest,
       DonatesHistoryEntriesWithoutAccount) {
  const base::Time fake_visit_time = base::Time::Now() - base::Hours(1);
  {
    std::vector<visited_url_ranking::URLVisitAggregate> aggregates;
    aggregates.push_back(visited_url_ranking::CreateSampleURLVisitAggregate(
        GURL("https://example.com"),
        /*visibility_score=*/1.0f,
        /*time=*/fake_visit_time,
        /*fetchers=*/{visited_url_ranking::Fetcher::kHistory}));
    EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
        .WillOnce(RunOnceCallback<1>(
            ResultStatus::kSuccess,
            URLVisitsMetadata{.most_recent_timestamp = fake_visit_time},
            std::move(aggregates)));
  }
  base::test::TestFuture<
      std::vector<AuxiliarySearchDonationService::HistoryData>, CoreAccountInfo>
      future;
  auto delegate = std::make_unique<MockDelegate>();
  EXPECT_CALL(*delegate, DonateHistoryEntries(_, _))
      .WillOnce(InvokeFuture(future));
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(), std::move(delegate));

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  EXPECT_TRUE(future.IsReady());
  auto [entries, account] = future.Take();
  EXPECT_EQ(entries.size(), 1u);
  // `CreateSampleURLVisitAggregate` has a hard-coded title, so don't check it
  // here as it could change.
  EXPECT_THAT(
      entries,
      ElementsAre(AllOf(
          testing::Field(&AuxiliarySearchDonationService::HistoryData::url,
                         GURL("https://example.com")),
          testing::Field(
              &AuxiliarySearchDonationService::HistoryData::last_visited,
              fake_visit_time))));
  EXPECT_TRUE(account.IsEmpty());
}

TEST_F(AuxiliarySearchDonationServiceTest, DonatesHistoryEntriesWithAccount) {
  AccountInfo account_info = identity_test_env().MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);

  const base::Time fake_visit_time = base::Time::Now() - base::Hours(1);
  {
    std::vector<visited_url_ranking::URLVisitAggregate> aggregates;
    aggregates.push_back(visited_url_ranking::CreateSampleURLVisitAggregate(
        GURL("https://example.com"),
        /*visibility_score=*/1.0f,
        /*time=*/fake_visit_time,
        /*fetchers=*/{visited_url_ranking::Fetcher::kHistory}));
    EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
        .WillOnce(RunOnceCallback<1>(
            ResultStatus::kSuccess,
            URLVisitsMetadata{.most_recent_timestamp = fake_visit_time},
            std::move(aggregates)));
  }
  base::test::TestFuture<
      std::vector<AuxiliarySearchDonationService::HistoryData>, CoreAccountInfo>
      future;
  auto delegate = std::make_unique<MockDelegate>();
  EXPECT_CALL(*delegate, DonateHistoryEntries(_, _))
      .WillOnce(InvokeFuture(future));
  AuxiliarySearchDonationService service(
      page_content_annotations_service(), mock_ranking_service(),
      identity_manager(), test_pref_service(), std::move(delegate));

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  EXPECT_TRUE(future.IsReady());
  auto [entries, account] = future.Take();
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_THAT(
      entries,
      ElementsAre(AllOf(
          testing::Field(&AuxiliarySearchDonationService::HistoryData::url,
                         GURL("https://example.com")),
          testing::Field(
              &AuxiliarySearchDonationService::HistoryData::last_visited,
              fake_visit_time))));
  EXPECT_FALSE(account.IsEmpty());
  EXPECT_EQ(account.email, "test@gmail.com");
  EXPECT_EQ(account.gaia, account_info.GetGaiaId());
}

}  // namespace
