// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"

#include "ash/test/ash_test_base.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#include "chrome/browser/ash/magic_boost/mock_editor_panel_manager.h"
#include "chrome/browser/ash/magic_boost/mock_magic_boost_state.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/lottie/resource.h"

namespace ash {

class MagicBoostControllerAshTest : public AshTestBase {
 public:
  MagicBoostControllerAshTest() {
    // Sets the default functions for the test to create image with the lottie
    // resource id. Otherwise there's no `g_parse_lottie_as_still_image_` set in
    // the `ResourceBundle`.
    ui::ResourceBundle::SetLottieParsingFunctions(
        &lottie::ParseLottieAsStillImage,
        &lottie::ParseLottieAsThemedStillImage);
  }
  MagicBoostControllerAshTest(const MagicBoostControllerAshTest&) = delete;
  MagicBoostControllerAshTest& operator=(const MagicBoostControllerAshTest&) =
      delete;
  ~MagicBoostControllerAshTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    mock_magic_boost_state_ = std::make_unique<MockMagicBoostState>();
    mock_magic_boost_state_->set_editor_panel_manager_for_test(
        &mock_editor_panel_manager_);
  }

  void TearDown() override {
    mock_magic_boost_state_.reset();
    AshTestBase::TearDown();
  }

  void OnDisclaimerAcceptButtonPressed(
      crosapi::mojom::MagicBoostController::TransitionAction
          transition_action) {
    controller.OnDisclaimerAcceptButtonPressed(transition_action);
  }

  void OnDisclaimerDeclineButtonPressed() {
    controller.OnDisclaimerDeclineButtonPressed();
  }

  MockEditorPanelManager& mock_editor_panel_manager() {
    return mock_editor_panel_manager_;
  }

 protected:
  std::unique_ptr<MockMagicBoostState> mock_magic_boost_state_;
  testing::NiceMock<MockEditorPanelManager> mock_editor_panel_manager_;
  MagicBoostControllerAsh controller;
};

TEST_F(MagicBoostControllerAshTest, DisclaimerWidget) {
  EXPECT_FALSE(controller.disclaimer_widget_for_test());

  controller.ShowDisclaimerUi(
      /*display_id=*/display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  controller.CloseDisclaimerUi();

  EXPECT_FALSE(controller.disclaimer_widget_for_test());
}

TEST_F(MagicBoostControllerAshTest, OnDisclaimerAcceptButtonPressed) {
  ON_CALL(*mock_magic_boost_state_, ShouldIncludeOrcaInOptIn)
      .WillByDefault([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(false);
      });

  controller.ShowDisclaimerUi(
      /*display_id=*/display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature).Times(0);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature).Times(0);

  OnDisclaimerAcceptButtonPressed(
      crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing);

  EXPECT_EQ(chromeos::HMRConsentStatus::kApproved,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_TRUE(mock_magic_boost_state_->hmr_enabled().value());

  EXPECT_FALSE(controller.disclaimer_widget_for_test());
}

TEST_F(MagicBoostControllerAshTest,
       OnDisclaimerAcceptButtonPressedIncludeOrca) {
  ON_CALL(*mock_magic_boost_state_, ShouldIncludeOrcaInOptIn)
      .WillByDefault([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  controller.ShowDisclaimerUi(
      /*display_id=*/display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature).Times(0);

  OnDisclaimerAcceptButtonPressed(
      crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing);

  EXPECT_EQ(chromeos::HMRConsentStatus::kApproved,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_TRUE(mock_magic_boost_state_->hmr_enabled().value());

  EXPECT_FALSE(controller.disclaimer_widget_for_test());
}

TEST_F(MagicBoostControllerAshTest,
       OnDisclaimerAcceptButtonPressedIncludeOrcaAndTriggerEditorUI) {
  ON_CALL(*mock_magic_boost_state_, ShouldIncludeOrcaInOptIn)
      .WillByDefault([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  mock_magic_boost_state_->set_editor_panel_manager_for_test(
      &mock_editor_panel_manager_);

  controller.ShowDisclaimerUi(
      /*display_id=*/display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      crosapi::mojom::MagicBoostController::TransitionAction::kShowEditorPanel);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature).Times(0);
  EXPECT_CALL(mock_editor_panel_manager(), StartEditingFlow);

  OnDisclaimerAcceptButtonPressed(
      crosapi::mojom::MagicBoostController::TransitionAction::kShowEditorPanel);

  EXPECT_EQ(chromeos::HMRConsentStatus::kApproved,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_TRUE(mock_magic_boost_state_->hmr_enabled().value());

  EXPECT_FALSE(controller.disclaimer_widget_for_test());
  testing::Mock::VerifyAndClearExpectations(&mock_editor_panel_manager());
}

TEST_F(MagicBoostControllerAshTest, OnDisclaimerDeclineButtonPressed) {
  ON_CALL(*mock_magic_boost_state_, ShouldIncludeOrcaInOptIn)
      .WillByDefault([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(false);
      });

  controller.ShowDisclaimerUi(
      /*display_id=*/display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature).Times(0);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature).Times(0);

  OnDisclaimerDeclineButtonPressed();

  EXPECT_EQ(chromeos::HMRConsentStatus::kDeclined,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_FALSE(mock_magic_boost_state_->hmr_enabled().value());

  EXPECT_FALSE(controller.disclaimer_widget_for_test());
}

TEST_F(MagicBoostControllerAshTest,
       OnDisclaimerDeclineButtonPressedIncludeOrca) {
  ON_CALL(*mock_magic_boost_state_, ShouldIncludeOrcaInOptIn)
      .WillByDefault([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  controller.ShowDisclaimerUi(
      /*display_id=*/display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature).Times(0);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature);

  OnDisclaimerDeclineButtonPressed();

  EXPECT_EQ(chromeos::HMRConsentStatus::kDeclined,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_FALSE(mock_magic_boost_state_->hmr_enabled().value());

  EXPECT_FALSE(controller.disclaimer_widget_for_test());
}

}  // namespace ash
