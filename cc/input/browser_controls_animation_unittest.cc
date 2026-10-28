// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_animation.h"

#include <optional>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/tween.h"

namespace cc {
namespace {

using Direction = BrowserControlsAnimation::Direction;

TEST(BrowserControlsAnimationTest, UninitializedTickAndResetReturnNullopt) {
  BrowserControlsAnimation animation;
  EXPECT_FALSE(animation.IsInitialized());
  EXPECT_EQ(Direction::kNoAnimation, animation.direction());
  EXPECT_EQ(std::nullopt, animation.Tick(base::TimeTicks()));
  EXPECT_EQ(std::nullopt, animation.Reset());
}

TEST(BrowserControlsAnimationTest, ShowingAnimationInterpolatesAndCompletes) {
  BrowserControlsAnimation animation;
  animation.Initialize(Direction::kShowingControls, /*start_value=*/0.0f,
                       /*stop_value=*/1.0f, base::Milliseconds(200),
                       /*jump_to_end_on_reset=*/false, gfx::Tween::LINEAR);
  EXPECT_TRUE(animation.IsInitialized());
  EXPECT_EQ(Direction::kShowingControls, animation.direction());
  EXPECT_FLOAT_EQ(1.0f, animation.FinalValue());

  const base::TimeTicks start_time =
      base::TimeTicks() + base::Milliseconds(1000);
  EXPECT_FLOAT_EQ(0.0f, *animation.Tick(start_time));
  EXPECT_TRUE(animation.IsInitialized());

  EXPECT_FLOAT_EQ(0.5f, *animation.Tick(start_time + base::Milliseconds(100)));
  EXPECT_TRUE(animation.IsInitialized());

  EXPECT_FLOAT_EQ(1.0f, *animation.Tick(start_time + base::Milliseconds(200)));
  EXPECT_FALSE(animation.IsInitialized());
  EXPECT_EQ(Direction::kNoAnimation, animation.direction());
}

TEST(BrowserControlsAnimationTest, CompletesEarlyAndClampsAtBounds) {
  BrowserControlsAnimation animation;
  // Start at 0.5 and animate toward -0.5 over 200ms, bounded to [0.25, 1.0].
  // The animation should reach the 0.25 lower bound after 50ms and finish.
  animation.Initialize(Direction::kHidingControls, /*start_value=*/0.5f,
                       /*stop_value=*/-0.5f, base::Milliseconds(200),
                       /*jump_to_end_on_reset=*/false, gfx::Tween::LINEAR);
  animation.SetBounds(0.25f, 1.0f);
  EXPECT_FLOAT_EQ(0.25f, animation.FinalValue());

  const base::TimeTicks start_time =
      base::TimeTicks() + base::Milliseconds(1000);
  EXPECT_FLOAT_EQ(0.5f, *animation.Tick(start_time));
  EXPECT_FLOAT_EQ(0.375f, *animation.Tick(start_time + base::Milliseconds(25)));
  EXPECT_TRUE(animation.IsInitialized());

  EXPECT_FLOAT_EQ(0.25f, *animation.Tick(start_time + base::Milliseconds(50)));
  EXPECT_FALSE(animation.IsInitialized());
}

TEST(BrowserControlsAnimationTest, ResetRespectsJumpToEnd) {
  BrowserControlsAnimation no_jump;
  no_jump.Initialize(Direction::kShowingControls, /*start_value=*/0.2f,
                     /*stop_value=*/1.2f, base::Milliseconds(200),
                     /*jump_to_end_on_reset=*/false, gfx::Tween::LINEAR);
  no_jump.SetBounds(0.0f, 1.0f);
  EXPECT_FALSE(no_jump.jump_to_end_on_reset());
  EXPECT_EQ(std::nullopt, no_jump.Reset());
  EXPECT_FALSE(no_jump.IsInitialized());

  BrowserControlsAnimation jump_to_end;
  jump_to_end.Initialize(Direction::kShowingControls, /*start_value=*/0.2f,
                         /*stop_value=*/1.2f, base::Milliseconds(200),
                         /*jump_to_end_on_reset=*/true, gfx::Tween::LINEAR);
  jump_to_end.SetBounds(0.0f, 1.0f);
  EXPECT_TRUE(jump_to_end.jump_to_end_on_reset());
  std::optional<float> final_value = jump_to_end.Reset();
  ASSERT_TRUE(final_value.has_value());
  EXPECT_FLOAT_EQ(1.0f, *final_value);
  EXPECT_FALSE(jump_to_end.IsInitialized());
}

}  // namespace
}  // namespace cc
