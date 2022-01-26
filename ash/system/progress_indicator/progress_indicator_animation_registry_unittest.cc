// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/progress_indicator/progress_indicator_animation_registry.h"

#include "ash/constants/ash_features.h"
#include "ash/system/holding_space/holding_space_progress_icon_animation.h"
#include "ash/system/holding_space/holding_space_progress_indicator_animation.h"
#include "ash/system/holding_space/holding_space_progress_ring_animation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// ProgressIndicatorAnimationRegistryTest --------------------------------------

// Base class for tests of the `ProgressIndicatorAnimationRegistry`
// parameterized by whether animation v2 is enabled.
class ProgressIndicatorAnimationRegistryTest
    : public testing::Test,
      public testing::WithParamInterface<
          /*animation_v2_enabled=*/bool> {
 public:
  ProgressIndicatorAnimationRegistryTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kHoldingSpaceInProgressAnimationV2, IsAnimationV2Enabled());
  }

  // Returns whether animation v2 is enabled given test parameterization.
  bool IsAnimationV2Enabled() const { return GetParam(); }

  // Returns the `registry_` under test.
  ProgressIndicatorAnimationRegistry* registry() { return &registry_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ProgressIndicatorAnimationRegistry registry_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ProgressIndicatorAnimationRegistryTest,
                         /*in_progress_animation_v2_enabled=*/testing::Bool());

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_P(ProgressIndicatorAnimationRegistryTest, SetProgressIconAnimationForKey) {
  // Create `key` and verify no progress icon animation is set.
  size_t key = 0u;
  EXPECT_FALSE(registry()->GetProgressIconAnimationForKey(&key));

  // Count progress icon animation changed events.
  size_t callback_call_count = 0u;
  auto subscription = registry()->AddProgressIconAnimationChangedCallbackForKey(
      &key, base::BindLambdaForTesting([&](HoldingSpaceProgressIconAnimation*) {
        ++callback_call_count;
      }));

  // Unset progress icon animation for `key`.
  EXPECT_FALSE(registry()->SetProgressIconAnimationForKey(&key, nullptr));
  EXPECT_FALSE(registry()->GetProgressIconAnimationForKey(&key));
  EXPECT_EQ(callback_call_count, 0u);

  // Create a progress icon `animation`.
  auto animation = std::make_unique<HoldingSpaceProgressIconAnimation>();
  auto* animation_ptr = animation.get();

  // Set progress icon `animation` for `key`.
  EXPECT_EQ(
      registry()->SetProgressIconAnimationForKey(&key, std::move(animation)),
      animation_ptr);
  EXPECT_EQ(registry()->GetProgressIconAnimationForKey(&key), animation_ptr);
  EXPECT_EQ(callback_call_count, 1u);

  // Unset progress icon animation for `key`.
  EXPECT_FALSE(registry()->SetProgressIconAnimationForKey(&key, nullptr));
  EXPECT_FALSE(registry()->GetProgressIconAnimationForKey(&key));
  EXPECT_EQ(callback_call_count, 2u);
}

TEST_P(ProgressIndicatorAnimationRegistryTest, SetProgressRingAnimationForKey) {
  // Create `key` and verify no progress ring animation is set.
  size_t key = 0u;
  EXPECT_FALSE(registry()->GetProgressRingAnimationForKey(&key));

  // Count progress ring animation changed events.
  size_t callback_call_count = 0u;
  auto subscription = registry()->AddProgressRingAnimationChangedCallbackForKey(
      &key, base::BindLambdaForTesting([&](HoldingSpaceProgressRingAnimation*) {
        ++callback_call_count;
      }));

  // Unset progress ring animation for `key`.
  EXPECT_FALSE(registry()->SetProgressRingAnimationForKey(&key, nullptr));
  EXPECT_FALSE(registry()->GetProgressRingAnimationForKey(&key));
  EXPECT_EQ(callback_call_count, 0u);

  // Create a progress ring `animation`.
  auto animation = HoldingSpaceProgressRingAnimation::CreateOfType(
      HoldingSpaceProgressRingAnimation::Type::kPulse);
  auto* animation_ptr = animation.get();

  // Set progress ring `animation` for `key`.
  EXPECT_EQ(
      registry()->SetProgressRingAnimationForKey(&key, std::move(animation)),
      animation_ptr);
  EXPECT_EQ(registry()->GetProgressRingAnimationForKey(&key), animation_ptr);
  EXPECT_EQ(callback_call_count, 1u);

  // Unset progress ring animation for `key`.
  EXPECT_FALSE(registry()->SetProgressRingAnimationForKey(&key, nullptr));
  EXPECT_FALSE(registry()->GetProgressRingAnimationForKey(&key));
  EXPECT_EQ(callback_call_count, 2u);
}

}  // namespace ash
