// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/post_login_glanceables_metrics_recorder.h"

#include <list>

#include "ash/calendar/calendar_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/time/calendar_list_model.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/unified/date_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"

namespace ash {

namespace {

using ::google_apis::calendar::SingleCalendar;

constexpr char kCalendarId1[] = "user1@email.com";
constexpr char kCalendarSummary1[] = "user1@email.com";
constexpr char kCalendarColorId1[] = "12";
bool kCalendarSelected1 = true;
bool kCalendarPrimary1 = true;

}  // namespace

class PostLoginGlanceablesMetricsRecorderTest : public AshTestBase {
 public:
  PostLoginGlanceablesMetricsRecorderTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();

    histogram_tester_ = std::make_unique<base::HistogramTester>();
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
      int thirty_second_delay_sample_count,
      int five_minute_delay_sample_count,
      int fifteen_minute_delay_sample_count,
      int thirty_minute_delay_sample_count) {
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.NoDelay", source,
        /*expected_bucket_count=*/no_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.15SecondDelay", source,
        /*expected_bucket_count=*/fifteen_second_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.30SecondDelay", source,
        /*expected_bucket_count=*/thirty_second_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.5MinuteDelay", source,
        /*expected_bucket_count=*/five_minute_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.15MinuteDelay", source,
        /*expected_bucket_count=*/fifteen_minute_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.30MinuteDelay", source,
        /*expected_bucket_count=*/thirty_minute_delay_sample_count);
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(PostLoginGlanceablesMetricsRecorderTest, OverviewFetch) {
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/1,
      /*fifteen_second_delay_sample_count=*/1,
      /*thirty_second_delay_sample_count=*/1,
      /*five_minute_delay_sample_count=*/1,
      /*fifteen_minute_delay_sample_count=*/1,
      /*thirty_minute_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/2,
      /*fifteen_second_delay_sample_count=*/1,
      /*thirty_second_delay_sample_count=*/1,
      /*five_minute_delay_sample_count=*/1,
      /*fifteen_minute_delay_sample_count=*/1,
      /*thirty_minute_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/3,
      /*fifteen_second_delay_sample_count=*/2,
      /*thirty_second_delay_sample_count=*/1,
      /*five_minute_delay_sample_count=*/1,
      /*fifteen_minute_delay_sample_count=*/1,
      /*thirty_minute_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/4,
      /*fifteen_second_delay_sample_count=*/2,
      /*thirty_second_delay_sample_count=*/2,
      /*five_minute_delay_sample_count=*/1,
      /*fifteen_minute_delay_sample_count=*/1,
      /*thirty_minute_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Minutes(10));
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/5,
      /*fifteen_second_delay_sample_count=*/3,
      /*thirty_second_delay_sample_count=*/3,
      /*five_minute_delay_sample_count=*/2,
      /*fifteen_minute_delay_sample_count=*/1,
      /*thirty_minute_delay_sample_count=*/1);
  task_environment()->FastForwardBy(base::Minutes(10));
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/6,
      /*fifteen_second_delay_sample_count=*/4,
      /*thirty_second_delay_sample_count=*/4,
      /*five_minute_delay_sample_count=*/3,
      /*fifteen_minute_delay_sample_count=*/2,
      /*thirty_minute_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Minutes(20));
  ToggleOverview();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kOverview,
      /*no_delay_sample_count=*/7,
      /*fifteen_second_delay_sample_count=*/5,
      /*thirty_second_delay_sample_count=*/5,
      /*five_minute_delay_sample_count=*/4,
      /*fifteen_minute_delay_sample_count=*/3,
      /*thirty_minute_delay_sample_count=*/2);
}

TEST_F(PostLoginGlanceablesMetricsRecorderTest, FullRestore) {
  ExpectBucketCounts(PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::
                         kPostLoginFullRestore,
                     /*no_delay_sample_count=*/0,
                     /*fifteen_second_delay_sample_count=*/0,
                     /*thirty_second_delay_sample_count=*/0,
                     /*five_minute_delay_sample_count=*/0,
                     /*fifteen_minute_delay_sample_count=*/0,
                     /*thirty_minute_delay_sample_count=*/0);

  Shell::Get()
      ->post_login_glanceables_metrics_reporter()
      ->RecordPostLoginFullRestoreShown();

  ExpectBucketCounts(PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::
                         kPostLoginFullRestore,
                     /*no_delay_sample_count=*/1,
                     /*fifteen_second_delay_sample_count=*/1,
                     /*thirty_second_delay_sample_count=*/1,
                     /*five_minute_delay_sample_count=*/1,
                     /*fifteen_minute_delay_sample_count=*/1,
                     /*thirty_minute_delay_sample_count=*/1);
}

