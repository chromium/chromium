// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_ui_controller.h"

#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/optional.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/components/phonehub/fake_tether_controller.h"
#include "chromeos/components/phonehub/phone_model_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

using FeatureStatus = chromeos::phonehub::FeatureStatus;

namespace ash {

constexpr char kUser1Email[] = "user1@test.com";
constexpr char kUser2Email[] = "user2@test.com";

class PhoneHubUiControllerTest : public NoSessionAshTestBase,
                                 public PhoneHubUiController::Observer {
 public:
  PhoneHubUiControllerTest() = default;

  ~PhoneHubUiControllerTest() override { controller_->RemoveObserver(this); }

  // AshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    // Create user 1 session and simulate its login.
    SimulateUserLogin(kUser1Email);
    // Create user 2 session.
    GetSessionControllerClient()->AddUserSession(kUser2Email);

    controller_ = std::make_unique<PhoneHubUiController>();
    controller_->AddObserver(this);

    GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
    GetOnboardingUiTracker()->SetShouldShowOnboardingUi(false);
    controller_->SetPhoneHubManager(&phone_hub_manager_);

    CHECK(ui_state_changed_);
    ui_state_changed_ = false;
  }

  void SetLoggedInUser(bool is_primary) {
    const std::string& email = is_primary ? kUser1Email : kUser2Email;
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  chromeos::phonehub::FakeFeatureStatusProvider* GetFeatureStatusProvider() {
    return phone_hub_manager_.fake_feature_status_provider();
  }

  chromeos::phonehub::FakeOnboardingUiTracker* GetOnboardingUiTracker() {
    return phone_hub_manager_.fake_onboarding_ui_tracker();
  }

  chromeos::phonehub::FakeTetherController* GetTetherController() {
    return phone_hub_manager_.fake_tether_controller();
  }

  void SetPhoneStatusModel(
      const base::Optional<chromeos::phonehub::PhoneStatusModel>&
          phone_status_model) {
    phone_hub_manager_.mutable_phone_model()->SetPhoneStatusModel(
        phone_status_model);
  }

 protected:
  // PhoneHubUiController::Observer:
  void OnPhoneHubUiStateChanged() override {
    CHECK(!ui_state_changed_);
    ui_state_changed_ = true;
  }

  std::unique_ptr<PhoneHubUiController> controller_;
  chromeos::phonehub::FakePhoneHubManager phone_hub_manager_;
  bool ui_state_changed_ = false;
};

TEST_F(PhoneHubUiControllerTest, NotEligibleForFeature) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kNotEligibleForFeature);
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_->ui_state());
  EXPECT_TRUE(ui_state_changed_);
  EXPECT_FALSE(controller_->CreateContentView(/*delegate=*/nullptr).get());
}

TEST_F(PhoneHubUiControllerTest, OnboardingNotEligible) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_->ui_state());
  EXPECT_FALSE(controller_->CreateContentView(/*delegate=*/nullptr).get());
}

TEST_F(PhoneHubUiControllerTest, ShowOnboardingUi_WithoutPhone) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kDisabled);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);
  EXPECT_TRUE(ui_state_changed_);

  EXPECT_EQ(PhoneHubUiController::UiState::kOnboardingWithoutPhone,
            controller_->ui_state());

  auto content_view = controller_->CreateContentView(/*delegate=*/nullptr);
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, ShowOnboardingUi_WithPhone) {
  GetFeatureStatusProvider()->SetStatus(
      FeatureStatus::kEligiblePhoneButNotSetUp);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);
  EXPECT_TRUE(ui_state_changed_);

  EXPECT_EQ(PhoneHubUiController::UiState::kOnboardingWithPhone,
            controller_->ui_state());

  auto content_view = controller_->CreateContentView(/*delegate=*/nullptr);
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, PhoneSelectedAndPendingSetup) {
  GetFeatureStatusProvider()->SetStatus(
      FeatureStatus::kPhoneSelectedAndPendingSetup);
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_->ui_state());
}

