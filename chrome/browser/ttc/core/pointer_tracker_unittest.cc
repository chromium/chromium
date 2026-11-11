// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_tracker.h"

#include <algorithm>
#include <vector>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace ttc {
namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr base::TimeTicks kStart = base::TimeTicks() + base::Minutes(1);

// The dwell radius: a dwell's centroid is within this of the resting spot.
constexpr float kRadius = 12.0f;

base::TimeTicks At(base::TimeDelta offset) {
  return kStart + offset;
}

// Records a straight move from `from` to `to` with a sample every `step`,
// starting at `start` and ending exactly at `start + duration` on `to`.
void Move(PointerTracker& tracker,
          base::TimeDelta start,
          base::TimeDelta duration,
          const gfx::PointF& from,
          const gfx::PointF& to,
          base::TimeDelta step = base::Milliseconds(16)) {
  for (base::TimeDelta t;; t += step) {
    t = std::min(t, duration);
    float fraction = duration.is_zero() ? 1.0f : t / duration;
    gfx::PointF point(from.x() + (to.x() - from.x()) * fraction,
                      from.y() + (to.y() - from.y()) * fraction);
    tracker.RecordSample(At(start + t), point);
    if (t >= duration) {
      break;
    }
  }
}

TEST(PointerTrackerTest, EmptyTrackerHasNoSamples) {
  PointerTracker tracker;
  EXPECT_THAT(
      tracker.ResolveLocations(At(base::Seconds(4)), At(base::Seconds(5))),
      IsEmpty());
}

// A pointer that stopped recently is dwelling where it stopped.
TEST(PointerTrackerTest, RecentSampleIsReturned) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(10, 10));
  tracker.RecordSample(At(base::Seconds(1)), gfx::PointF(300, 200));
  std::vector<PointerTracker::Location> results =
      tracker.ResolveLocations(At(base::Seconds(0)), At(base::Seconds(3)));

  ASSERT_THAT(results, SizeIs(1));
  EXPECT_POINTF_EQ(gfx::PointF(300, 200), results[0].point);
  EXPECT_EQ(results[0].time, At(base::Seconds(1)));
  EXPECT_FALSE(results[0].is_press);
  EXPECT_TRUE(results[0].is_dwell);
}

TEST(PointerTrackerTest, PressIsReported) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(10, 10),
                       /*is_press=*/true);
  std::vector<PointerTracker::Location> results =
      tracker.ResolveLocations(At(base::Seconds(0)), At(base::Seconds(1)));

  ASSERT_THAT(results, SizeIs(1));
  EXPECT_TRUE(results[0].is_press);
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
  EXPECT_THAT(
      tracker.ResolveLocations(At(base::Seconds(19)), At(base::Seconds(20))),
      IsEmpty());
  // A press at the same point is a gesture and counts.
  tracker.RecordSample(At(base::Seconds(21)), gfx::PointF(10, 10),
                       /*is_press=*/true);
  EXPECT_THAT(
      tracker.ResolveLocations(At(base::Seconds(21)), At(base::Seconds(21))),
      SizeIs(1));
}

// A pointer that had not moved for longer than the idle limit when the turn's
// window opened is parked, not pointing: nothing is returned so no image is
// captured. How long the turn has lasted makes no difference.
TEST(PointerTrackerTest, ParkedPointerHasNoSamples) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(10, 10));
  // The turn starts at 17 s, so its window opens 16 s after the pointer
  // arrived; the idle limit is 15 s.
  EXPECT_THAT(
      tracker.ResolveLocations(At(base::Seconds(17)), At(base::Seconds(18))),
      IsEmpty());
  // History is 20 s, so the sample is still kept.
  EXPECT_EQ(tracker.sample_count_for_testing(), 1u);
  // A window opening right at the limit still counts, whether the tool is
  // called right away or 3 s into the turn.
  EXPECT_THAT(
      tracker.ResolveLocations(At(base::Seconds(16)), At(base::Seconds(16))),
      SizeIs(1));
  EXPECT_THAT(
      tracker.ResolveLocations(At(base::Seconds(16)), At(base::Seconds(19))),
      SizeIs(1));
}

// Samples older than the history as of now are only still there because
// nothing has been recorded since; resolving prunes them.
TEST(PointerTrackerTest, SamplesOlderThanHistoryArePruned) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(10, 10));
  // The window opens at 9 s, within the idle limit, but the tool is called
  // 21 s after the sample.
  EXPECT_THAT(
      tracker.ResolveLocations(At(base::Seconds(10)), At(base::Seconds(21))),
      IsEmpty());
  EXPECT_EQ(tracker.sample_count_for_testing(), 0u);
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
}

