// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/post_login_glanceables_metrics_recorder.h"

#include "ash/calendar/calendar_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/unified/date_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"

namespace ash {

class PostLoginGlanceablesMetricsRecorderTest : public AshTestBase {
 public:
  PostLoginGlanceablesMetricsRecorderTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();

    AccountId account_id = AccountId::FromUserEmail("user1@email.com");
    calendar_client_ =
        std::make_unique<calendar_test_utils::CalendarClientTestImpl>();
    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        account_id);
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id, calendar_client_.get());

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void ToggleDateTray() {
    DateTray* date_tray =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->date_tray();
    LeftClickOn(date_tray);
    LeftClickOn(date_tray);
  }

  void ToggleOverview() {
    auto* overview_controller = Shell::Get()->overview_controller();
    overview_controller->StartOverview(OverviewStartAction::kTests);
    overview_controller->EndOverview(OverviewEndAction::kTests);
  }

  void ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource source,
      int no_delay_sample_count,
      int fifteen_second_delay_sample_count,
      int thirty_second_delay_sample_count) {
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.NoDelay", source,
        /*expected_bucket_count=*/no_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.15SecondDelay", source,
        /*expected_bucket_count=*/fifteen_second_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.30SecondDelay", source,
        /*expected_bucket_count=*/thirty_second_delay_sample_count);
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::unique_ptr<calendar_test_utils::CalendarClientTestImpl> calendar_client_;
};

TEST_F(PostLoginGlanceablesMetricsRecorderTest, OverviewFetch) {
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/1,
      /*fifteen_second_delay_sample_count=*/1,
      /*thirty_second_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/2,
      /*fifteen_second_delay_sample_count=*/1,
      /*thirty_second_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/3,
      /*fifteen_second_delay_sample_count=*/2,
      /*thirty_second_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/4,
      /*fifteen_second_delay_sample_count=*/2,
      /*thirty_second_delay_sample_count=*/2);
}

TEST_F(PostLoginGlanceablesMetricsRecorderTest, CalendarFetch) {
  ToggleDateTray();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kCalendar,
      /*no_delay_sample_count=*/4,
      /*fifteen_second_delay_sample_count=*/1,
      /*thirty_second_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleDateTray();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kCalendar,
      /*no_delay_sample_count=*/8,
      /*fifteen_second_delay_sample_count=*/1,
      /*thirty_second_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleDateTray();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kCalendar,
      /*no_delay_sample_count=*/12,
      /*fifteen_second_delay_sample_count=*/2,
      /*thirty_second_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleDateTray();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kCalendar,
      /*no_delay_sample_count=*/16,
      /*fifteen_second_delay_sample_count=*/2,
      /*thirty_second_delay_sample_count=*/2);
}

TEST_F(PostLoginGlanceablesMetricsRecorderTest, FullRestore) {
  ExpectBucketCounts(PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::
                         kPostLoginFullRestore,
                     /*no_delay_sample_count=*/0,
                     /*fifteen_second_delay_sample_count=*/0,
                     /*thirty_second_delay_sample_count=*/0);

  Shell::Get()
      ->post_login_glanceables_metrics_reporter()
      ->RecordPostLoginFullRestoreShown();

  ExpectBucketCounts(PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::
                         kPostLoginFullRestore,
                     /*no_delay_sample_count=*/1,
                     /*fifteen_second_delay_sample_count=*/1,
                     /*thirty_second_delay_sample_count=*/1);
}

}  // namespace ash
