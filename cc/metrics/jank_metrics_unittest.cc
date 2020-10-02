// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/jank_metrics.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/metrics/throughput_ukm_reporter.h"
#include "cc/trees/ukm_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class JankMetricsTest : public testing::Test {
 public:
  JankMetricsTest() = default;
  ~JankMetricsTest() override = default;

  // Create a sequence of PresentationFeedback for testing based on the provided
  // sequence of actual frame intervals and the expected frame interval. The
  // size of the returned sequence is |actual_intervals_ms|.size() + 1
  static std::vector<gfx::PresentationFeedback> CreateFeedbackSequence(
      const std::vector<double>& actual_intervals_ms,
      double expected_interval_ms) {
    std::vector<gfx::PresentationFeedback> feedbacks;

    // The timestamp of the first presentation.
    base::TimeTicks start_time = base::TimeTicks::Now();
    double accum_interval = 0.0;
    base::TimeDelta expected_interval =
        base::TimeDelta::FromMillisecondsD(expected_interval_ms);

    feedbacks.emplace_back(
        gfx::PresentationFeedback(start_time, expected_interval, 0));
    for (auto interval : actual_intervals_ms) {
      accum_interval += interval;
      feedbacks.emplace_back(gfx::PresentationFeedback{
          start_time + base::TimeDelta::FromMillisecondsD(accum_interval),
          expected_interval, 0});
    }
    return feedbacks;
  }

  // Notify |jank_reporter| of all presentations in |feedbacks|.
  void AddPresentedFramesToJankReporter(
      JankMetrics* jank_reporter,
      const std::vector<gfx::PresentationFeedback>& feedbacks) {
    for (auto feedback : feedbacks) {
      jank_reporter->AddPresentedFrame(feedback.timestamp, feedback.interval);
    }
  }
};

TEST_F(JankMetricsTest, CompositorAnimationMildFluctuationNoJank) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kCompositorAnimation;
  FrameSequenceMetrics::ThreadType thread_type =
      FrameSequenceMetrics::ThreadType::kCompositor;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // No jank. Small upticks such as 15->17 or 14->18 do not qualify as janks.
  auto feedbacks =
      CreateFeedbackSequence({16.67, 16.67, 15, 17, 14, 18, 15, 16.67}, 16.67);

  AddPresentedFramesToJankReporter(&jank_reporter, feedbacks);
  jank_reporter.ReportJankMetrics(100u);

  // One sample of 0 janks reported for "Compositor".
  const char* metric =
      "Graphics.Smoothness.Jank.Compositor.CompositorAnimation";
  const char* invalid_metric =
      "Graphics.Smoothness.Jank.Main.CompositorAnimation";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(0, 1)));

  // No reporting for "Main".
  histogram_tester.ExpectTotalCount(invalid_metric, 0u);
}

TEST_F(JankMetricsTest, MainThreadAnimationOneJank) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kMainThreadAnimation;
  FrameSequenceMetrics::ThreadType thread_type =
      FrameSequenceMetrics::ThreadType::kMain;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // One Main thread jank from 15 to 24, since 24 - 15 = 9, which is greater
  // then 0.5 * frame_interval = 8.33. The jank occurrence is visually marked
  // with a "+" sign.
  auto feedbacks =
      CreateFeedbackSequence({48, 15, +24, 14, 18, 15, 16.67}, 16.67);

  AddPresentedFramesToJankReporter(&jank_reporter, feedbacks);
  jank_reporter.ReportJankMetrics(100u);

  // One jank is reported for "Main".
  const char* metric = "Graphics.Smoothness.Jank.Main.MainThreadAnimation";
  const char* invalid_metric =
      "Graphics.Smoothness.Jank.Compositor.MainThreadAnimation";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(1, 1)));

  // No jank is reported for "Compositor"
  histogram_tester.ExpectTotalCount(invalid_metric, 0u);
}

TEST_F(JankMetricsTest, VideoManyJanksOver300ExpectedFrames) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type = FrameSequenceTrackerType::kVideo;
  FrameSequenceMetrics::ThreadType thread_type =
      FrameSequenceMetrics::ThreadType::kCompositor;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // 7 janks.
  auto feedbacks = CreateFeedbackSequence(
      {15, +33, +50, 33, 16, +33, +50, +100, +120, +180}, 16.67);

  AddPresentedFramesToJankReporter(&jank_reporter, feedbacks);
  jank_reporter.ReportJankMetrics(300u);

  // Report in the 7/300 ~= 2% bucket for "Compositor"
  const char* metric = "Graphics.Smoothness.Jank.Compositor.Video";
  const char* invalid_metric = "Graphics.Smoothness.Jank.Main.Video";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(2, 1)));

  // No jank is reported for "Main"
  histogram_tester.ExpectTotalCount(invalid_metric, 0u);
}

