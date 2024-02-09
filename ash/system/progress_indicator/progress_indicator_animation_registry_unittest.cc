// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/progress_indicator/progress_indicator_animation_registry.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/system/progress_indicator/progress_icon_animation.h"
#include "ash/system/progress_indicator/progress_indicator_animation.h"
#include "ash/system/progress_indicator/progress_ring_animation.h"
#include "base/containers/contains.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// ProgressIndicatorAnimationRegistryTest --------------------------------------

// Base class for tests of the `ProgressIndicatorAnimationRegistry`.
class ProgressIndicatorAnimationRegistryTest : public testing::Test {
 public:
  // Returns the `registry_` under test.
  ProgressIndicatorAnimationRegistry* registry() { return &registry_; }

 private:
  ProgressIndicatorAnimationRegistry registry_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_F(ProgressIndicatorAnimationRegistryTest, EraseAllAnimations) {
  // Create `master_keys` for progress animations which may be referenced from
  // each test case.
  std::vector<size_t> master_keys = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u};
  std::vector<ProgressIndicatorAnimationRegistry::AnimationKey> keys = {
      ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_keys[0]),
      ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_keys[1]),
      ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_keys[2]),
      ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_keys[3]),
      ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_keys[4]),
      ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_keys[5]),
      ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_keys[6]),
      ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_keys[7]),
      ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_keys[8]),
  };

  // A test case is defined by:
  // * keys for which to add progress animations,
  // * a callback to invoke to erase progress animations,
  // * keys for which to expect progress animations after erase invocation.
  struct TestCase {
    std::vector<ProgressIndicatorAnimationRegistry::AnimationKey> keys;
    base::OnceClosure erase_callback;
    std::vector<ProgressIndicatorAnimationRegistry::AnimationKey>
        keys_with_animations_after_erase_callback;
  };

  std::vector<TestCase> test_cases;

  // Test case: Invoke `EraseAllAnimations()`.
  test_cases.push_back(
      TestCase{.keys = {keys[0], keys[1], keys[2]},
               .erase_callback = base::BindOnce(
                   &ProgressIndicatorAnimationRegistry::EraseAllAnimations,
                   base::Unretained(registry())),
               .keys_with_animations_after_erase_callback = {}});

  // Test case: Invoke `EraseAllAnimationsForKey()`.
  test_cases.push_back(TestCase{
      .keys = {keys[3], keys[4], keys[5]},
      .erase_callback = base::BindOnce(
          &ProgressIndicatorAnimationRegistry::EraseAllAnimationsForKey,
          base::Unretained(registry()), keys[4]),
      .keys_with_animations_after_erase_callback = {keys[3], keys[5]}});

  // Test case: Invoke `EraseAllAnimationsForKeyIf()`.
  test_cases.push_back(TestCase{
      .keys = {keys[6], keys[7], keys[8]},
      .erase_callback = base::BindOnce(
          &ProgressIndicatorAnimationRegistry::EraseAllAnimationsForKeyIf,
          base::Unretained(registry()),
          [&](ProgressIndicatorAnimationRegistry::AnimationKey candidate_key) {
            return candidate_key != keys[7];
          }),
      .keys_with_animations_after_erase_callback = {keys[7]}});

  // Iterate over `test_cases`.
  for (auto& test_case : test_cases) {
    // Track number of animation changed events.
    std::vector<base::CallbackListSubscription> subscriptions;
    size_t icon_callback_call_count = 0u;
    size_t ring_callback_call_count = 0u;

    // Iterate over `test_case.keys`.
    for (auto it = test_case.keys.begin(); it != test_case.keys.end(); ++it) {
      const auto key = *it;
      const size_t index = std::distance(test_case.keys.begin(), it);

      // Verify no progress animations are set for `key`.
      EXPECT_FALSE(registry()->GetProgressIconAnimationForKey(key));
      EXPECT_FALSE(registry()->GetProgressRingAnimationForKey(key));

      // Count progress icon animation changed events for `key`.
      subscriptions.push_back(
          registry()->AddProgressIconAnimationChangedCallbackForKey(
              key, base::BindLambdaForTesting([&](ProgressIconAnimation*) {
                ++icon_callback_call_count;
              })));

      // Count progress ring animation changed events for `key`.
      subscriptions.push_back(
          registry()->AddProgressRingAnimationChangedCallbackForKey(
              key, base::BindLambdaForTesting([&](ProgressRingAnimation*) {
                ++ring_callback_call_count;
              })));

      // Create a progress icon animation for `key`.
      registry()->SetProgressIconAnimationForKey(
          key, ProgressIconAnimation::Create());
      EXPECT_TRUE(registry()->GetProgressIconAnimationForKey(key));
      EXPECT_EQ(icon_callback_call_count, index + 1u);

      // Create a progress ring animation for `key`.
      registry()->SetProgressRingAnimationForKey(
          key, ProgressRingAnimation::CreateOfType(
                   ProgressRingAnimation::Type::kPulse));
      EXPECT_TRUE(registry()->GetProgressRingAnimationForKey(key));
      EXPECT_EQ(ring_callback_call_count, index + 1u);
    }

    // Reset animation changed event counts.
    icon_callback_call_count = 0u;
    ring_callback_call_count = 0u;

    // Erase animations.
    std::move(test_case.erase_callback).Run();
    EXPECT_EQ(icon_callback_call_count,
              test_case.keys.size() -
                  test_case.keys_with_animations_after_erase_callback.size());
    EXPECT_EQ(ring_callback_call_count,
              test_case.keys.size() -
                  test_case.keys_with_animations_after_erase_callback.size());

    // Iterate over `keys`.
    for (const auto key : keys) {
      const bool expected = base::Contains(
          test_case.keys_with_animations_after_erase_callback, key);

      // Verify progress animations are set for `key` only if `expected`.
      EXPECT_EQ(!!registry()->GetProgressIconAnimationForKey(key), expected);
      EXPECT_EQ(!!registry()->GetProgressRingAnimationForKey(key), expected);
    }
  }
}

