// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_sequence_tracker.h"

#include <memory>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {
namespace {

constexpr base::TimeTicks MillisecondsTicks(int ms) {
  return base::TimeTicks() + base::Milliseconds(ms);
}

class ScrollSequenceTrackerTest : public testing::Test {
 protected:
  ScrollSequenceTracker tracker_;
};

TEST_F(ScrollSequenceTrackerTest, InitialState) {
  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(), base::TimeTicks());
  EXPECT_FALSE(tracker_.has_seen_scroll_update_after_begin());
}

TEST_F(ScrollSequenceTrackerTest, ScrollBegin) {
  base::TimeTicks scroll_begin_arrival_timestamp = MillisecondsTicks(30);
  {
    base::SimpleTestTickClock tick_clock;
    tick_clock.SetNowTicks(scroll_begin_arrival_timestamp);
    std::unique_ptr<ScrollEventMetrics> metrics =
        ScrollEventMetrics::CreateForTesting(
            ui::EventType::kGestureScrollBegin,
            ui::ScrollInputType::kTouchscreen,
            /*is_inertial=*/false, /*timestamp=*/MillisecondsTicks(10),
            /*arrived_in_browser_main_timestamp=*/MillisecondsTicks(20),
            &tick_clock, /*scroll_begin_arrival_timestamp=*/base::TimeTicks());
    ASSERT_NE(metrics, nullptr);

    // We advance the clock here to verify that `OnScrollBegin()` call below
    // indeed uses the timestamp of the `ScrollEventMetrics::CreateForTesting()`
    // call above. creation of `metrics` above.
    tick_clock.Advance(base::Milliseconds(10));

    tracker_.OnScrollBegin(metrics.get());
  }

  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(),
            scroll_begin_arrival_timestamp);
  EXPECT_FALSE(tracker_.has_seen_scroll_update_after_begin());

  tracker_.OnScrollUpdate();
  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(),
            scroll_begin_arrival_timestamp);
  EXPECT_TRUE(tracker_.has_seen_scroll_update_after_begin());

  tracker_.OnScrollUpdate();
  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(),
            scroll_begin_arrival_timestamp);
  EXPECT_TRUE(tracker_.has_seen_scroll_update_after_begin());
}

TEST_F(ScrollSequenceTrackerTest, ScrollBeginWithNullMetrics) {
  base::TimeTicks before = base::TimeTicks::Now();
  tracker_.OnScrollBegin(nullptr);
  base::TimeTicks after = base::TimeTicks::Now();

  base::TimeTicks scroll_begin_arrival_timestamp =
      tracker_.scroll_begin_arrival_timestamp();
  EXPECT_THAT(scroll_begin_arrival_timestamp,
              ::testing::AllOf(::testing::Ge(before), ::testing::Le(after)));
  EXPECT_FALSE(tracker_.has_seen_scroll_update_after_begin());

  tracker_.OnScrollUpdate();
  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(),
            scroll_begin_arrival_timestamp);
  EXPECT_TRUE(tracker_.has_seen_scroll_update_after_begin());

  tracker_.OnScrollUpdate();
  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(),
            scroll_begin_arrival_timestamp);
  EXPECT_TRUE(tracker_.has_seen_scroll_update_after_begin());
}

TEST_F(ScrollSequenceTrackerTest, ScrollUpdateWithoutScrollBegin) {
  tracker_.OnScrollUpdate();
  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(), base::TimeTicks());
  EXPECT_TRUE(tracker_.has_seen_scroll_update_after_begin());

  tracker_.OnScrollUpdate();
  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(), base::TimeTicks());
  EXPECT_TRUE(tracker_.has_seen_scroll_update_after_begin());
}

TEST_F(ScrollSequenceTrackerTest, SequenceBeginUpdateBeginUpdate) {
  tracker_.OnScrollBegin(nullptr);
  tracker_.OnScrollUpdate();
  EXPECT_TRUE(tracker_.has_seen_scroll_update_after_begin());

  // New begin should reset it!
  tracker_.OnScrollBegin(nullptr);
  EXPECT_FALSE(tracker_.has_seen_scroll_update_after_begin());

  tracker_.OnScrollUpdate();
  EXPECT_TRUE(tracker_.has_seen_scroll_update_after_begin());
}

TEST_F(ScrollSequenceTrackerTest, MultipleScrollBegins) {
  {
    base::TimeTicks scroll_begin_arrival_timestamp = MillisecondsTicks(30);
    base::SimpleTestTickClock tick_clock;
    tick_clock.SetNowTicks(scroll_begin_arrival_timestamp);
    std::unique_ptr<ScrollEventMetrics> metrics =
        ScrollEventMetrics::CreateForTesting(
            ui::EventType::kGestureScrollBegin,
            ui::ScrollInputType::kTouchscreen,
            /*is_inertial=*/false, /*timestamp=*/MillisecondsTicks(10),
            /*arrived_in_browser_main_timestamp=*/MillisecondsTicks(20),
            &tick_clock, /*scroll_begin_arrival_timestamp=*/base::TimeTicks());
    ASSERT_NE(metrics, nullptr);

    tracker_.OnScrollBegin(metrics.get());
  }

  tracker_.OnScrollUpdate();
  tracker_.OnScrollUpdate();

  base::TimeTicks scroll_begin_arrival_timestamp2 = MillisecondsTicks(130);
  {
    base::SimpleTestTickClock tick_clock2;
    tick_clock2.SetNowTicks(scroll_begin_arrival_timestamp2);
    std::unique_ptr<ScrollEventMetrics> metrics2 =
        ScrollEventMetrics::CreateForTesting(
            ui::EventType::kGestureScrollBegin,
            ui::ScrollInputType::kTouchscreen,
            /*is_inertial=*/false, /*timestamp=*/MillisecondsTicks(110),
            /*arrived_in_browser_main_timestamp=*/MillisecondsTicks(120),
            &tick_clock2, /*scroll_begin_arrival_timestamp=*/base::TimeTicks());
    ASSERT_NE(metrics2, nullptr);
    tick_clock2.Advance(base::Milliseconds(10));

    tracker_.OnScrollBegin(metrics2.get());
  }

  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(),
            scroll_begin_arrival_timestamp2);
  EXPECT_FALSE(tracker_.has_seen_scroll_update_after_begin());

  tracker_.OnScrollUpdate();
  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(),
            scroll_begin_arrival_timestamp2);
  EXPECT_TRUE(tracker_.has_seen_scroll_update_after_begin());

  tracker_.OnScrollUpdate();
  EXPECT_EQ(tracker_.scroll_begin_arrival_timestamp(),
            scroll_begin_arrival_timestamp2);
  EXPECT_TRUE(tracker_.has_seen_scroll_update_after_begin());
}

}  // namespace
}  // namespace cc
