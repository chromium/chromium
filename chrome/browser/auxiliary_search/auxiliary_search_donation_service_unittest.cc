// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"

#include <memory>

#include "base/android/application_status_listener.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::RunOnceCallback;
using ::page_content_annotations::HistoryVisit;
using ::page_content_annotations::PageContentAnnotationsResult;
using ::testing::_;
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
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  page_content_annotations::TestPageContentAnnotationsService*
  page_content_annotations_service() {
    return page_content_annotations_service_.get();
  }
  visited_url_ranking::MockVisitedURLRankingService* mock_ranking_service() {
    return &mock_ranking_service_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<page_content_annotations::TestPageContentAnnotationsService>
      page_content_annotations_service_;
  visited_url_ranking::MockVisitedURLRankingService mock_ranking_service_;
};

TEST_F(AuxiliarySearchDonationServiceTest, IgnoresRemoteVisits) {
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(0);

  HistoryVisit remote_visit;
  remote_visit.navigation_id = 0;
  service.OnPageContentAnnotated(remote_visit, CreateAnnotationsResult());
}

TEST_F(AuxiliarySearchDonationServiceTest, FetchesLocalVisitAfterDelay) {
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(1);

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
}

TEST_F(AuxiliarySearchDonationServiceTest,
       MultipleAnnotationsFetchesOnlyOnceAfterDelay) {
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(1);

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay() +
                                   service.GetDonationDelay());
}

TEST_F(AuxiliarySearchDonationServiceTest,
       MultipleAnnotationsFetchesAgainAfterDelay) {
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(2);

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
}

TEST_F(AuxiliarySearchDonationServiceTest, FirstFetchUsesDefaultBeginTime) {
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());

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
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());
  EXPECT_CALL(*mock_ranking_service(), RankURLVisitAggregates(_, _, _))
      .WillRepeatedly(
          RunOnceCallback<2>(ResultStatus::kSuccess, CreateVisitAggregates()));

  // First fetch returns the fake visit time as metadata. The second fetch
  // should use the provided fake visit time.
  const base::Time fake_visit_time = base::Time::Now() - base::Hours(1);
  base::Time begin_time;
  {
    testing::InSequence seq;
    EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
        .Times(1)
        .WillOnce(RunOnceCallback<1>(
            ResultStatus::kSuccess,
            URLVisitsMetadata{.most_recent_timestamp = fake_visit_time},
            CreateVisitAggregates()));
    EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
        .Times(1)
        .WillOnce(WithArg<0>(SaveBeginTime(&begin_time)));
  }

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  EXPECT_EQ(begin_time, fake_visit_time);
}

TEST_F(AuxiliarySearchDonationServiceTest, FetchDoesNotFetchTooFarBack) {
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());
  EXPECT_CALL(*mock_ranking_service(), RankURLVisitAggregates(_, _, _))
      .WillRepeatedly(
          RunOnceCallback<2>(ResultStatus::kSuccess, CreateVisitAggregates()));
  // First fetch returns the fake visit time as metadata. The second fetch
  // should not use the provided fake visit time because it is too far back.
  const base::Time fake_visit_time = base::Time::Now() - base::Hours(1);
  base::Time begin_time;
  {
    testing::InSequence seq;
    EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
        .Times(1)
        .WillOnce(RunOnceCallback<1>(
            ResultStatus::kSuccess,
            URLVisitsMetadata{.most_recent_timestamp = fake_visit_time},
            CreateVisitAggregates()));
    EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
        .Times(1)
        .WillOnce(WithArg<0>(SaveBeginTime(&begin_time)));
  }

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
  task_environment().FastForwardBy(service.GetHistoryAgeThresholdForTesting());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  EXPECT_EQ(begin_time,
            base::Time::Now() - service.GetHistoryAgeThresholdForTesting());
}

TEST_F(AuxiliarySearchDonationServiceTest, FetchDoesNotUpdateBeginTimeOnError) {
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());
  EXPECT_CALL(*mock_ranking_service(), RankURLVisitAggregates(_, _, _))
      .WillRepeatedly(
          RunOnceCallback<2>(ResultStatus::kSuccess, CreateVisitAggregates()));

  // First fetch returns the fake visit time as metadata. The second fetch
  // returns an error. The third fetch should still use the fake visit time
  // from the first fetch.
  const base::Time fake_visit_time = base::Time::Now() - base::Hours(1);
  base::Time begin_time;
  {
    testing::InSequence seq;
    EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
        .Times(1)
        .WillOnce(RunOnceCallback<1>(
            ResultStatus::kSuccess,
            URLVisitsMetadata{.most_recent_timestamp = fake_visit_time},
            CreateVisitAggregates()));
    EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
        .Times(1)
        .WillOnce(RunOnceCallback<1>(ResultStatus::kError, URLVisitsMetadata{},
                                     CreateVisitAggregates()));
    EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _))
        .Times(1)
        .WillOnce(WithArg<0>(SaveBeginTime(&begin_time)));
  }

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelay());

  EXPECT_EQ(begin_time, fake_visit_time);
}

TEST_F(AuxiliarySearchDonationServiceTest,
       PausingApplicationTriggersImmediateDonation) {
  base::test::TestFuture<base::android::ApplicationState> future;
  auto listener = base::android::ApplicationStatusListener::New(
      future.GetRepeatingCallback());
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());
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
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(0);

  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES);
  EXPECT_TRUE(future.Wait());
}

}  // namespace
