// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/legacy_shake_detector.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"

namespace glic {

namespace {

class LegacyShakeDetectorTest : public testing::Test {
 protected:
  // Adds a sample `delay` after the previous one.
  bool AddSample(float x,
                 float y,
                 base::TimeDelta delay = base::Milliseconds(16)) {
    now_ += delay;
    return detector_.AddSample(gfx::PointF(x, y), now_);
  }

  // Moves back and forth, returning whether the last move detected a shake.
  // Earlier moves must not detect one.
  bool Shake() {
    for (float x : {0.0f, 20.0f, 0.0f, 20.0f, 0.0f}) {
      EXPECT_FALSE(AddSample(x, 0.0f));
    }
    return AddSample(20.0f, 0.0f);
  }

  LegacyShakeDetector detector_;
  base::TimeTicks now_ = base::TimeTicks() + base::Seconds(1);
};

TEST_F(LegacyShakeDetectorTest, DetectsShake) {
  EXPECT_TRUE(Shake());
}

TEST_F(LegacyShakeDetectorTest, ResetsAfterDetection) {
  EXPECT_TRUE(Shake());
  EXPECT_TRUE(Shake());
}

TEST_F(LegacyShakeDetectorTest, ContinuousMoveIsNotAShake) {
  for (float x = 0.0f; x <= 200.0f; x += 20.0f) {
    EXPECT_FALSE(AddSample(x, 0.0f));
  }
}

TEST_F(LegacyShakeDetectorTest, IgnoresSmallMoves) {
  for (float x : {0.0f, 5.0f, 0.0f, 5.0f, 0.0f, 5.0f, 0.0f, 5.0f, 0.0f}) {
    EXPECT_FALSE(AddSample(x, 0.0f));
  }
}

TEST_F(LegacyShakeDetectorTest, SlowShakeIsNotAShake) {
  EXPECT_FALSE(AddSample(0.0f, 0.0f));
  EXPECT_FALSE(AddSample(20.0f, 0.0f));
  EXPECT_FALSE(AddSample(0.0f, 0.0f));
  EXPECT_FALSE(AddSample(20.0f, 0.0f));
  EXPECT_FALSE(AddSample(0.0f, 0.0f));
  // Too long since the last direction change: starts over.
  EXPECT_FALSE(AddSample(20.0f, 0.0f, base::Milliseconds(1001)));
}

TEST_F(LegacyShakeDetectorTest, ResetClearsProgress) {
  EXPECT_FALSE(AddSample(0.0f, 0.0f));
  EXPECT_FALSE(AddSample(20.0f, 0.0f));
  EXPECT_FALSE(AddSample(0.0f, 0.0f));
  EXPECT_FALSE(AddSample(20.0f, 0.0f));
  EXPECT_FALSE(AddSample(0.0f, 0.0f));
  detector_.Reset();
  EXPECT_TRUE(Shake());
}

}  // namespace

}  // namespace glic
