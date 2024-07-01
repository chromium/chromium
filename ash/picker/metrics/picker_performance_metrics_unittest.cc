// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_performance_metrics.h"

#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::IsEmpty;

void WaitUntilNextFramePresented(ui::Compositor* compositor) {
  base::RunLoop run_loop;
  compositor->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting(
          [&](const viz::FrameTimingDetails& frame_timing_details) {
            run_loop.Quit();
          }));
  run_loop.Run();
}

class PickerPerformanceMetricsTest : public views::ViewsTestBase {
 public:
  PickerPerformanceMetricsTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(PickerPerformanceMetricsTest,
       DoesNotRecordMetricsWithoutCallingStartRecording) {
  base::HistogramTester histogram;

  PickerPerformanceMetrics metrics(base::TimeTicks::Now());
  metrics.MarkInputFocus();
  metrics.MarkContentsChanged();
  metrics.MarkSearchResultsUpdated(
      PickerPerformanceMetrics::SearchResultsUpdate::kReplace);

  EXPECT_THAT(histogram.GetTotalCountsForPrefix("Ash.Picker.Session"),
              IsEmpty());
}

TEST_F(PickerPerformanceMetricsTest, RecordsFirstFocusLatency) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  const auto trigger_event_timestamp = base::TimeTicks::Now();
  task_environment()->FastForwardBy(base::Seconds(1));
  PickerPerformanceMetrics metrics(trigger_event_timestamp);
  metrics.StartRecording(*widget);
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.InputReadyLatency",
                                   base::Seconds(2), 1);
}

TEST_F(PickerPerformanceMetricsTest, RecordsOnlyFirstFocusLatency) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  const auto trigger_event_timestamp = base::TimeTicks::Now();
  task_environment()->FastForwardBy(base::Seconds(1));
  PickerPerformanceMetrics metrics(trigger_event_timestamp);
  metrics.StartRecording(*widget);
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();
  // Mark a second focus. Only the first focus should be recorded.
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.InputReadyLatency",
                                   base::Seconds(2), 1);
}

TEST_F(PickerPerformanceMetricsTest, RecordsPresentationLatencyForSearchField) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  PickerPerformanceMetrics metrics(base::TimeTicks::Now());
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  WaitUntilNextFramePresented(widget->GetCompositor());

  histogram.ExpectTotalCount(
      "Ash.Picker.Session.PresentationLatency.SearchField", 1);
}

TEST_F(PickerPerformanceMetricsTest, RecordsPresentationLatencyForResults) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  PickerPerformanceMetrics metrics(base::TimeTicks::Now());
  metrics.StartRecording(*widget);
  metrics.MarkSearchResultsUpdated(
      PickerPerformanceMetrics::SearchResultsUpdate::kReplace);
  WaitUntilNextFramePresented(widget->GetCompositor());

  histogram.ExpectTotalCount(
      "Ash.Picker.Session.PresentationLatency.SearchResults", 1);
}

TEST_F(PickerPerformanceMetricsTest,
       RecordsPresentationLatencyForResultsShowingNoResultsFound) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  PickerPerformanceMetrics metrics(base::TimeTicks::Now());
  metrics.StartRecording(*widget);
  metrics.MarkSearchResultsUpdated(
      PickerPerformanceMetrics::SearchResultsUpdate::kNoResultsFound);
  WaitUntilNextFramePresented(widget->GetCompositor());

  histogram.ExpectTotalCount(
      "Ash.Picker.Session.PresentationLatency.SearchResults", 1);
}

TEST_F(PickerPerformanceMetricsTest, RecordsSearchLatencyOnSearchFinished) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  PickerPerformanceMetrics metrics;
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkSearchResultsUpdated(
      PickerPerformanceMetrics::SearchResultsUpdate::kReplace);

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   base::Seconds(1), 1);
}

// TODO: b/349913604 - Replace this metric with a new one which records search
// latency on showing "no results found".
TEST_F(PickerPerformanceMetricsTest,
       DoesNotRecordSearchLatencyOnShowingNoResultsFound) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  PickerPerformanceMetrics metrics;
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  metrics.MarkSearchResultsUpdated(
      PickerPerformanceMetrics::SearchResultsUpdate::kNoResultsFound);

  histogram.ExpectTotalCount("Ash.Picker.Session.SearchLatency", 0);
}

TEST_F(PickerPerformanceMetricsTest,
    DoesNotRecordSearchLatencyOnCanceledSearch) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  PickerPerformanceMetrics metrics;
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkContentsChanged();
  task_environment()->FastForwardBy(base::Seconds(2));
  metrics.MarkSearchResultsUpdated(
      PickerPerformanceMetrics::SearchResultsUpdate::kReplace);

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   base::Seconds(2), 1);
}

}  // namespace
}  // namespace ash
