// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_animation_registry.h"

#include <array>

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/system/holding_space/holding_space_progress_ring_animation.h"
#include "ash/test/ash_test_base.h"
#include "base/barrier_closure.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

constexpr char kTestUser[] = "user@test";

// Helpers ---------------------------------------------------------------------

template <typename... T>
base::RepeatingCallback<void(T...)> IgnoreArgs(base::RepeatingClosure closure) {
  return base::BindRepeating([](T...) {}).Then(std::move(closure));
}

// HoldingSpaceAnimationRegistryTest -------------------------------------------

class HoldingSpaceAnimationRegistryTest : public AshTestBase {
 public:
  HoldingSpaceAnimationRegistryTest() = default;

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

  void ExpectProgressRingAnimationOfTypeForKey(
      const void* key,
      const absl::optional<HoldingSpaceProgressRingAnimation::Type>& type) {
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
  testing::NiceMock<MockHoldingSpaceClient> holding_space_client_;
  HoldingSpaceModel holding_space_model_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_F(HoldingSpaceAnimationRegistryTest, ProgressIndicatorAnimations) {
  using Type = HoldingSpaceProgressRingAnimation::Type;

  StartSession();

  // Verify initial animation `registry()` state.
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);

  // Add a completed item to the `model()`.
  HoldingSpaceItem* item_0 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/0"));

  // Verify animation `registry()` state.
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);

  // Add an indeterminately in-progress item to the `model()`.
  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/1"),
              HoldingSpaceProgress(0, absl::nullopt));

  // Verify animation `registry()` state.
  ExpectProgressRingAnimationOfTypeForKey(controller(), Type::kIndeterminate);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kIndeterminate);

  // Add a determinately in-progress item to the `model()`.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/2"),
              HoldingSpaceProgress(0, 10));

  // Verify animation `registry()` state.
  ExpectProgressRingAnimationOfTypeForKey(controller(), Type::kIndeterminate);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kIndeterminate);
  ExpectProgressRingAnimationOfTypeForKey(item_2, absl::nullopt);

  // Complete the first in-progress item.
  model()->UpdateItem(item_1->id())->SetProgress(HoldingSpaceProgress(10, 10));

  // Verify animation `registry()` state.
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kPulse);
  ExpectProgressRingAnimationOfTypeForKey(item_2, absl::nullopt);

  // Complete the second in-progress item.
  model()->UpdateItem(item_2->id())->SetProgress(HoldingSpaceProgress(10, 10));

  // Verify animation `registry()` state.
  ExpectProgressRingAnimationOfTypeForKey(controller(), Type::kPulse);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kPulse);
  ExpectProgressRingAnimationOfTypeForKey(item_2, Type::kPulse);

  {
    // Wait for `kPulse` animations to complete.
    base::RunLoop run_loop;
    auto pulse_animation_complete = base::BarrierClosure(
        3u, base::BindLambdaForTesting([&]() {
          // Verify animation `registry()` state.
          ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);
          ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
          ExpectProgressRingAnimationOfTypeForKey(item_1, absl::nullopt);
          ExpectProgressRingAnimationOfTypeForKey(item_2, absl::nullopt);
          run_loop.Quit();
        }));

    std::array<base::CallbackListSubscription, 3u> subscriptions = {
        registry()->AddProgressRingAnimationChangedCallbackForKey(
            controller(), IgnoreArgs<HoldingSpaceProgressRingAnimation*>(
                              pulse_animation_complete)),
        registry()->AddProgressRingAnimationChangedCallbackForKey(
            item_1, IgnoreArgs<HoldingSpaceProgressRingAnimation*>(
                        pulse_animation_complete)),
        registry()->AddProgressRingAnimationChangedCallbackForKey(
            item_2, IgnoreArgs<HoldingSpaceProgressRingAnimation*>(
                        pulse_animation_complete))};

    run_loop.Run();
  }
}

}  // namespace ash
