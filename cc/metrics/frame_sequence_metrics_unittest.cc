// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_metrics.h"

#include <cstdint>
#include <utility>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "cc/metrics/frame_info.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class FrameSequenceMetricsTest : public testing::Test {
 public:
  FrameSequenceMetricsTest() = default;
  ~FrameSequenceMetricsTest() override = default;

  void SetFramesExpectedAndProduced(FrameSequenceMetrics& metrics,
                                    uint32_t frames_expected,
                                    uint32_t frames_dropped);

  void SubmitSortedFrame(FrameSequenceMetrics& metrics,
                         uint64_t source_id,
                         uint64_t sequence_number,
                         base::TimeTicks frame_time,
                         FrameInfo::FrameFinalState final_state,
                         uint64_t frames_throttled_since_last = 0,
                         bool checkerboarded_needs_raster = false) {
    constexpr base::TimeDelta kInterval = base::Milliseconds(16);
    viz::BeginFrameArgs args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, source_id, sequence_number, frame_time,
        frame_time + kInterval, kInterval, viz::BeginFrameArgs::NORMAL);
    args.frames_throttled_since_last = frames_throttled_since_last;

    FrameInfo frame_info;
    frame_info.final_state = final_state;
    frame_info.final_state_v4 = final_state;
    frame_info.smooth_thread = FrameInfo::SmoothThread::kSmoothMain;
    frame_info.sequence_number = sequence_number;
    frame_info.termination_time = frame_time + kInterval;
    frame_info.checkerboarded_needs_raster = checkerboarded_needs_raster;
    metrics.AddSortedFrame(args, frame_info);
  }

  uint32_t FramesMissingContentV3(const FrameSequenceMetrics& metrics) const {
    return metrics.v3_.frames_missing_content;
  }
};

void FrameSequenceMetricsTest::SetFramesExpectedAndProduced(
    FrameSequenceMetrics& metrics,
    uint32_t frames_expected,
    uint32_t frames_dropped) {
  metrics.v3_.frames_expected = frames_expected;
  metrics.v3_.frames_dropped = frames_dropped;
}

TEST_F(FrameSequenceMetricsTest, MergeMetrics) {
  // Create a metric with only a small number of frames. It shouldn't report any
  // metrics.
  FrameSequenceMetrics first(FrameSequenceTrackerType::kTouchScroll);
  SetFramesExpectedAndProduced(first, 20u, 10u);
  EXPECT_FALSE(first.HasEnoughDataForReporting());

  // Create a second metric with too few frames to report any metrics.
  auto second = std::make_unique<FrameSequenceMetrics>(
      FrameSequenceTrackerType::kTouchScroll);
  SetFramesExpectedAndProduced(*second, 90u, 30u);
  EXPECT_FALSE(second->HasEnoughDataForReporting());

  // Merge the two metrics. The result should have enough frames to report
  // metrics.
  first.Merge(std::move(second));
  EXPECT_TRUE(first.HasEnoughDataForReporting());
}

#if DCHECK_IS_ON()
TEST_F(FrameSequenceMetricsTest, ScrollingThreadMergeMetrics) {
  FrameSequenceMetrics first(FrameSequenceTrackerType::kTouchScroll);
  first.SetScrollingThread(FrameInfo::SmoothEffectDrivingThread::kCompositor);
  SetFramesExpectedAndProduced(first, 20u, 10u);

  auto second = std::make_unique<FrameSequenceMetrics>(
      FrameSequenceTrackerType::kTouchScroll);
  SetFramesExpectedAndProduced(*second, 50u, 40u);
  second->SetScrollingThread(FrameInfo::SmoothEffectDrivingThread::kMain);

  ASSERT_DEATH_IF_SUPPORTED(first.Merge(std::move(second)), "");
}
#endif  // DCHECK_IS_ON()

TEST_F(FrameSequenceMetricsTest, CheckerboardingMetricV4EmitsValidValue) {
  // Tests historical DCHECK histogram metric array bound check
  // crash condition (crbug.com/373811619).
  base::HistogramTester histograms;

  // Create a metric with raster impl frames.
  FrameSequenceMetrics first(FrameSequenceTrackerType::kTouchScroll);
  SetFramesExpectedAndProduced(first, 300u, 10u);
  first.SetScrollingThread(FrameInfo::SmoothEffectDrivingThread::kCompositor);
  EXPECT_TRUE(first.HasEnoughDataForReporting());
  first.ReportMetrics();

  auto second = std::make_unique<FrameSequenceMetrics>(
      FrameSequenceTrackerType::kTouchScroll);
  SetFramesExpectedAndProduced(*second, 150u, 40u);
  second->SetScrollingThread(FrameInfo::SmoothEffectDrivingThread::kMain);
  second->ReportMetrics();

  // Checkerboarding V4 should be reported per-thread.
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.Checkerboarding4.MainThread.TouchScroll", 1u);
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.Checkerboarding4.CompositorThread.TouchScroll", 1u);
}