TEST_F(JankMetricsTest, WheelScrollMainThreadTwoJanks) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kWheelScroll;
  FrameSequenceMetrics::ThreadType thread_type =
      FrameSequenceMetrics::ThreadType::kMain;

  JankMetrics jank_reporter{tracker_type, thread_type};

  auto feedbacks = CreateFeedbackSequence({33, 16, +33, +48, 33}, 16.67);
  AddPresentedFramesToJankReporter(&jank_reporter, feedbacks);
  jank_reporter.ReportJankMetrics(100u);

  // Expect 2 janks for "Main" and no jank for "Compositor"
  const char* metric = "Graphics.Smoothness.Jank.Main.WheelScroll";
  const char* invalid_metric =
      "Graphics.Smoothness.Jank.Compositor.WheelScroll";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(2, 1)));

  histogram_tester.ExpectTotalCount(invalid_metric, 0u);
}

TEST_F(JankMetricsTest, TouchScrollCompositorThreadManyJanks) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kTouchScroll;
  FrameSequenceMetrics::ThreadType thread_type =
      FrameSequenceMetrics::ThreadType::kCompositor;

  JankMetrics jank_reporter{tracker_type, thread_type};

  auto feedbacks =
      CreateFeedbackSequence({33, 16, +33, +48, +100, 16, +48, +100}, 16.67);

  AddPresentedFramesToJankReporter(&jank_reporter, feedbacks);
  jank_reporter.ReportJankMetrics(120u);

  // Expect janks in the 5/120 ~= 4% bucket for "Compositor", and no jank for
  // "Main"
  const char* metric = "Graphics.Smoothness.Jank.Compositor.TouchScroll";
  const char* invalid_metric = "Graphics.Smoothness.Jank.Main.TouchScroll";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(4, 1)));

  histogram_tester.ExpectTotalCount(invalid_metric, 0u);
}

// Test if the jank reporter can correctly merge janks from another jank
// reporter.
TEST_F(JankMetricsTest, RAFMergeJanks) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type = FrameSequenceTrackerType::kRAF;
  FrameSequenceMetrics::ThreadType thread_type =
      FrameSequenceMetrics::ThreadType::kMain;

  JankMetrics jank_reporter{tracker_type, thread_type};
  std::unique_ptr<JankMetrics> other_reporter =
      std::make_unique<JankMetrics>(tracker_type, thread_type);

  auto feedbacks = CreateFeedbackSequence({33, +50, 16, +33, 33, +48}, 16.67);
  AddPresentedFramesToJankReporter(other_reporter.get(), feedbacks);
  AddPresentedFramesToJankReporter(&jank_reporter, feedbacks);

  jank_reporter.Merge(std::move(other_reporter));
  jank_reporter.ReportJankMetrics(100u);

  // Expect 6 janks for "Main" (3 from each reporter)
  const char* metric = "Graphics.Smoothness.Jank.Main.RAF";
  const char* invalid_metric = "Graphics.Smoothness.Jank.Compositor.RAF";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(6, 1)));

  histogram_tester.ExpectTotalCount(invalid_metric, 0u);
}

// Test if jank reporting is correctly disabled for Custom trackers.
TEST_F(JankMetricsTest, CustomNotReported) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type = FrameSequenceTrackerType::kCustom;
  FrameSequenceMetrics::ThreadType thread_type =
      FrameSequenceMetrics::ThreadType::kMain;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // There should be 4 janks, but the jank reporter does not track or report
  // them.
  auto feedbacks = CreateFeedbackSequence({16, +33, +48, 16, +33, +48}, 16.67);

  AddPresentedFramesToJankReporter(&jank_reporter, feedbacks);
  jank_reporter.ReportJankMetrics(100u);

  // Expect no jank reports even though the sequence contains jank
  histogram_tester.ExpectTotalCount("Graphics.Smoothness.Jank.Main.Custom", 0u);
  histogram_tester.ExpectTotalCount(
      "Graphics.Smoothness.Jank.Compositor.Custom", 0u);
}

}  // namespace cc