TEST(PointerTrackerTest, SampleCountIsCapped) {
  PointerTracker::Config config;
  config.max_samples = 10;
  PointerTracker tracker(config);
  for (int i = 0; i < 50; ++i) {
    tracker.RecordSample(At(base::Milliseconds(16 * i)), gfx::PointF(i, 0));
  }
  EXPECT_EQ(tracker.sample_count_for_testing(), 10u);
  // The most recent samples are the ones kept.
  std::vector<PointerTracker::Location> results = tracker.ResolveLocations(
      At(base::Seconds(0)), At(base::Milliseconds(800)));
  ASSERT_THAT(results, SizeIs(1));
  EXPECT_POINTF_EQ(gfx::PointF(49, 0), results[0].point);
}

// Moves that arrive faster than the sampling interval are dropped; presses
// never are.
TEST(PointerTrackerTest, MovesWithinMinIntervalAreDropped) {
  PointerTracker tracker;
  for (int ms : {0, 5, 10, 16, 20, 32}) {
    tracker.RecordSample(At(base::Milliseconds(ms)), gfx::PointF(ms, 0));
  }
  EXPECT_EQ(tracker.sample_count_for_testing(), 3u);
  tracker.RecordSample(At(base::Milliseconds(33)), gfx::PointF(33, 0),
                       /*is_press=*/true);
  EXPECT_EQ(tracker.sample_count_for_testing(), 4u);
}

TEST(PointerTrackerTest, OutOfOrderSampleTakesTheLastSamplesTime) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(2)), gfx::PointF(1, 1));
  // A press, so that it is kept despite being within the sampling interval of
  // the (clamped) previous time.
  tracker.RecordSample(At(base::Seconds(1)), gfx::PointF(500, 500),
                       /*is_press=*/true);
  std::vector<PointerTracker::Location> results =
      tracker.ResolveLocations(At(base::Seconds(0)), At(base::Seconds(3)));
  ASSERT_THAT(results, SizeIs(1));
  EXPECT_POINTF_EQ(gfx::PointF(500, 500), results[0].point);
  EXPECT_EQ(results[0].time, At(base::Seconds(2)));
}

TEST(PointerTrackerTest, ClearForgetsEverything) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(1, 1));
  tracker.Clear();
  EXPECT_EQ(tracker.sample_count_for_testing(), 0u);
  EXPECT_THAT(
      tracker.ResolveLocations(At(base::Seconds(0)), At(base::Seconds(1))),
      IsEmpty());
}

// The common case: the user moves to a spot, stops, and speaks. No samples
// arrive while the pointer is still, so the dwell is inferred from the gap.
TEST(PointerTrackerTest, HoverThenSpeakIsADwell) {
  PointerTracker tracker;
  Move(tracker, base::Seconds(0), base::Milliseconds(500), gfx::PointF(0, 0),
       gfx::PointF(300, 200));
  // Pointer rests at (300, 200). The user starts speaking 1 s later and the
  // tool is called 2 s after that.
  std::vector<PointerTracker::Location> results = tracker.ResolveLocations(
      At(base::Milliseconds(1500)), At(base::Milliseconds(3500)));

  ASSERT_THAT(results, SizeIs(1));
  EXPECT_POINTF_NEAR(gfx::PointF(300, 200), results[0].point, kRadius);
  // The run began with the last few samples of the approach.
  EXPECT_LE(results[0].time, At(base::Milliseconds(500)));
  EXPECT_GE(results[0].time, At(base::Milliseconds(400)));
  EXPECT_TRUE(results[0].is_dwell);
  EXPECT_FALSE(results[0].is_press);
}

// A pointer that stopped well before the turn is still returned if it
// arrived within the idle limit, even though no sample falls in the window.
TEST(PointerTrackerTest, PositionBeforeWindowCountsIfRecent) {
  PointerTracker tracker;
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(10, 10));
  // Turn starts 5 s later; window opens at 4 s, after the only sample.
  std::vector<PointerTracker::Location> results =
      tracker.ResolveLocations(At(base::Seconds(5)), At(base::Seconds(6)));

  ASSERT_THAT(results, SizeIs(1));
  EXPECT_POINTF_EQ(gfx::PointF(10, 10), results[0].point);
  EXPECT_TRUE(results[0].is_dwell);
}

