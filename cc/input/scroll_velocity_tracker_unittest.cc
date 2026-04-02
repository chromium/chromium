// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_velocity_tracker.h"

#include <array>
#include <limits>
#include <utility>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

class ScrollVelocityTrackerTest : public testing::Test {
 public:
  ScrollVelocityTrackerTest() : velocity_tracker_(kWindowDelta) {}

 protected:
  void RecordSample(base::TimeDelta t, const gfx::Vector2dF& scroll_delta) {
    velocity_tracker_.AddSample(kTestStartTime + t, scroll_delta);
  }

  gfx::Vector2dF CurrentVelocity() const {
    return velocity_tracker_.CurrentVelocity();
  }

  void ResetSamples() { velocity_tracker_.Reset(); }

 private:
  static constexpr base::TimeTicks kTestStartTime =
      base::TimeTicks() + base::Seconds(1);
  static constexpr base::TimeDelta kWindowDelta = base::Milliseconds(10);
  ScrollVelocityTracker velocity_tracker_;
};

TEST_F(ScrollVelocityTrackerTest, VelocityIsZeroWhenNoSamples) {
  EXPECT_TRUE(CurrentVelocity().IsZero());
}

TEST_F(ScrollVelocityTrackerTest, VelocityForSingleSample) {
  // When there is only one sample, velocity is computed based on the time
  // window.
  RecordSample(base::Milliseconds(0), gfx::Vector2dF(10.f, 20.f));
  EXPECT_EQ(CurrentVelocity(), gfx::Vector2dF(1.f, 2.f));
}

TEST_F(ScrollVelocityTrackerTest, OldSamplesDiscarded) {
  RecordSample(base::Milliseconds(0), gfx::Vector2dF(0.f, 10.f));
  RecordSample(base::Milliseconds(5), gfx::Vector2dF(10.f, 30.f));
  EXPECT_EQ(CurrentVelocity(), gfx::Vector2dF(2.f, 8.f));

  // Samples within window size (10ms) from the latest sample are not discarded.
  RecordSample(base::Milliseconds(6), gfx::Vector2dF(8.f, 20.f));
  EXPECT_EQ(CurrentVelocity(), gfx::Vector2dF(3.f, 10.f));

  // Samples older than window size (10ms) from the latest sample are discarded.
  RecordSample(base::Milliseconds(11), gfx::Vector2dF(-6.f, 40.f));
  EXPECT_EQ(CurrentVelocity(), gfx::Vector2dF(2.f, 15.f));

  // Samples exactly window size (10ms) old are not discarded.
  RecordSample(base::Milliseconds(16), gfx::Vector2dF(8.f, -15.f));
  EXPECT_EQ(CurrentVelocity(), gfx::Vector2dF(1.f, 4.5f));
}

TEST_F(ScrollVelocityTrackerTest, SamplesWithSameTimestampAreCoalesced) {
  RecordSample(base::Milliseconds(2), gfx::Vector2dF(10.f, 0.f));
  RecordSample(base::Milliseconds(2), gfx::Vector2dF(20.f, 0.f));
  EXPECT_EQ(CurrentVelocity(), gfx::Vector2dF(3.f, 0.f));

  RecordSample(base::Milliseconds(4), gfx::Vector2dF(20.f, 0.f));
  EXPECT_EQ(CurrentVelocity(), gfx::Vector2dF(25.f, 0.f));
}

}  // namespace

}  // namespace cc
