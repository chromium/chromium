// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_progress_tracker.h"

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/lottie/animation.h"

namespace ash {
namespace {

constexpr float kTimestampEpsilon = .001f;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::FloatEq;
using ::testing::Ge;
using ::testing::Le;
using ::testing::SizeIs;

class AmbientAnimationProgressTrackerTest : public ::testing::Test {
 protected:
  static constexpr gfx::Size kAnimationSize = gfx::Size(100, 100);

  AmbientAnimationProgressTrackerTest()
      : clock_(base::TimeTicks::Now()),
        canvas_(kAnimationSize, /*image_scale=*/1.f, /*is_opaque=*/false) {}

  scoped_refptr<cc::SkottieWrapper> CreateSkottie(int duration_secs) {
    return cc::CreateSkottie(kAnimationSize, duration_secs);
  }

  void Paint(lottie::Animation& animation) {
    animation.Paint(&canvas_, clock_, kAnimationSize);
  }

  base::TimeTicks clock_;
  gfx::Canvas canvas_;
  AmbientAnimationProgressTracker tracker_;
};

TEST_F(AmbientAnimationProgressTrackerTest, SingleAnimation) {
  AmbientAnimationProgressTracker tracker;
  lottie::Animation animation(CreateSkottie(/*duration_secs=*/10));
  tracker.RegisterAnimation(&animation);
  ASSERT_FALSE(tracker.HasActiveAnimations());
  animation.Start();
  Paint(animation);
  ASSERT_TRUE(tracker.HasActiveAnimations());
  AmbientAnimationProgressTracker::ImmutableParams immutable_params =
      tracker.GetImmutableParams();
  AmbientAnimationProgressTracker::Progress global_progress =
      tracker.GetGlobalProgress();
  EXPECT_THAT(global_progress.num_completed_cycles, Eq(0));
  EXPECT_THAT(global_progress.current_timestamp, FloatEq(0.f));
  EXPECT_THAT(immutable_params.total_duration, Eq(base::Seconds(10)));
  ASSERT_THAT(immutable_params.scheduled_cycles, SizeIs(1));
  EXPECT_THAT(immutable_params.scheduled_cycles.front().start_offset,
              Eq(base::TimeDelta()));
  EXPECT_THAT(immutable_params.scheduled_cycles.front().end_offset,
              Eq(base::Seconds(10)));
  EXPECT_THAT(immutable_params.style, Eq(lottie::Animation::Style::kLoop));

  clock_ += base::Seconds(5);
  Paint(animation);
  global_progress = tracker.GetGlobalProgress();
  EXPECT_THAT(global_progress.num_completed_cycles, Eq(0));
  EXPECT_THAT(global_progress.current_timestamp, FloatEq(.5f));

  clock_ += base::Seconds(5);
  Paint(animation);
  global_progress = tracker.GetGlobalProgress();
  EXPECT_THAT(global_progress.num_completed_cycles, Eq(1));
  EXPECT_THAT(global_progress.current_timestamp, FloatEq(0.f));

  clock_ += base::Seconds(5);
  Paint(animation);
  global_progress = tracker.GetGlobalProgress();
  EXPECT_THAT(global_progress.num_completed_cycles, Eq(1));
  EXPECT_THAT(global_progress.current_timestamp, FloatEq(.5f));
}

TEST_F(AmbientAnimationProgressTrackerTest, MultipleAnimations) {
  AmbientAnimationProgressTracker tracker;
  lottie::Animation animation_1(CreateSkottie(/*duration_secs=*/10));
  lottie::Animation animation_2(CreateSkottie(/*duration_secs=*/10));
  tracker.RegisterAnimation(&animation_1);
  tracker.RegisterAnimation(&animation_2);
  ASSERT_FALSE(tracker.HasActiveAnimations());
  animation_1.Start();
  animation_2.Start();
  Paint(animation_1);
  Paint(animation_2);
  ASSERT_TRUE(tracker.HasActiveAnimations());
  AmbientAnimationProgressTracker::ImmutableParams immutable_params =
      tracker.GetImmutableParams();
  AmbientAnimationProgressTracker::Progress global_progress =
      tracker.GetGlobalProgress();
  EXPECT_THAT(global_progress.num_completed_cycles, Eq(0));
  EXPECT_THAT(global_progress.current_timestamp, FloatEq(0.f));
  EXPECT_THAT(immutable_params.total_duration, Eq(base::Seconds(10)));
  ASSERT_THAT(immutable_params.scheduled_cycles, SizeIs(1));
  EXPECT_THAT(immutable_params.scheduled_cycles.front().start_offset,
              Eq(base::TimeDelta()));
  EXPECT_THAT(immutable_params.scheduled_cycles.front().end_offset,
              Eq(base::Seconds(10)));
  EXPECT_THAT(immutable_params.style, Eq(lottie::Animation::Style::kLoop));

  clock_ += base::Seconds(5);
  Paint(animation_1);
  clock_ += base::Milliseconds(100);
  Paint(animation_2);
  global_progress = tracker.GetGlobalProgress();
  EXPECT_THAT(global_progress.num_completed_cycles, Eq(0));
  EXPECT_THAT(global_progress.current_timestamp,
              AllOf(Ge(.5f - kTimestampEpsilon), Le(.51f + kTimestampEpsilon)));

  clock_ += base::Seconds(5) - base::Milliseconds(100);
  Paint(animation_1);
  Paint(animation_2);
  global_progress = tracker.GetGlobalProgress();
  EXPECT_THAT(global_progress.num_completed_cycles, Eq(1));
  EXPECT_THAT(global_progress.current_timestamp, FloatEq(.0f));

  clock_ += base::Seconds(2);
  Paint(animation_2);
  clock_ += base::Seconds(1);
  Paint(animation_1);
  global_progress = tracker.GetGlobalProgress();
  EXPECT_THAT(global_progress.num_completed_cycles, Eq(1));
  EXPECT_THAT(global_progress.current_timestamp,
              AllOf(Ge(.2f - kTimestampEpsilon), Le(.3f + kTimestampEpsilon)));
}

TEST_F(AmbientAnimationProgressTrackerTest, AnimationsDestroyed) {
  AmbientAnimationProgressTracker tracker;
  auto animation_1 =
      std::make_unique<lottie::Animation>(CreateSkottie(/*duration_secs=*/10));
  auto animation_2 =
      std::make_unique<lottie::Animation>(CreateSkottie(/*duration_secs=*/10));
  tracker.RegisterAnimation(animation_1.get());
  tracker.RegisterAnimation(animation_2.get());
  animation_1->Start();
  animation_2->Start();
  Paint(*animation_1);
  Paint(*animation_2);

  clock_ += base::Seconds(5);
  Paint(*animation_1);
  clock_ += base::Milliseconds(100);
  Paint(*animation_2);

  animation_1.reset();
  AmbientAnimationProgressTracker::Progress global_progress =
      tracker.GetGlobalProgress();
  EXPECT_THAT(global_progress.num_completed_cycles, Eq(0));
  EXPECT_THAT(global_progress.current_timestamp, FloatEq(.51f));

  animation_2.reset();
  EXPECT_FALSE(tracker.HasActiveAnimations());

  auto animation_3 =
      std::make_unique<lottie::Animation>(CreateSkottie(/*duration_secs=*/20));
  auto animation_4 =
      std::make_unique<lottie::Animation>(CreateSkottie(/*duration_secs=*/20));
  tracker.RegisterAnimation(animation_3.get());
  tracker.RegisterAnimation(animation_4.get());
  animation_3->Start();
  animation_4->Start();
  Paint(*animation_3);
  Paint(*animation_4);

  clock_ += base::Seconds(5);
  Paint(*animation_3);
  Paint(*animation_4);
  global_progress = tracker.GetGlobalProgress();
  EXPECT_THAT(global_progress.num_completed_cycles, Eq(0));
  EXPECT_THAT(global_progress.current_timestamp, FloatEq(.25f));
}

TEST_F(AmbientAnimationProgressTrackerTest, AnimationRestarted) {
  AmbientAnimationProgressTracker tracker;
  auto animation =
      std::make_unique<lottie::Animation>(CreateSkottie(/*duration_secs=*/10));
  tracker.RegisterAnimation(animation.get());
  animation->Start();
  Paint(*animation);

  clock_ += base::Seconds(5);
  Paint(*animation);

  ASSERT_TRUE(tracker.HasActiveAnimations());
  EXPECT_THAT(tracker.GetGlobalProgress().current_timestamp, FloatEq(0.5f));

  animation->Stop();
  EXPECT_FALSE(tracker.HasActiveAnimations());
  // Should be a no-op.
  tracker.RegisterAnimation(animation.get());

  animation->Start();
  Paint(*animation);

  ASSERT_TRUE(tracker.HasActiveAnimations());
  EXPECT_THAT(tracker.GetGlobalProgress().current_timestamp, FloatEq(0.f));
}

}  // namespace
}  // namespace ash
