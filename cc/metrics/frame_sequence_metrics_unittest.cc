// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_metrics.h"

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "components/ukm/test_ukm_recorder.h"
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

}  // namespace cc