// Movement during the turn with no pause anywhere falls back to the most
// recent position.
TEST(PointerTrackerTest, ContinuousMotionFallsBackToLatestSample) {
  PointerTracker tracker;
  // Turn starts at 1 s; the pointer sweeps across the page from 1 s to 3 s and
  // the tool is called at exactly 3 s, so no run lasts 300 ms.
  Move(tracker, base::Seconds(1), base::Seconds(2), gfx::PointF(0, 0),
       gfx::PointF(1000, 0));
  std::vector<PointerTracker::Location> results =
      tracker.ResolveLocations(At(base::Seconds(1)), At(base::Seconds(3)));

  ASSERT_THAT(results, SizeIs(1));
  EXPECT_POINTF_EQ(gfx::PointF(1000, 0), results[0].point);
  EXPECT_FALSE(results[0].is_dwell);
}

// The most recent dwell comes first; with max_results raised, earlier
// dwells follow it, up to the cap.
TEST(PointerTrackerTest, MultipleResultsAreNewestFirstAndCapped) {
  PointerTracker::Config config;
  config.max_results = 2;
  PointerTracker tracker(config);
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(100, 100));
  Move(tracker, base::Seconds(1), base::Milliseconds(500),
       gfx::PointF(100, 100), gfx::PointF(500, 100));
  // Dwell at B from 1.5 s to 3 s, then move to C and dwell there.
  Move(tracker, base::Seconds(3), base::Milliseconds(500),
       gfx::PointF(500, 100), gfx::PointF(500, 400));
  std::vector<PointerTracker::Location> results =
      tracker.ResolveLocations(At(base::Seconds(0)), At(base::Seconds(5)));

  ASSERT_THAT(results, SizeIs(2));
  EXPECT_POINTF_NEAR(gfx::PointF(500, 400), results[0].point, kRadius);
  EXPECT_POINTF_NEAR(gfx::PointF(500, 100), results[1].point, kRadius);
  EXPECT_TRUE(results[0].is_dwell);
  EXPECT_TRUE(results[1].is_dwell);
}

// A click is the strongest pointing gesture: it counts even if the pointer
// immediately moves on, and a click where the pointer already rests does not
// make a second result for the same spot. A press's own point is returned
// rather than its run's centroid.
TEST(PointerTrackerTest, PressCountsWithoutDwelling) {
  PointerTracker::Config config;
  config.max_results = 3;
  PointerTracker tracker(config);
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(100, 100));
  tracker.RecordSample(At(base::Seconds(1)), gfx::PointF(101, 100),
                       /*is_press=*/true);
  // Move away, click without stopping, keep moving until the tool is called.
  Move(tracker, base::Milliseconds(1500), base::Milliseconds(500),
       gfx::PointF(101, 100), gfx::PointF(500, 300));
  tracker.RecordSample(At(base::Milliseconds(2016)), gfx::PointF(520, 310),
                       /*is_press=*/true);
  Move(tracker, base::Milliseconds(2032), base::Milliseconds(468),
       gfx::PointF(540, 320), gfx::PointF(900, 600));
  std::vector<PointerTracker::Location> results = tracker.ResolveLocations(
      At(base::Milliseconds(0)), At(base::Milliseconds(2500)));

  ASSERT_THAT(results, SizeIs(2));
  EXPECT_POINTF_EQ(gfx::PointF(520, 310), results[0].point);
  EXPECT_TRUE(results[0].is_press);
  EXPECT_TRUE(results[0].is_dwell);
  EXPECT_POINTF_EQ(gfx::PointF(101, 100), results[1].point);
  EXPECT_TRUE(results[1].is_press);
  EXPECT_EQ(results[1].time, At(base::Seconds(0)));
}

// Only the part of a run inside the turn's window counts. A pointer that rested
// (and was clicked) at a spot before the window but moves away right after the
// window opens did not dwell there during the turn.
TEST(PointerTrackerTest, RestBeforeWindowOnlyCountsInsideWindow) {
  PointerTracker::Config config;
  config.max_results = 3;
  PointerTracker tracker(config);
  // Clicked at A well before the turn, e.g. during the previous turn.
  tracker.RecordSample(At(base::Seconds(0)), gfx::PointF(100, 100),
                       /*is_press=*/true);
  // The turn starts at 5 s, so the window opens at 4 s. The pointer leaves A
  // 100 ms later and keeps moving until the tool is called.
  Move(tracker, base::Milliseconds(4100), base::Milliseconds(900),
       gfx::PointF(100, 100), gfx::PointF(900, 100));
  std::vector<PointerTracker::Location> results =
      tracker.ResolveLocations(At(base::Seconds(5)), At(base::Seconds(5)));

  // A is neither a dwell nor a press for this turn, so the fallback applies.
  ASSERT_THAT(results, SizeIs(1));
  EXPECT_POINTF_NEAR(gfx::PointF(900, 100), results[0].point, kRadius);
  EXPECT_FALSE(results[0].is_dwell);
  EXPECT_FALSE(results[0].is_press);
}

}  // namespace
}  // namespace ttc
