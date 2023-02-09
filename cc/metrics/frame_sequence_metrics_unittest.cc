// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "cc/trees/ukm_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(FrameSequenceMetricsTest, MergeMetrics) {
  // Create a metric with only a small number of frames. It shouldn't report any
  // metrics.
  FrameSequenceMetrics first(FrameSequenceTrackerType::kTouchScroll);
  first.impl_throughput().frames_expected = 20;
  first.impl_throughput().frames_produced = 10;
  first.impl_throughput().frames_ontime = 5;
  EXPECT_FALSE(first.HasEnoughDataForReporting());

  // Create a second metric with too few frames to report any metrics.
  auto second = std::make_unique<FrameSequenceMetrics>(
      FrameSequenceTrackerType::kTouchScroll);
  second->impl_throughput().frames_expected = 90;
  second->impl_throughput().frames_produced = 60;
  second->impl_throughput().frames_ontime = 50;
  EXPECT_FALSE(second->HasEnoughDataForReporting());

  // Merge the two metrics. The result should have enough frames to report
  // metrics.
  first.Merge(std::move(second));
  EXPECT_TRUE(first.HasEnoughDataForReporting());
}

#if DCHECK_IS_ON()
TEST(FrameSequenceMetricsTest, ScrollingThreadMergeMetrics) {
  FrameSequenceMetrics first(FrameSequenceTrackerType::kTouchScroll);
  first.SetScrollingThread(FrameInfo::SmoothEffectDrivingThread::kCompositor);
  first.impl_throughput().frames_expected = 20;
  first.impl_throughput().frames_produced = 10;
  first.impl_throughput().frames_ontime = 10;

  auto second = std::make_unique<FrameSequenceMetrics>(
      FrameSequenceTrackerType::kTouchScroll);
  second->SetScrollingThread(FrameInfo::SmoothEffectDrivingThread::kMain);
  second->main_throughput().frames_expected = 50;
  second->main_throughput().frames_produced = 10;
  second->main_throughput().frames_ontime = 10;

  ASSERT_DEATH_IF_SUPPORTED(first.Merge(std::move(second)), "");
}
#endif  // DCHECK_IS_ON()

TEST(FrameSequenceMetricsTest, AllMetricsReported) {
  base::HistogramTester histograms;

  // Create a metric with enough frames on impl to be reported, but not enough
  // on main.
  FrameSequenceMetrics first(FrameSequenceTrackerType::kTouchScroll);
  first.impl_throughput().frames_expected = 120;
  first.impl_throughput().frames_produced = 80;
  first.impl_throughput().frames_ontime = 60;
  first.main_throughput().frames_expected = 20;
  first.main_throughput().frames_produced = 10;
  first.main_throughput().frames_ontime = 5;
  EXPECT_TRUE(first.HasEnoughDataForReporting());
  first.ReportMetrics();

  // The compositor-thread metric should be reported, but not the main-thread
  // metric.
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.PercentMissedDeadlineFrames.CompositorThread."
      "TouchScroll",
      1u);
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.PercentMissedDeadlineFrames.MainThread.TouchScroll",
      0u);

  // There should still be data left over for the main-thread.
  EXPECT_TRUE(first.HasDataLeftForReporting());

  auto second = std::make_unique<FrameSequenceMetrics>(
      FrameSequenceTrackerType::kTouchScroll);
  second->impl_throughput().frames_expected = 110;
  second->impl_throughput().frames_produced = 100;
  second->impl_throughput().frames_ontime = 80;
  second->main_throughput().frames_expected = 90;
  second->main_throughput().frames_ontime = 70;
  first.Merge(std::move(second));
  EXPECT_TRUE(first.HasEnoughDataForReporting());
  first.ReportMetrics();
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.PercentMissedDeadlineFrames.CompositorThread."
      "TouchScroll",
      2u);
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.PercentMissedDeadlineFrames.MainThread.TouchScroll",
      1u);
  // All the metrics have now been reported. No data should be left over.
  EXPECT_FALSE(first.HasDataLeftForReporting());
}

TEST(FrameSequenceMetricsTest, IrrelevantMetricsNotReported) {
  base::HistogramTester histograms;

  // Create a metric with enough frames on impl to be reported, but not enough
  // on main.
  FrameSequenceMetrics first(FrameSequenceTrackerType::kCompositorAnimation);
  first.impl_throughput().frames_expected = 120;
  first.impl_throughput().frames_produced = 80;
  first.impl_throughput().frames_ontime = 70;
  first.main_throughput().frames_expected = 120;
  first.main_throughput().frames_produced = 80;
  first.main_throughput().frames_ontime = 70;
  EXPECT_TRUE(first.HasEnoughDataForReporting());
  first.ReportMetrics();

  // The compositor-thread metric should be reported, but not the main-thread
  // or slower-thread metric.
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.PercentMissedDeadlineFrames.CompositorThread."
      "CompositorAnimation",
      1u);
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.PercentMissedDeadlineFrames.MainThread."
      "CompositorAnimation",
      0u);
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.PercentMissedDeadlineFrames.SlowerThread."
      "CompositorAnimation",
      0u);

  // Not reported, but the data should be reset.
  EXPECT_EQ(first.impl_throughput().frames_expected, 0u);
  EXPECT_EQ(first.impl_throughput().frames_produced, 0u);
  EXPECT_EQ(first.impl_throughput().frames_ontime, 0u);
  EXPECT_EQ(first.main_throughput().frames_expected, 0u);
  EXPECT_EQ(first.main_throughput().frames_produced, 0u);
  EXPECT_EQ(first.main_throughput().frames_ontime, 0u);

  FrameSequenceMetrics second(FrameSequenceTrackerType::kRAF);
  second.impl_throughput().frames_expected = 120;
  second.impl_throughput().frames_produced = 80;
  second.impl_throughput().frames_ontime = 70;
  second.main_throughput().frames_expected = 120;
  second.main_throughput().frames_produced = 80;
  second.main_throughput().frames_ontime = 70;
  EXPECT_TRUE(second.HasEnoughDataForReporting());
  second.ReportMetrics();

  // The main-thread metric should be reported, but not the compositor-thread
  // or slower-thread metric.
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.PercentMissedDeadlineFrames.CompositorThread.RAF",
      0u);
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.PercentMissedDeadlineFrames.MainThread.RAF", 1u);
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.PercentMissedDeadlineFrames.SlowerThread.RAF", 0u);
}

}  // namespace cc
