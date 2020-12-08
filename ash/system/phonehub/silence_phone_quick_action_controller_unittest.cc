// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/silence_phone_quick_action_controller.h"

#include "ash/test/ash_test_base.h"
#include "chromeos/components/phonehub/fake_do_not_disturb_controller.h"

namespace ash {

class SilencePhoneQuickActionControllerTest
    : public AshTestBase,
      public SilencePhoneQuickActionController::Observer {
 public:
  SilencePhoneQuickActionControllerTest() = default;

  ~SilencePhoneQuickActionControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    dnd_controller_ =
        std::make_unique<chromeos::phonehub::FakeDoNotDisturbController>();
    controller_ = std::make_unique<SilencePhoneQuickActionController>(
        dnd_controller_.get());

    controller_->AddObserver(this);
    controller_->CreateItem();
  }

  void TearDown() override {
    controller_->RemoveObserver(this);
    controller_.reset();
    dnd_controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  // SilencePhoneQuickActionController::Observer:
  void OnSilencePhoneItemStateChanged() override { ++num_calls_; }

  SilencePhoneQuickActionController* controller() { return controller_.get(); }

  chromeos::phonehub::FakeDoNotDisturbController* dnd_controller() {
    return dnd_controller_.get();
  }

  bool IsButtonDisabled() {
    return SilencePhoneQuickActionController::ActionState::kDisabled ==
           controller_->GetItemState();
  }

  size_t GetNumObserverCalls() { return num_calls_; }

 private:
  std::unique_ptr<SilencePhoneQuickActionController> controller_;
  std::unique_ptr<chromeos::phonehub::FakeDoNotDisturbController>
      dnd_controller_;
  size_t num_calls_ = 0;
};

TEST_F(SilencePhoneQuickActionControllerTest, ItemStateChanged) {
  // Set request to fail to avoid triggering state's changes by the model.
  dnd_controller()->SetShouldRequestFail(true);

  // Initially, there's one observer call during initiation.
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Allow the button to be enabled.
  dnd_controller()->SetDoNotDisturbStateInternal(
      /*is_dnd_enabled=*/false, /*can_request_new_dnd_state=*/true);
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Press the button to enabled state will trigger observer.
  controller()->OnButtonPressed(false /* is_now_enabled */);
  EXPECT_EQ(3u, GetNumObserverCalls());

  // Item state changed to enabled.
  EXPECT_TRUE(controller()->IsItemEnabled());

  // Press the button to disabled state will trigger observer.
  controller()->OnButtonPressed(true /* is_now_enabled */);
  EXPECT_EQ(4u, GetNumObserverCalls());

  // Item state changed to disabled.
  EXPECT_FALSE(controller()->IsItemEnabled());

  dnd_controller()->SetShouldRequestFail(false);
}

TEST_F(SilencePhoneQuickActionControllerTest, CanRequestNewDndState) {
  // Set DoNotDisturbState to not allow any new request.
  dnd_controller()->SetDoNotDisturbStateInternal(
      /*is_dnd_enabled=*/false, /*can_request_new_dnd_state=*/false);
  EXPECT_EQ(1u, GetNumObserverCalls());
  // Since no new state requests are allowed, expect the button to be disabled.
  EXPECT_FALSE(controller()->IsItemEnabled());
  EXPECT_TRUE(IsButtonDisabled());

  // Simulate that the phone updated its DoNotDisturb state, but is still in a
  // work profile.
  dnd_controller()->SetDoNotDisturbStateInternal(
      /*is_dnd_enabled=*/true, /*can_request_new_dnd_state=*/false);
  EXPECT_EQ(2u, GetNumObserverCalls());
  // The button should still be disabled despite the phone has DoNotDisturb mode
  // enabled. However, the underlying toggle has been flipped to enabled.
  EXPECT_TRUE(controller()->IsItemEnabled());
  EXPECT_TRUE(IsButtonDisabled());

  // Flip toggle state back to enabled, but still in a work profile.
  dnd_controller()->SetDoNotDisturbStateInternal(
      /*is_dnd_enabled=*/false, /*can_request_new_dnd_state=*/false);
  EXPECT_EQ(3u, GetNumObserverCalls());
  EXPECT_FALSE(controller()->IsItemEnabled());
  EXPECT_TRUE(IsButtonDisabled());
}

}  // namespace ash
