// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_ui_controller.h"

#include "ash/test/ash_test_base.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

using FeatureStatus = chromeos::phonehub::FeatureStatus;

namespace ash {

class PhoneHubUiControllerTest : public AshTestBase,
                                 public PhoneHubUiController::Observer {
 public:
  PhoneHubUiControllerTest() = default;

  ~PhoneHubUiControllerTest() override { controller_.RemoveObserver(this); }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    controller_.AddObserver(this);

    GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
    GetOnboardingUiTracker()->SetShouldShowOnboardingUi(false);
    controller_.SetPhoneHubManager(&phone_hub_manager_);

    CHECK(ui_state_changed_);
    ui_state_changed_ = false;
  }

  chromeos::phonehub::FakeFeatureStatusProvider* GetFeatureStatusProvider() {
    return phone_hub_manager_.fake_feature_status_provider();
  }

  chromeos::phonehub::FakeOnboardingUiTracker* GetOnboardingUiTracker() {
    return phone_hub_manager_.fake_onboarding_ui_tracker();
  }

 protected:
  // PhoneHubUiController::Observer:
  void OnPhoneHubUiStateChanged() override {
    CHECK(!ui_state_changed_);
    ui_state_changed_ = true;
  }

  PhoneHubUiController controller_;
  chromeos::phonehub::FakePhoneHubManager phone_hub_manager_;
  bool ui_state_changed_ = false;
};

TEST_F(PhoneHubUiControllerTest, NotEligibleForFeature) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kNotEligibleForFeature);
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_.ui_state());
  EXPECT_TRUE(ui_state_changed_);
  EXPECT_FALSE(controller_.CreateContentView(/*bubble_view=*/nullptr).get());
}

TEST_F(PhoneHubUiControllerTest, OnboardingNotEligible) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_.ui_state());
  EXPECT_FALSE(controller_.CreateContentView(/*bubble_view=*/nullptr).get());
}

TEST_F(PhoneHubUiControllerTest, ShowOnboardingUi_WithoutPhone) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kDisabled);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);
  EXPECT_TRUE(ui_state_changed_);

  EXPECT_EQ(PhoneHubUiController::UiState::kOnboardingWithoutPhone,
            controller_.ui_state());

  auto content_view = controller_.CreateContentView(/*bubble_view=*/nullptr);
  // TODO(tengs): Test the actual view id.
  EXPECT_EQ(0, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, ShowOnboardingUi_WithPhone) {
  GetFeatureStatusProvider()->SetStatus(
      FeatureStatus::kEligiblePhoneButNotSetUp);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);
  EXPECT_TRUE(ui_state_changed_);

  EXPECT_EQ(PhoneHubUiController::UiState::kOnboardingWithPhone,
            controller_.ui_state());

  auto content_view = controller_.CreateContentView(/*bubble_view=*/nullptr);
  // TODO(tengs): Test the actual view id.
  EXPECT_EQ(0, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, PhoneConnectingForOnboarding) {
  GetFeatureStatusProvider()->SetStatus(
      FeatureStatus::kPhoneSelectedAndPendingSetup);
  EXPECT_EQ(PhoneHubUiController::UiState::kInitialConnecting,
            controller_.ui_state());

  auto content_view = controller_.CreateContentView(/*bubble_view=*/nullptr);
  // TODO(tengs): Test the actual view id.
  EXPECT_EQ(0, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, BluetoothOff) {
  GetFeatureStatusProvider()->SetStatus(
      FeatureStatus::kUnavailableBluetoothOff);
  EXPECT_EQ(PhoneHubUiController::UiState::kBluetoothDisabled,
            controller_.ui_state());

  auto content_view = controller_.CreateContentView(/*bubble_view=*/nullptr);
  // TODO(tengs): Test the actual view id.
  EXPECT_EQ(0, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, PhoneDisconnected) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(PhoneHubUiController::UiState::kConnectionError,
            controller_.ui_state());

  auto content_view = controller_.CreateContentView(/*bubble_view=*/nullptr);
  // TODO(tengs): Test the actual view id.
  EXPECT_EQ(0, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, PhoneConnecting) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnecting,
            controller_.ui_state());

  auto content_view = controller_.CreateContentView(/*bubble_view=*/nullptr);
  // TODO(tengs): Test the actual view id.
  EXPECT_EQ(0, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, PhoneConnected) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnected,
            controller_.ui_state());

  auto content_view = controller_.CreateContentView(/*bubble_view=*/nullptr);
  // TODO(tengs): Test the actual view id.
  EXPECT_EQ(0, content_view->GetID());
}

}  // namespace ash