TEST_F(FrameSequenceMetricsTest,
       PausedBeginFramesDoNotRecordFalseJankAndPopulateCustomJankCount) {
  FrameSequenceMetrics metrics(FrameSequenceTrackerType::kCustom);
  FrameSequenceMetrics::CustomReportData reported;
  metrics.SetCustomReporter(base::BindLambdaForTesting(
      [&](const FrameSequenceMetrics::CustomReportData& data) {
        reported = data;
      }));

  const base::TimeTicks t0 = base::TimeTicks() + base::Seconds(1);
  // Two normal 16ms frames establish `last_frame_delta = 16ms` with 0 janks.
  SubmitSortedFrame(metrics, 1u, 1u, t0,
                    FrameInfo::FrameFinalState::kPresentedAll);
  SubmitSortedFrame(metrics, 1u, 2u, t0 + base::Milliseconds(16),
                    FrameInfo::FrameFinalState::kPresentedAll);

  // Simulate a 10s idle pause where NeedsBeginFrames was off (`seq` jumps
  // 2 -> 627). Resuming must not record the 10s idle sleep as a jank.
  SubmitSortedFrame(metrics, 1u, 627u, t0 + base::Milliseconds(10016),
                    FrameInfo::FrameFinalState::kPresentedAll);
  SubmitSortedFrame(metrics, 1u, 628u, t0 + base::Milliseconds(10032),
                    FrameInfo::FrameFinalState::kPresentedAll);

  // A subsequent real dropped frame followed by presentation (32ms delta vs
  // 16ms baseline) must still be recorded as a single 32ms jank.
  SubmitSortedFrame(metrics, 1u, 629u, t0 + base::Milliseconds(10048),
                    FrameInfo::FrameFinalState::kDropped);
  SubmitSortedFrame(metrics, 1u, 630u, t0 + base::Milliseconds(10064),
                    FrameInfo::FrameFinalState::kPresentedAll);

  metrics.ReportMetrics();
  EXPECT_EQ(reported.frames_expected_v3, 6u);
  EXPECT_EQ(reported.frames_dropped_v3, 1u);
  EXPECT_EQ(reported.jank_count_v3, 1u);
  ASSERT_EQ(reported.janks.size(), 1u);
  EXPECT_EQ(reported.janks[0].duration, base::Milliseconds(32));
}

TEST_F(FrameSequenceMetricsTest, ThrottledFramesPreserveJankV3Baseline) {
  FrameSequenceMetrics metrics(FrameSequenceTrackerType::kCustom);
  FrameSequenceMetrics::CustomReportData reported;
  metrics.SetCustomReporter(base::BindLambdaForTesting(
      [&](const FrameSequenceMetrics::CustomReportData& data) {
        reported = data;
      }));

  const base::TimeTicks t0 = base::TimeTicks() + base::Seconds(1);
  // Steady 30fps throttled sequence (`frames_throttled_since_last = 1`).
  SubmitSortedFrame(metrics, 1u, 1u, t0,
                    FrameInfo::FrameFinalState::kPresentedAll);
  SubmitSortedFrame(metrics, 1u, 3u, t0 + base::Milliseconds(32),
                    FrameInfo::FrameFinalState::kPresentedAll,
                    /*frames_throttled_since_last=*/1u);
  // Late presentation (48ms > 32ms + 8ms) must still trigger a jank.
  SubmitSortedFrame(metrics, 1u, 5u, t0 + base::Milliseconds(80),
                    FrameInfo::FrameFinalState::kPresentedAll,
                    /*frames_throttled_since_last=*/1u);

  metrics.ReportMetrics();
  EXPECT_EQ(reported.jank_count_v3, 1u);
  ASSERT_EQ(reported.janks.size(), 1u);
  EXPECT_EQ(reported.janks[0].duration, base::Milliseconds(48));
}

TEST_F(FrameSequenceMetricsTest,
       PausedBeginFramesPreserveLastPresentedCheckerboarding) {
  FrameSequenceMetrics metrics(FrameSequenceTrackerType::kMainThreadAnimation);
  const base::TimeTicks t0 = base::TimeTicks() + base::Seconds(1);

  SubmitSortedFrame(metrics, 1u, 1u, t0,
                    FrameInfo::FrameFinalState::kPresentedAll,
                    /*frames_throttled_since_last=*/0u,
                    /*checkerboarded_needs_raster=*/true);
  // After an idle pause, a dropped frame keeps showing frame 1 on screen, so
  // checkerboarding must still be attributed from `last_presented_frame`.
  SubmitSortedFrame(metrics, 1u, 200u, t0 + base::Seconds(5),
                    FrameInfo::FrameFinalState::kDropped);

  EXPECT_EQ(FramesMissingContentV3(metrics), 2u);
}

}  // namespace cc
