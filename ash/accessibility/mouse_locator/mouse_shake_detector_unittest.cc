// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/mouse_locator/mouse_shake_detector.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

namespace {

// Mirrors the detector's end-of-shake timeout.
constexpr base::TimeDelta kShakeEndTimeout = base::Milliseconds(250);

constexpr float kStrokeDistance = 30.0f;  // DIP (>= 24 DIP minimum).
constexpr base::TimeDelta kStepTime = base::Milliseconds(5);
constexpr float kStepDistance = 5.0f;

class TestObserver : public MouseShakeDetector::Observer {
 public:
  void OnMouseShakeStarted(const gfx::PointF& location_in_screen) override {
    ++started_count_;
    last_started_location_ = location_in_screen;
  }

  void OnMouseShakeEnded() override { ++ended_count_; }

  int started_count() const { return started_count_; }
  int ended_count() const { return ended_count_; }
  const gfx::PointF& last_started_location() const {
    return last_started_location_;
  }

 private:
  int started_count_ = 0;
  int ended_count_ = 0;
  gfx::PointF last_started_location_;
};

}  // namespace

class MouseShakeDetectorTest : public testing::Test {
 protected:
  void SetUp() override { observation_.Observe(&detector_); }

  // Moves to `location` after advancing the mock clock by `time_step`.
  void MoveTo(const gfx::PointF& location, base::TimeDelta time_step) {
    task_environment_.FastForwardBy(time_step);
    detector_.ProcessPosition(location, task_environment_.NowTicks());
  }

  // Shakes horizontally, continuing from the previous call, so that the
  // detector confirms `reversal_count` more reversals. Each stroke covers
  // `stroke_distance` in steps of `step_distance` every `time_step`.
  void Shake(int reversal_count,
             base::TimeDelta time_step = kStepTime,
             float step_distance = kStepDistance,
             float stroke_distance = kStrokeDistance) {
    int strokes = reversal_count;
    if (!shaking_started_) {
      // Anchor plus one initial forward stroke, which is not a reversal.
      detector_.ProcessPosition(position_, task_environment_.NowTicks());
      shaking_started_ = true;
      ++strokes;
    }
    for (int s = 0; s < strokes; ++s) {
      float distance_moved = 0.0f;
      while (distance_moved < stroke_distance) {
        const float move =
            std::min(step_distance, stroke_distance - distance_moved);
        position_.set_x(position_.x() + direction_ * move);
        distance_moved += move;
        MoveTo(position_, time_step);
      }
      direction_ = -direction_;
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MouseShakeDetector detector_;
  TestObserver observer_;
  base::ScopedObservation<MouseShakeDetector, MouseShakeDetector::Observer>
      observation_{&observer_};

 private:
  gfx::PointF position_{100.0f, 100.0f};
  float direction_ = 1.0f;
  bool shaking_started_ = false;
};

// A 1000Hz device with small steps starts a shake on the 10th reversal.
TEST_F(MouseShakeDetectorTest, PollingRateInvariance_1000Hz) {
  Shake(10, base::Milliseconds(1), 2.0f);
  EXPECT_EQ(observer_.started_count(), 1);
  EXPECT_TRUE(detector_.is_shaking());
}

// A 125Hz device with large steps behaves the same for the same movement.
TEST_F(MouseShakeDetectorTest, PollingRateInvariance_125Hz) {
  Shake(10, base::Milliseconds(8), 10.0f);
  EXPECT_EQ(observer_.started_count(), 1);
  EXPECT_TRUE(detector_.is_shaking());
}

TEST_F(MouseShakeDetectorTest, NineReversalsDoNotStartShake) {
  Shake(9);
  EXPECT_EQ(observer_.started_count(), 0);
  EXPECT_FALSE(detector_.is_shaking());

  Shake(1);
  EXPECT_EQ(observer_.started_count(), 1);
}

// Continuous monotonic diagonal sweeping does not produce reversals.
TEST_F(MouseShakeDetectorTest, RejectDiagonalSweeping) {
  gfx::PointF position(50.0f, 50.0f);
  detector_.ProcessPosition(position, task_environment_.NowTicks());
  for (int i = 0; i < 100; ++i) {
    position.Offset(5.0f, 5.0f);
    MoveTo(position, base::Milliseconds(10));
  }
  EXPECT_EQ(observer_.started_count(), 0);
}

// Continuous circular motion never enters the 120 degree reversal cone.
TEST_F(MouseShakeDetectorTest, RejectCircularMotion) {
  constexpr float kRadius = 25.0f;
  constexpr gfx::PointF kCenter(200.0f, 200.0f);
  for (int step = 0; step < 120; ++step) {
    const float angle = (2.0f * std::numbers::pi_v<float> * step) / 40.0f;
    MoveTo(gfx::PointF(kCenter.x() + kRadius * std::cos(angle),
                       kCenter.y() + kRadius * std::sin(angle)),
           base::Milliseconds(10));
  }
  EXPECT_EQ(observer_.started_count(), 0);
}

// Reversal evidence older than the 1000ms window is discarded.
TEST_F(MouseShakeDetectorTest, StaleReversalsArePruned) {
  Shake(8);
  task_environment_.FastForwardBy(base::Milliseconds(1100));

  // 8 + 3 would start a shake if the stale reversals were kept. The first
  // stroke after the pause only re-anchors, so it is not a reversal.
  Shake(4);
  EXPECT_EQ(observer_.started_count(), 0);
}

TEST_F(MouseShakeDetectorTest, ShakeEndsWhenReversalsStop) {
  Shake(10);
  ASSERT_EQ(observer_.started_count(), 1);

  // The tail of the 10th stroke already used ~20ms of the timeout.
  task_environment_.FastForwardBy(kShakeEndTimeout - base::Milliseconds(50));
  EXPECT_EQ(observer_.ended_count(), 0);
  EXPECT_TRUE(detector_.is_shaking());

  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(observer_.ended_count(), 1);
  EXPECT_FALSE(detector_.is_shaking());
}

TEST_F(MouseShakeDetectorTest, ContinuedShakePostponesEnd) {
  Shake(10);
  ASSERT_EQ(observer_.started_count(), 1);

  // ~1.2s of continued shaking, far longer than the timeout.
  Shake(40);
  EXPECT_EQ(observer_.started_count(), 1);
  EXPECT_EQ(observer_.ended_count(), 0);
  EXPECT_TRUE(detector_.is_shaking());

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(observer_.started_count(), 1);
  EXPECT_EQ(observer_.ended_count(), 1);
}

// Reversals spaced further apart than the timeout do not sustain a shake.
TEST_F(MouseShakeDetectorTest, SlowReversalsDoNotSustainShake) {
  Shake(10);
  ASSERT_EQ(observer_.started_count(), 1);

  // The next reversal is confirmed on the 2nd step of the stroke, ~320ms
  // after the previous one with 150ms steps, so the shake has already ended.
  Shake(1, base::Milliseconds(150));
  EXPECT_EQ(observer_.ended_count(), 1);
}

// After a shake ends, a new shake needs a full set of fresh reversals.
TEST_F(MouseShakeDetectorTest, NewShakeAfterEndNeedsFullReversals) {
  Shake(10);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  ASSERT_EQ(observer_.ended_count(), 1);

  // Ending the shake resets the stroke, so the first stroke only re-anchors
  // and the next 9 are reversals.
  Shake(10);
  EXPECT_EQ(observer_.started_count(), 1);

  Shake(1);
  EXPECT_EQ(observer_.started_count(), 2);
  EXPECT_TRUE(detector_.is_shaking());
}

}  // namespace ash