TEST_F(ProgressIndicatorAnimationRegistryTest, SetProgressIconAnimationForKey) {
  // Create `key` and verify no progress icon animation is set.
  size_t master_key = 0u;
  auto key = ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_key);
  EXPECT_FALSE(registry()->GetProgressIconAnimationForKey(key));

  // Count progress icon animation changed events.
  size_t callback_call_count = 0u;
  auto subscription = registry()->AddProgressIconAnimationChangedCallbackForKey(
      key, base::BindLambdaForTesting(
               [&](ProgressIconAnimation*) { ++callback_call_count; }));

  // Unset progress icon animation for `key`.
  EXPECT_FALSE(registry()->SetProgressIconAnimationForKey(key, nullptr));
  EXPECT_FALSE(registry()->GetProgressIconAnimationForKey(key));
  EXPECT_EQ(callback_call_count, 0u);

  // Create a progress icon `animation`.
  auto animation = ProgressIconAnimation::Create();
  auto* animation_ptr = animation.get();

  // Set progress icon `animation` for `key`.
  EXPECT_EQ(
      registry()->SetProgressIconAnimationForKey(key, std::move(animation)),
      animation_ptr);
  EXPECT_EQ(registry()->GetProgressIconAnimationForKey(key), animation_ptr);
  EXPECT_EQ(callback_call_count, 1u);

  // Unset progress icon animation for `key`.
  EXPECT_FALSE(registry()->SetProgressIconAnimationForKey(key, nullptr));
  EXPECT_FALSE(registry()->GetProgressIconAnimationForKey(key));
  EXPECT_EQ(callback_call_count, 2u);
}

TEST_F(ProgressIndicatorAnimationRegistryTest, SetProgressRingAnimationForKey) {
  // Create `key` and verify no progress ring animation is set.
  size_t master_key = 0u;
  auto key = ProgressIndicatorAnimationRegistry::AsAnimationKey(&master_key);
  EXPECT_FALSE(registry()->GetProgressRingAnimationForKey(key));

  // Count progress ring animation changed events.
  size_t callback_call_count = 0u;
  auto subscription = registry()->AddProgressRingAnimationChangedCallbackForKey(
      key, base::BindLambdaForTesting(
               [&](ProgressRingAnimation*) { ++callback_call_count; }));

  // Unset progress ring animation for `key`.
  EXPECT_FALSE(registry()->SetProgressRingAnimationForKey(key, nullptr));
  EXPECT_FALSE(registry()->GetProgressRingAnimationForKey(key));
  EXPECT_EQ(callback_call_count, 0u);

  // Create a progress ring `animation`.
  auto animation =
      ProgressRingAnimation::CreateOfType(ProgressRingAnimation::Type::kPulse);
  auto* animation_ptr = animation.get();

  // Set progress ring `animation` for `key`.
  EXPECT_EQ(
      registry()->SetProgressRingAnimationForKey(key, std::move(animation)),
      animation_ptr);
  EXPECT_EQ(registry()->GetProgressRingAnimationForKey(key), animation_ptr);
  EXPECT_EQ(callback_call_count, 1u);

  // Unset progress ring animation for `key`.
  EXPECT_FALSE(registry()->SetProgressRingAnimationForKey(key, nullptr));
  EXPECT_FALSE(registry()->GetProgressRingAnimationForKey(key));
  EXPECT_EQ(callback_call_count, 2u);
}

}  // namespace ash
