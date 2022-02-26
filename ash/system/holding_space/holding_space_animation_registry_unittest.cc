// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_animation_registry.h"

#include <array>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/system/progress_indicator/progress_icon_animation.h"
#include "ash/system/progress_indicator/progress_indicator_animation.h"
#include "ash/system/progress_indicator/progress_ring_animation.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

constexpr char kTestUser[] = "user@test";

// HoldingSpaceAnimationRegistryTest -------------------------------------------

class HoldingSpaceAnimationRegistryTest
    : public AshTestBase,
      public testing::WithParamInterface<std::tuple<
          /*animation_v2_enabled=*/bool,
          /*animation_v2_delay_enabled=*/bool>> {
 public:
  HoldingSpaceAnimationRegistryTest() {
    std::vector<base::Feature> disabled_features;
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;

    if (IsAnimationV2Enabled()) {
      enabled_features.push_back(
          base::test::ScopedFeatureList::FeatureAndParams(
              features::kHoldingSpaceInProgressAnimationV2,
              {{"delay_enabled",
                IsAnimationV2DelayEnabled() ? "true" : "false"}}));
    } else {
      disabled_features.push_back(features::kHoldingSpaceInProgressAnimationV2);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  bool IsAnimationV2Enabled() const { return std::get<0>(GetParam()); }

  bool IsAnimationV2DelayEnabled() const {
    return IsAnimationV2Enabled() && std::get<1>(GetParam());
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Initialize holding space for `kTestUser`.
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        user_account, client(), model());
    GetSessionControllerClient()->AddUserSession(kTestUser);
    holding_space_prefs::MarkTimeOfFirstAvailability(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  HoldingSpaceItem* AddItem(
      HoldingSpaceItem::Type type,
      const base::FilePath& path,
      const HoldingSpaceProgress& progress = HoldingSpaceProgress()) {
    GURL file_system_url(
        base::StrCat({"filesystem:", path.BaseName().value()}));
    std::unique_ptr<HoldingSpaceItem> item =
        HoldingSpaceItem::CreateFileBackedItem(
            type, path, file_system_url, progress,
            base::BindOnce(
                [](HoldingSpaceItem::Type type, const base::FilePath& path) {
                  return std::make_unique<HoldingSpaceImage>(
                      holding_space_util::GetMaxImageSizeForType(type), path,
                      /*async_bitmap_resolver=*/base::DoNothing());
                }));
    HoldingSpaceItem* item_ptr = item.get();
    model()->AddItem(std::move(item));
    return item_ptr;
  }

  void ExpectProgressIconAnimationExistsForKey(const void* key, bool exists) {
    auto* animation = registry()->GetProgressIconAnimationForKey(key);
    EXPECT_EQ(!!animation, exists);
  }

  void ExpectProgressIconAnimationHasAnimatedForKey(const void* key,
                                                    bool has_animated) {
    auto* animation = registry()->GetProgressIconAnimationForKey(key);
    EXPECT_EQ(animation && animation->HasAnimated(), has_animated);
  }

  void ExpectProgressRingAnimationOfTypeForKey(
      const void* key,
      const absl::optional<ProgressRingAnimation::Type>& type) {
    auto* animation = registry()->GetProgressRingAnimationForKey(key);
    EXPECT_EQ(!!animation, type.has_value());
    if (animation && type.has_value())
      EXPECT_EQ(animation->type(), type.value());
  }

  void StartSession() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    GetSessionControllerClient()->SwitchActiveUser(user_account);
  }

  HoldingSpaceController* controller() { return HoldingSpaceController::Get(); }

  testing::NiceMock<MockHoldingSpaceClient>* client() {
    return &holding_space_client_;
  }

  HoldingSpaceModel* model() { return &holding_space_model_; }

  HoldingSpaceAnimationRegistry* registry() {
    return HoldingSpaceAnimationRegistry::GetInstance();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockHoldingSpaceClient> holding_space_client_;
  HoldingSpaceModel holding_space_model_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceAnimationRegistryTest,
                         testing::Combine(
                             /*animation_v2_enabled=*/testing::Bool(),
                             /*animation_v2_delay_enabled=*/testing::Bool()));

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_P(HoldingSpaceAnimationRegistryTest, ProgressIndicatorAnimations) {
  using Type = ProgressRingAnimation::Type;

  StartSession();

  // Verify initial animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), false);
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);

  // Add a completed item to the `model()`.
  HoldingSpaceItem* item_0 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/0"));

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), false);
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);

  // Add an indeterminately in-progress item to the `model()`.
  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/1"),
              HoldingSpaceProgress(0, absl::nullopt));

  bool v2_enabled = IsAnimationV2Enabled();
  bool v2_with_delay_disabled = v2_enabled && !IsAnimationV2DelayEnabled();

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), v2_enabled);
  ExpectProgressIconAnimationHasAnimatedForKey(controller(), v2_enabled);
  ExpectProgressRingAnimationOfTypeForKey(controller(), Type::kIndeterminate);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_1, v2_enabled);
  ExpectProgressIconAnimationHasAnimatedForKey(item_1, v2_with_delay_disabled);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kIndeterminate);

  // Add a determinately in-progress item to the `model()`.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/2"),
              HoldingSpaceProgress(0, 10));

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), v2_enabled);
  ExpectProgressIconAnimationHasAnimatedForKey(controller(), v2_enabled);
  ExpectProgressRingAnimationOfTypeForKey(controller(), Type::kIndeterminate);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_1, v2_enabled);
  ExpectProgressIconAnimationHasAnimatedForKey(item_1, v2_with_delay_disabled);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kIndeterminate);
  ExpectProgressIconAnimationExistsForKey(item_2, v2_enabled);
  ExpectProgressIconAnimationHasAnimatedForKey(item_2, v2_with_delay_disabled);
  ExpectProgressRingAnimationOfTypeForKey(item_2, absl::nullopt);

  // Complete the first in-progress item.
  model()->UpdateItem(item_1->id())->SetProgress(HoldingSpaceProgress(10, 10));

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), v2_enabled);
  ExpectProgressIconAnimationHasAnimatedForKey(controller(), v2_enabled);
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_1, false);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kPulse);
  ExpectProgressIconAnimationExistsForKey(item_2, v2_enabled);
  ExpectProgressIconAnimationHasAnimatedForKey(item_2, v2_with_delay_disabled);
  ExpectProgressRingAnimationOfTypeForKey(item_2, absl::nullopt);

  // Complete the second in-progress item.
  model()->UpdateItem(item_2->id())->SetProgress(HoldingSpaceProgress(10, 10));

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), false);
  ExpectProgressRingAnimationOfTypeForKey(controller(), Type::kPulse);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_1, false);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kPulse);
  ExpectProgressIconAnimationExistsForKey(item_2, false);
  ExpectProgressRingAnimationOfTypeForKey(item_2, Type::kPulse);

  // Wait for `kPulse` animations to complete.
  base::test::RepeatingTestFuture<ProgressRingAnimation*> future;
  std::array<base::CallbackListSubscription, 3u> subscriptions = {
      registry()->AddProgressRingAnimationChangedCallbackForKey(
          controller(), future.GetCallback()),
      registry()->AddProgressRingAnimationChangedCallbackForKey(
          item_1, future.GetCallback()),
      registry()->AddProgressRingAnimationChangedCallbackForKey(
          item_2, future.GetCallback())};
  for (size_t i = 0u; i < std::size(subscriptions); ++i)
    EXPECT_EQ(future.Take(), nullptr);

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), false);
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_1, false);
  ExpectProgressRingAnimationOfTypeForKey(item_1, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_2, false);
  ExpectProgressRingAnimationOfTypeForKey(item_2, absl::nullopt);
}

}  // namespace ash
