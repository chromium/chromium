// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_feature_pod_controller.h"

#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"

namespace ash {

// Tests manually control their session state.
class NearbyShareFeaturePodControllerTest : public NoSessionAshTestBase {
 public:
  NearbyShareFeaturePodControllerTest() = default;
  NearbyShareFeaturePodControllerTest(NearbyShareFeaturePodControllerTest&) =
      delete;
  NearbyShareFeaturePodControllerTest& operator=(
      NearbyShareFeaturePodControllerTest&) = delete;
  ~NearbyShareFeaturePodControllerTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    tray_model_ = std::make_unique<UnifiedSystemTrayModel>(nullptr);
    tray_controller_ =
        std::make_unique<UnifiedSystemTrayController>(tray_model_.get());

    test_delegate_ = static_cast<TestNearbyShareDelegate*>(
        Shell::Get()->nearby_share_delegate());
    nearby_share_controller_ = Shell::Get()->nearby_share_controller();

    test_delegate_->set_is_pod_button_visible(true);
  }

  void TearDown() override {
    button_.reset();
    pod_controller_.reset();
    tray_controller_.reset();
    tray_model_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  void SetUpButton() {
    pod_controller_ = std::make_unique<NearbyShareFeaturePodController>(
        tray_controller_.get());
    button_.reset(pod_controller_->CreateButton());
  }

  std::unique_ptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  std::unique_ptr<NearbyShareFeaturePodController> pod_controller_;
  std::unique_ptr<FeaturePodButton> button_;

  TestNearbyShareDelegate* test_delegate_ = nullptr;
  NearbyShareController* nearby_share_controller_ = nullptr;
};

TEST_F(NearbyShareFeaturePodControllerTest, ButtonVisibilityNotLoggedIn) {
  SetUpButton();
  // If not logged in, it should not be visible.
  EXPECT_FALSE(button_->GetVisible());
}

TEST_F(NearbyShareFeaturePodControllerTest, ButtonVisibilityLoggedIn) {
  CreateUserSessions(1);
  SetUpButton();
  // If logged in, it should be visible.
  EXPECT_TRUE(button_->GetVisible());
}

TEST_F(NearbyShareFeaturePodControllerTest, ButtonVisibilityLocked) {
  CreateUserSessions(1);
  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOCK_SCREEN);
  SetUpButton();
  // If locked, it should not be visible.
  EXPECT_FALSE(button_->GetVisible());
}

TEST_F(NearbyShareFeaturePodControllerTest, ButtonVisiblilityHiddenByDelegate) {
  CreateUserSessions(1);
  test_delegate_->set_is_pod_button_visible(false);
  SetUpButton();
  // If NearbyShareDelegate::IsPodButtonVisible() returns false, it should
  // not be visible.
  EXPECT_FALSE(button_->GetVisible());
}

TEST_F(NearbyShareFeaturePodControllerTest,
       ButtonToggledByHighVisibilityEnabledEvent) {
  CreateUserSessions(1);
  SetUpButton();
  ASSERT_FALSE(button_->IsToggled());
  nearby_share_controller_->HighVisibilityEnabledChanged(true);
  EXPECT_TRUE(button_->IsToggled());
  nearby_share_controller_->HighVisibilityEnabledChanged(false);
  EXPECT_FALSE(button_->IsToggled());
}

TEST_F(NearbyShareFeaturePodControllerTest, ButtonPressTogglesHighVisibility) {
  CreateUserSessions(1);
  SetUpButton();
  test_delegate_->method_calls().clear();

  test_delegate_->set_is_high_visibility_on(false);
  pod_controller_->OnIconPressed();
  EXPECT_EQ(1u, test_delegate_->method_calls().size());
  EXPECT_EQ(TestNearbyShareDelegate::Method::kEnableHighVisibility,
            test_delegate_->method_calls()[0]);

  test_delegate_->set_is_high_visibility_on(true);
  pod_controller_->OnIconPressed();
  EXPECT_EQ(2u, test_delegate_->method_calls().size());
  EXPECT_EQ(TestNearbyShareDelegate::Method::kDisableHighVisibility,
            test_delegate_->method_calls()[1]);
}

}  // namespace ash
