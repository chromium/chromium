// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_tracker.h"

#include <optional>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace ttc {
namespace {

constexpr base::TimeTicks kStart = base::TimeTicks() + base::Minutes(1);

base::TimeTicks At(base::TimeDelta offset) {
  return kStart + offset;
}

TEST(PointerTrackerTest, EmptyTrackerHasNoSamples) {
  PointerTracker tracker;
  EXPECT_FALSE(tracker.ResolveSample(At(base::Seconds(5))));
}

// A pointer that moved recently is pointing at wherever it is now.
TEST(PointerTrackerTest, RecentSampleIsReturned) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(10, 10));
  tracker.RecordSample(At(base::Seconds(1)), gfx::PointF(300, 200));
  std::optional<PointerTracker::Sample> sample =
      tracker.ResolveSample(At(base::Seconds(3)));

  ASSERT_TRUE(sample);
  EXPECT_POINTF_EQ(gfx::PointF(300, 200), sample->point);
  EXPECT_EQ(sample->time, At(base::Seconds(1)));
  EXPECT_FALSE(sample->is_press);
}

TEST(PointerTrackerTest, PressIsReported) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(10, 10),
                       /*is_press=*/true);
  std::optional<PointerTracker::Sample> sample =
      tracker.ResolveSample(At(base::Seconds(1)));

  ASSERT_TRUE(sample);
  EXPECT_TRUE(sample->is_press);
}

// Aura synthesizes mouse moves at the pointer's current position when content
// changes under it. Those are not movement, so they must not keep a parked
// pointer looking active.
TEST(PointerTrackerTest, MoveToSamePointIsNotMovement) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(10, 10));
  for (int i = 1; i <= 20; ++i) {
    tracker.RecordSample(At(base::Seconds(i)), gfx::PointF(10, 10));
  }
  EXPECT_EQ(tracker.sample_count_for_testing(), 1u);
  // 20 s after the real move the pointer is parked despite the synthesized
  // moves at 1 s..20 s.
  EXPECT_FALSE(tracker.ResolveSample(At(base::Seconds(20))));
  // A press at the same point is a gesture and counts.
  tracker.RecordSample(At(base::Seconds(21)), gfx::PointF(10, 10),
                       /*is_press=*/true);
  EXPECT_TRUE(tracker.ResolveSample(At(base::Seconds(21))));
}

// A pointer that has not moved for longer than the idle limit is parked, not
// pointing: nothing is returned so no image is captured.
TEST(PointerTrackerTest, ParkedPointerHasNoSamples) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(10, 10));
  // Idle limit is 15 s; history is 20 s, so the sample is still kept.
  EXPECT_FALSE(tracker.ResolveSample(At(base::Seconds(18))));
  EXPECT_EQ(tracker.sample_count_for_testing(), 1u);
  // Right at the limit still counts.
  EXPECT_TRUE(tracker.ResolveSample(At(base::Seconds(15))));
}

TEST(PointerTrackerTest, OldSamplesArePruned) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(1, 1));
  tracker.RecordSample(At(base::Seconds(10)), gfx::PointF(2, 2));
  EXPECT_EQ(tracker.sample_count_for_testing(), 2u);
  // History is 20 s. A sample exactly 20 s old is still kept.
  tracker.RecordSample(At(base::Seconds(20)), gfx::PointF(3, 3));
  EXPECT_EQ(tracker.sample_count_for_testing(), 3u);
  // The first two fall out; the sample at 20 s and the new one stay.
  tracker.RecordSample(At(base::Seconds(31)), gfx::PointF(4, 4));
  EXPECT_EQ(tracker.sample_count_for_testing(), 2u);
  std::optional<PointerTracker::Sample> sample =
      tracker.ResolveSample(At(base::Seconds(31)));
  ASSERT_TRUE(sample);
  EXPECT_POINTF_EQ(gfx::PointF(4, 4), sample->point);
}

TEST(PointerTrackerTest, SampleCountIsCapped) {
  PointerTracker::Config config;
  config.max_samples = 10;
  PointerTracker tracker(config);
  for (int i = 0; i < 50; ++i) {
    tracker.RecordSample(At(base::Milliseconds(i)), gfx::PointF(i, 0));
  }
  EXPECT_EQ(tracker.sample_count_for_testing(), 10u);
  // The most recent samples are the ones kept.
  std::optional<PointerTracker::Sample> sample =
      tracker.ResolveSample(At(base::Milliseconds(100)));
  ASSERT_TRUE(sample);
  EXPECT_POINTF_EQ(gfx::PointF(49, 0), sample->point);
}

TEST(PointerTrackerTest, OutOfOrderSampleTakesTheLastSamplesTime) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(2)), gfx::PointF(1, 1));
  tracker.RecordSample(At(base::Seconds(1)), gfx::PointF(500, 500));
  std::optional<PointerTracker::Sample> sample =
      tracker.ResolveSample(At(base::Seconds(3)));
  ASSERT_TRUE(sample);
  EXPECT_POINTF_EQ(gfx::PointF(500, 500), sample->point);
  EXPECT_EQ(sample->time, At(base::Seconds(2)));
}

TEST(PointerTrackerTest, ClearForgetsEverything) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(1, 1));
  tracker.Clear();
  EXPECT_EQ(tracker.sample_count_for_testing(), 0u);
  EXPECT_FALSE(tracker.ResolveSample(At(base::Seconds(1))));
}

}  // namespace
}  // namespace ttc