class PostLoginGlanceablesMetricsRecorderCalendarTest
    : public AshTestBase,
      public testing::WithParamInterface</*multi_calendar_enabled=*/bool> {
 public:
  PostLoginGlanceablesMetricsRecorderCalendarTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatureState(features::kMultiCalendarSupport,
                                              IsMultiCalendarEnabled());
  }

  void SetUp() override {
    AshTestBase::SetUp();

    AccountId account_id = AccountId::FromUserEmail("user1@email.com");
    calendar_client_ =
        std::make_unique<calendar_test_utils::CalendarClientTestImpl>();
    Shell::Get()->calendar_controller()->SetActiveUserAccountIdForTesting(
        account_id);
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id, calendar_client_.get());

    if (IsMultiCalendarEnabled()) {
      // A calendar list is fetched prior to the event fetch. Set a mock result
      // so the calendar list fetch is successful.
      SetCalendarList();
      // Shorten the response delay so the calendar list fetch returns quickly.
      calendar_client_->SetResponseDelay(base::Milliseconds(100));
    }

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  bool IsMultiCalendarEnabled() { return GetParam(); }

  void SetCalendarList() {
    // Sets a mock calendar list.
    std::list<std::unique_ptr<google_apis::calendar::SingleCalendar>> calendars;
    calendars.push_back(calendar_test_utils::CreateCalendar(
        kCalendarId1, kCalendarSummary1, kCalendarColorId1, kCalendarSelected1,
        kCalendarPrimary1));
    calendar_client_->SetCalendarList(
        calendar_test_utils::CreateMockCalendarList(std::move(calendars)));
  }

  void ToggleDateTray() {
    DateTray* date_tray =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->date_tray();
    LeftClickOn(date_tray);
    task_environment()->FastForwardBy(base::Milliseconds(300));
    LeftClickOn(date_tray);
  }

  void ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource source,
      int no_delay_sample_count,
      int fifteen_second_delay_sample_count,
      int thirty_second_delay_sample_count,
      int five_minute_delay_sample_count,
      int fifteen_minute_delay_sample_count,
      int thirty_minute_delay_sample_count) {
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.NoDelay", source,
        /*expected_bucket_count=*/no_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.15SecondDelay", source,
        /*expected_bucket_count=*/fifteen_second_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.30SecondDelay", source,
        /*expected_bucket_count=*/thirty_second_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.5MinuteDelay", source,
        /*expected_bucket_count=*/five_minute_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.15MinuteDelay", source,
        /*expected_bucket_count=*/fifteen_minute_delay_sample_count);
    histogram_tester_->ExpectBucketCount(
        "Ash.PostLoginGlanceables.HypotheticalFetchEvent.30MinuteDelay", source,
        /*expected_bucket_count=*/thirty_minute_delay_sample_count);
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::unique_ptr<calendar_test_utils::CalendarClientTestImpl> calendar_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(MultiCalendar,
                         PostLoginGlanceablesMetricsRecorderCalendarTest,
                         testing::Bool());

TEST_P(PostLoginGlanceablesMetricsRecorderCalendarTest, CalendarFetch) {
  ToggleDateTray();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kCalendar,
      /*no_delay_sample_count=*/4,
      /*fifteen_second_delay_sample_count=*/1,
      /*thirty_second_delay_sample_count=*/1,
      /*five_minute_delay_sample_count=*/1,
      /*fifteen_minute_delay_sample_count=*/1,
      /*thirty_minute_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleDateTray();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kCalendar,
      /*no_delay_sample_count=*/8,
      /*fifteen_second_delay_sample_count=*/1,
      /*thirty_second_delay_sample_count=*/1,
      /*five_minute_delay_sample_count=*/1,
      /*fifteen_minute_delay_sample_count=*/1,
      /*thirty_minute_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleDateTray();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kCalendar,
      /*no_delay_sample_count=*/12,
      /*fifteen_second_delay_sample_count=*/2,
      /*thirty_second_delay_sample_count=*/1,
      /*five_minute_delay_sample_count=*/1,
      /*fifteen_minute_delay_sample_count=*/1,
      /*thirty_minute_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Seconds(12));
  ToggleDateTray();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kCalendar,
      /*no_delay_sample_count=*/16,
      /*fifteen_second_delay_sample_count=*/2,
      /*thirty_second_delay_sample_count=*/2,
      /*five_minute_delay_sample_count=*/1,
      /*fifteen_minute_delay_sample_count=*/1,
      /*thirty_minute_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Minutes(20));
  ToggleDateTray();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kCalendar,
      /*no_delay_sample_count=*/20,
      /*fifteen_second_delay_sample_count=*/3,
      /*thirty_second_delay_sample_count=*/3,
      /*five_minute_delay_sample_count=*/2,
      /*fifteen_minute_delay_sample_count=*/2,
      /*thirty_minute_delay_sample_count=*/1);

  task_environment()->FastForwardBy(base::Minutes(20));
  ToggleDateTray();

  ExpectBucketCounts(
      PostLoginGlanceablesMetricsRecorder::DataFetchEventSource::kCalendar,
      /*no_delay_sample_count=*/24,
      /*fifteen_second_delay_sample_count=*/4,
      /*thirty_second_delay_sample_count=*/4,
      /*five_minute_delay_sample_count=*/3,
      /*fifteen_minute_delay_sample_count=*/3,
      /*thirty_minute_delay_sample_count=*/2);
}

}  // namespace ash
