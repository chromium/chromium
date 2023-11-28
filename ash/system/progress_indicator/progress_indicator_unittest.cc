// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/progress_indicator/progress_indicator.h"

#include "ash/system/progress_indicator/progress_indicator_animation_registry.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"

namespace ash {
namespace {

// TestProgressIndicator -------------------------------------------------------

class TestProgressIndicator : public ProgressIndicator {
 public:
  TestProgressIndicator()
      : ProgressIndicator(
            /*animation_registry=*/nullptr,
            ProgressIndicatorAnimationRegistry::AsAnimationKey(this)) {}

  void SetProgress(const std::optional<float>& progress) {
    progress_ = progress;
    static_cast<ui::LayerDelegate*>(this)->UpdateVisualState();
  }

 private:
  // ProgressIndicator:
  std::optional<float> CalculateProgress() const override { return progress_; }
  std::optional<float> progress_;
};

}  // namespace

// ProgressIndicatorTest -------------------------------------------------------

using ProgressIndicatorTest = AshTestBase;

// Verifies that `ProgressIndicator::CreateDefaultInstance()` works as intended.
// It should delegate progress calculation to a constructor provided callback
// and manage progress animations as needed.
TEST_F(ProgressIndicatorTest, CreateDefaultInstance) {
  std::optional<float> progress = ProgressIndicator::kProgressComplete;

  // Create a default instance of `ProgressIndicator` that paints `progress`
  // whenever visual state is updated.
  auto progress_indicator = ProgressIndicator::CreateDefaultInstance(
      base::BindLambdaForTesting([&]() { return progress; }));

  // Cache `layer_delegate` associate with `progress_indicator` to manually
  // trigger update of visual state.
  auto* layer_delegate =
      static_cast<ui::LayerDelegate*>(progress_indicator.get());

  // Cache animation `key` and `registry` associated with `progress_indicator`.
  auto key = progress_indicator->animation_key();
  auto* registry = progress_indicator->animation_registry();

  // Verify initial progress and animation states.
  EXPECT_EQ(progress_indicator->progress(), progress);
  EXPECT_FALSE(registry->GetProgressIconAnimationForKey(key));
  EXPECT_FALSE(registry->GetProgressRingAnimationForKey(key));

  // Update `progress` to 0%. Verify progress and animation states.
  progress = 0.f;
  layer_delegate->UpdateVisualState();
  EXPECT_EQ(progress_indicator->progress(), progress);
  ASSERT_TRUE(registry->GetProgressIconAnimationForKey(key));
  EXPECT_TRUE(registry->GetProgressIconAnimationForKey(key)->HasAnimated());
  EXPECT_FALSE(registry->GetProgressRingAnimationForKey(key));

  // Update `progress` to 50%. Verify progress and animation states.
  progress = 0.5f;
  layer_delegate->UpdateVisualState();
  EXPECT_EQ(progress_indicator->progress(), progress);
  ASSERT_TRUE(registry->GetProgressIconAnimationForKey(key));
  EXPECT_TRUE(registry->GetProgressIconAnimationForKey(key)->HasAnimated());
  EXPECT_FALSE(registry->GetProgressRingAnimationForKey(key));

  // Update `progress` to indeterminate. Verify progress and animation states.
  progress = std::nullopt;
  layer_delegate->UpdateVisualState();
  EXPECT_EQ(progress_indicator->progress(), progress);
  ASSERT_TRUE(registry->GetProgressIconAnimationForKey(key));
  EXPECT_TRUE(registry->GetProgressIconAnimationForKey(key)->HasAnimated());
  ASSERT_TRUE(registry->GetProgressRingAnimationForKey(key));
  EXPECT_EQ(registry->GetProgressRingAnimationForKey(key)->type(),
            ProgressRingAnimation::Type::kIndeterminate);

  // Update `progress` to 75%. Verify progress and animation states.
  progress = 0.75f;
  layer_delegate->UpdateVisualState();
  EXPECT_EQ(progress_indicator->progress(), progress);
  ASSERT_TRUE(registry->GetProgressIconAnimationForKey(key));
  EXPECT_TRUE(registry->GetProgressIconAnimationForKey(key)->HasAnimated());
  EXPECT_FALSE(registry->GetProgressRingAnimationForKey(key));

  // Update `progress` to 100%. Verify progress an animation states.
  progress = ProgressIndicator::kProgressComplete;
  layer_delegate->UpdateVisualState();
  EXPECT_EQ(progress_indicator->progress(), progress);
  EXPECT_FALSE(registry->GetProgressIconAnimationForKey(key));
  ASSERT_TRUE(registry->GetProgressRingAnimationForKey(key));
  EXPECT_EQ(registry->GetProgressRingAnimationForKey(key)->type(),
            ProgressRingAnimation::Type::kPulse);

  // The pulse animation that runs on progress completion should be removed
  // automatically on animation completion.
  base::test::RepeatingTestFuture<ProgressRingAnimation*> future;
  auto subscription = registry->AddProgressRingAnimationChangedCallbackForKey(
      key, future.GetCallback());
  EXPECT_EQ(future.Take(), nullptr);
}

// Verifies that `ProgressIndicator::AddProgressChangedCallback()` works as
// intended.
TEST_F(ProgressIndicatorTest, AddProgressChangedCallback) {
  // Create a test `progress_indicator`.
  TestProgressIndicator progress_indicator;
  progress_indicator.SetProgress(0.5f);

  // Add a callback to be notified of progress changed events. The callback
  // should be invoked on progress changes so long as the returned subscription
  // continues to exist.
  int callback_call_count = 0;
  auto subscription =
      std::make_unique<base::RepeatingClosureList::Subscription>(
          progress_indicator.AddProgressChangedCallback(
              base::BindLambdaForTesting([&]() { ++callback_call_count; })));

  // Change the underlying progress.
  progress_indicator.SetProgress(0.75f);
  EXPECT_EQ(callback_call_count, 1);

  // Attempt to change the underlying progress to the same value.
  progress_indicator.SetProgress(0.75f);
  EXPECT_EQ(callback_call_count, 1);

  // Reset the subscription and change the underlying progress.
  subscription.reset();
  progress_indicator.SetProgress(1.f);
  EXPECT_EQ(callback_call_count, 1);
}

}  // namespace ash