TEST_F(PhoneHubUiControllerTest, BluetoothOff) {
  GetFeatureStatusProvider()->SetStatus(
      FeatureStatus::kUnavailableBluetoothOff);
  EXPECT_EQ(PhoneHubUiController::UiState::kBluetoothDisabled,
            controller_->ui_state());

  auto content_view = controller_->CreateContentView(/*delegate=*/nullptr);
  EXPECT_EQ(PhoneHubViewID::kBluetoothDisabledView, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, PhoneDisconnected) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneDisconnected,
            controller_->ui_state());

  auto content_view = controller_->CreateContentView(/*delegate=*/nullptr);
  EXPECT_EQ(PhoneHubViewID::kDisconnectedView, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, PhoneConnecting) {
  GetTetherController()->SetStatus(
      chromeos::phonehub::TetherController::Status::kConnectionAvailable);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnecting,
            controller_->ui_state());

  auto content_view = controller_->CreateContentView(/*delegate=*/nullptr);
  EXPECT_EQ(PhoneHubViewID::kPhoneConnectingView, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, TetherConnectionPending) {
  GetTetherController()->SetStatus(
      chromeos::phonehub::TetherController::Status::kConnecting);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(PhoneHubUiController::UiState::kTetherConnectionPending,
            controller_->ui_state());

  // Tether status becomes connected, but the feature status is still
  // |kEnabledAndConnecting|. The UiState should still be
  // kTetherConnectionPending.
  GetTetherController()->SetStatus(
      chromeos::phonehub::TetherController::Status::kConnected);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(PhoneHubUiController::UiState::kTetherConnectionPending,
            controller_->ui_state());

  // Tether status is connected, the feature status is |kEnabledAndConnected|,
  // but there is no phone model. The UiState should still be
  // kTetherConnectionPending.
  SetPhoneStatusModel(base::nullopt);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(PhoneHubUiController::UiState::kTetherConnectionPending,
            controller_->ui_state());

  auto content_view = controller_->CreateContentView(/*delegate=*/nullptr);
  EXPECT_EQ(PhoneHubViewID::kTetherConnectionPendingView,
            content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, PhoneConnected) {
  SetPhoneStatusModel(chromeos::phonehub::CreateFakePhoneStatusModel());
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnected,
            controller_->ui_state());

  auto content_view = controller_->CreateContentView(/*delegate=*/nullptr);
  EXPECT_EQ(kPhoneConnectedView, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, UnavailableScreenLocked) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kLockOrSuspended);
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_->ui_state());
  EXPECT_FALSE(controller_->CreateContentView(/*bubble_view=*/nullptr).get());
}

TEST_F(PhoneHubUiControllerTest, UnavailableSecondaryUser) {
  // Simulate log in to secondary user.
  SetLoggedInUser(false /* is_primary */);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_->ui_state());
  EXPECT_FALSE(controller_->CreateContentView(/*bubble_view=*/nullptr).get());

  // Switch back to primary user.
  SetLoggedInUser(true /* is_primary */);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnecting,
            controller_->ui_state());
}

TEST_F(PhoneHubUiControllerTest, ConnectedViewDelayed) {
  // Since there is no phone model, expect that we stay at the connecting screen
  // even though the feature status is kEnabledAndConnected.
  SetPhoneStatusModel(base::nullopt);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnecting,
            controller_->ui_state());
  auto content_view = controller_->CreateContentView(/*delegate=*/nullptr);
  EXPECT_EQ(kPhoneConnectingView, content_view->GetID());

  // Update the phone status model and expect the connected view to show up.
  SetPhoneStatusModel(chromeos::phonehub::CreateFakePhoneStatusModel());
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnected,
            controller_->ui_state());
  auto content_view2 = controller_->CreateContentView(/*delegate=*/nullptr);
  EXPECT_EQ(kPhoneConnectedView, content_view2->GetID());
}

}  // namespace ash
