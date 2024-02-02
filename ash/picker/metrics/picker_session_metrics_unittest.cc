// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_session_metrics.h"

#include "ash/test/ash_test_base.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::IsEmpty;

void WaitUntilNextFramePresented(ui::Compositor* compositor) {
  base::RunLoop run_loop;
  compositor->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting([&](base::TimeTicks timestamp) {
        run_loop.Quit();
      }));
  run_loop.Run();
}

class PickerSessionMetricsTest : public AshTestBase {
 public:
  PickerSessionMetricsTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(PickerSessionMetricsTest,
       DoesNotRecordMetricsWithoutCallingStartRecording) {
  base::HistogramTester histogram;

  PickerSessionMetrics metrics(base::TimeTicks::Now());
  metrics.MarkInputFocus();
  metrics.MarkContentsChanged();
  metrics.MarkSearchResultsUpdated();

  EXPECT_THAT(histogram.GetTotalCountsForPrefix("Ash.Picker.Session"),
              IsEmpty());
}

TEST_F(PickerSessionMetricsTest, RecordsFirstFocusLatency) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  const auto trigger_event_timestamp = base::TimeTicks::Now();
  task_environment()->FastForwardBy(base::Seconds(1));
  PickerSessionMetrics metrics(trigger_event_timestamp);
  metrics.StartRecording(*widget);
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.InputReadyLatency",
                                   base::Seconds(2), 1);
}

TEST_F(PickerSessionMetricsTest, RecordsOnlyFirstFocusLatency) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  const auto trigger_event_timestamp = base::TimeTicks::Now();
  task_environment()->FastForwardBy(base::Seconds(1));
  PickerSessionMetrics metrics(trigger_event_timestamp);
  metrics.StartRecording(*widget);
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();
  // Mark a second focus. Only the first focus should be recorded.
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.InputReadyLatency",
                                   base::Seconds(2), 1);
}

TEST_F(PickerSessionMetricsTest, RecordsPresentationLatencyForSearchField) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  PickerSessionMetrics metrics(base::TimeTicks::Now());
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  WaitUntilNextFramePresented(widget->GetCompositor());

  histogram.ExpectTotalCount(
      "Ash.Picker.Session.PresentationLatency.SearchField", 1);
}

TEST_F(PickerSessionMetricsTest, RecordsPresentationLatencyForResults) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  PickerSessionMetrics metrics(base::TimeTicks::Now());
  metrics.StartRecording(*widget);
  metrics.MarkSearchResultsUpdated();
  WaitUntilNextFramePresented(widget->GetCompositor());

  histogram.ExpectTotalCount(
      "Ash.Picker.Session.PresentationLatency.SearchResults", 1);
}

TEST_F(PickerSessionMetricsTest, RecordsSearchLatencyOnSearchFinished) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  PickerSessionMetrics metrics;
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkSearchResultsUpdated();

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   base::Seconds(1), 1);
}

TEST_F(PickerSessionMetricsTest, DoesNotRecordSearchLatencyOnCanceledSearch) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  PickerSessionMetrics metrics;
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkContentsChanged();
  task_environment()->FastForwardBy(base::Seconds(2));
  metrics.MarkSearchResultsUpdated();

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   base::Seconds(2), 1);
}

}  // namespace
}  // namespace ash
