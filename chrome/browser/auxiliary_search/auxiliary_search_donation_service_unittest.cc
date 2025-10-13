// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::page_content_annotations::HistoryVisit;
using ::page_content_annotations::PageContentAnnotationsResult;
using ::testing::_;

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
  task_environment().FastForwardBy(service.GetDonationDelayForTesting());
}

TEST_F(AuxiliarySearchDonationServiceTest,
       MultipleAnnotationsFetchesOnlyOnceAfterDelay) {
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());
  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(1);

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelayForTesting() +
                                   service.GetDonationDelayForTesting());
}

TEST_F(AuxiliarySearchDonationServiceTest,
       MultipleAnnotationsFetchesAgainAfterDelay) {
  AuxiliarySearchDonationService service(page_content_annotations_service(),
                                         mock_ranking_service());

  EXPECT_CALL(*mock_ranking_service(), FetchURLVisitAggregates(_, _)).Times(2);

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelayForTesting());

  service.OnPageContentAnnotated(CreateLocalVisit(), CreateAnnotationsResult());
  task_environment().FastForwardBy(service.GetDonationDelayForTesting());
}

}  // namespace
