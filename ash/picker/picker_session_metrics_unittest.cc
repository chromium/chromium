// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_session_metrics.h"

#include "ash/test/ash_test_base.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Le;

base::TimeTicks WaitUntilNextFramePresented(ui::Compositor* compositor) {
  base::TimeTicks presentation_timestamp;
  base::RunLoop run_loop;
  compositor->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting([&](base::TimeTicks timestamp) {
        run_loop.Quit();
        presentation_timestamp = timestamp;
      }));
  run_loop.Run();
  return presentation_timestamp;
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
  const base::TimeTicks contents_changed_timestamp = base::TimeTicks::Now();
  const base::TimeTicks presentation_timestamp_before =
      WaitUntilNextFramePresented(widget->GetCompositor());
  metrics.MarkContentsChanged();
  widget->SchedulePaintInRect(gfx::Rect(0, 0, 1, 1));
  const base::TimeTicks presentation_timestamp_after =
      WaitUntilNextFramePresented(widget->GetCompositor());

  histogram.ExpectTotalCount(
      "Ash.Picker.Session.PresentationLatency.SearchField", 1);
  // There may be intermediate frames between `presentation_timestamp_before`
  // and `presentation_timestamp_after`. Thus, these two timestamps can only
  // be used to bound the metric value.
  const base::TimeDelta latency_lower_bound =
      presentation_timestamp_before - contents_changed_timestamp;
  const base::TimeDelta latency_upper_bound =
      presentation_timestamp_after - contents_changed_timestamp;
  EXPECT_THAT(histogram.GetTotalSum(
                  "Ash.Picker.Session.PresentationLatency.SearchField"),
              AllOf(Ge(latency_lower_bound.InMilliseconds()),
                    Le(latency_upper_bound.InMilliseconds())));
}

}  // namespace
}  // namespace ash
