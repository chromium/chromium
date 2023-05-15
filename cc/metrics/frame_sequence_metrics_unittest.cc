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
  EXPECT_FALSE(first.HasEnoughDataForReporting());

  // Create a second metric with too few frames to report any metrics.
  auto second = std::make_unique<FrameSequenceMetrics>(
      FrameSequenceTrackerType::kTouchScroll);
  second->impl_throughput().frames_expected = 90;
  second->impl_throughput().frames_produced = 60;
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

  auto second = std::make_unique<FrameSequenceMetrics>(
      FrameSequenceTrackerType::kTouchScroll);
  second->SetScrollingThread(FrameInfo::SmoothEffectDrivingThread::kMain);
  second->main_throughput().frames_expected = 50;
  second->main_throughput().frames_produced = 10;

  ASSERT_DEATH_IF_SUPPORTED(first.Merge(std::move(second)), "");
}
#endif  // DCHECK_IS_ON()

}  // namespace cc
