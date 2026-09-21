// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_controller.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/ash/magic_boost/magic_boost_metrics.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state.h"
#include "chrome/browser/ash/magic_boost/mock_editor_panel_manager.h"
#include "chrome/browser/ash/magic_boost/mock_magic_boost_state.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

constexpr char kTestUrl[] = "https://www.google.com";

std::string HistogramName(std::string_view suffix) {
  return base::StrCat({kMagicBoostDisclaimerViewHistogram, suffix});
}

}  // namespace

class MagicBoostControllerTest : public ChromeAshTestBase {
 public:
  MagicBoostControllerTest() = default;
  MagicBoostControllerTest(const MagicBoostControllerTest&) = delete;
  MagicBoostControllerTest& operator=(const MagicBoostControllerTest&) = delete;
  ~MagicBoostControllerTest() override = default;

  // ChromeAshTestBase:
  void SetUp() override {
    ChromeAshTestBase::SetUp();

    mock_magic_boost_state_ = std::make_unique<MockMagicBoostState>();
    mock_magic_boost_state_->set_editor_panel_manager_for_test(
        &mock_editor_panel_manager_);
  }

  void TearDown() override {
    mock_magic_boost_state_.reset();
    ChromeAshTestBase::TearDown();
  }

  void OnDisclaimerAcceptButtonPressed(
      int64_t display_id,
      magic_boost::TransitionAction transition_action) {
    controller.OnDisclaimerAcceptButtonPressed(display_id, transition_action);
  }

  void OnDisclaimerDeclineButtonPressed() {
    controller.OnDisclaimerDeclineButtonPressed();
  }

  void OnLinkPressed() { controller.OnLinkPressed(kTestUrl); }

  MockEditorPanelManager& mock_editor_panel_manager() {
    return mock_editor_panel_manager_;
  }

 protected:
  std::unique_ptr<MockMagicBoostState> mock_magic_boost_state_;
  testing::NiceMock<MockEditorPanelManager> mock_editor_panel_manager_;
  MagicBoostControllerImpl controller;
};

TEST_F(MagicBoostControllerTest, DisclaimerWidget) {
  EXPECT_FALSE(controller.disclaimer_widget_for_test());

  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(HistogramName("Total"), 0);
  histogram_tester->ExpectTotalCount(HistogramName("OrcaAndHmr"), 0);

  controller.ShowDisclaimerUi(display::Screen::Get()->GetPrimaryDisplay().id(),
                              magic_boost::TransitionAction::kDoNothing,
                              magic_boost::OptInFeatures::kOrcaAndHmr);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  controller.CloseDisclaimerUi();

  EXPECT_FALSE(controller.disclaimer_widget_for_test());

  // Records the `kShow` metrics.
  histogram_tester->ExpectTotalCount(HistogramName("Total"), 1);
  histogram_tester->ExpectTotalCount(HistogramName("OrcaAndHmr"), 1);
  histogram_tester->ExpectBucketCount(HistogramName("OrcaAndHmr"),
                                      DisclaimerViewAction::kShow, 1);
  histogram_tester->ExpectBucketCount(HistogramName("Total"),
                                      DisclaimerViewAction::kShow, 1);
}

TEST_F(MagicBoostControllerTest, OnDisclaimerAcceptButtonPressed) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(HistogramName("Total"), 0);
  histogram_tester->ExpectTotalCount(HistogramName("HmrOnly"), 0);

  const int64_t display_id = display::Screen::Get()->GetPrimaryDisplay().id();
  controller.ShowDisclaimerUi(display_id,
                              magic_boost::TransitionAction::kDoNothing,
                              magic_boost::OptInFeatures::kHmrOnly);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature).Times(0);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature).Times(0);

  OnDisclaimerAcceptButtonPressed(display_id,
                                  magic_boost::TransitionAction::kDoNothing);

  EXPECT_EQ(chromeos::HMRConsentStatus::kApproved,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_FALSE(controller.disclaimer_widget_for_test());

  // Records the `kAcceptButtonPressed` metrics.
  histogram_tester->ExpectTotalCount(HistogramName("Total"), 2);
  histogram_tester->ExpectBucketCount(HistogramName("HmrOnly"),
                                      DisclaimerViewAction::kShow, 1);
  histogram_tester->ExpectBucketCount(
      HistogramName("HmrOnly"), DisclaimerViewAction::kAcceptButtonPressed, 1);
  histogram_tester->ExpectBucketCount(
      HistogramName("Total"), DisclaimerViewAction::kAcceptButtonPressed, 1);
}

TEST_F(MagicBoostControllerTest, OnDisclaimerAcceptButtonPressedIncludeOrca) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(HistogramName("Total"), 0);
  histogram_tester->ExpectTotalCount(HistogramName("OrcaAndHmr"), 0);

  const int64_t display_id = display::Screen::Get()->GetPrimaryDisplay().id();
  controller.ShowDisclaimerUi(display_id,
                              magic_boost::TransitionAction::kDoNothing,
                              magic_boost::OptInFeatures::kOrcaAndHmr);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature).Times(0);

  OnDisclaimerAcceptButtonPressed(display_id,
                                  magic_boost::TransitionAction::kDoNothing);

  EXPECT_EQ(chromeos::HMRConsentStatus::kApproved,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_FALSE(controller.disclaimer_widget_for_test());

  // Records the `kAcceptButtonPressed` metrics.
  histogram_tester->ExpectTotalCount(HistogramName("Total"), 2);
  histogram_tester->ExpectBucketCount(HistogramName("OrcaAndHmr"),
                                      DisclaimerViewAction::kShow, 1);
  histogram_tester->ExpectBucketCount(
      HistogramName("OrcaAndHmr"), DisclaimerViewAction::kAcceptButtonPressed,
      1);
  histogram_tester->ExpectBucketCount(
      HistogramName("Total"), DisclaimerViewAction::kAcceptButtonPressed, 1);
}

TEST_F(MagicBoostControllerTest,
       OnDisclaimerAcceptButtonPressedIncludeOrcaAndTriggerEditorUI) {
  mock_magic_boost_state_->set_editor_panel_manager_for_test(
      &mock_editor_panel_manager_);

  const int64_t display_id = display::Screen::Get()->GetPrimaryDisplay().id();
  controller.ShowDisclaimerUi(display_id,
                              magic_boost::TransitionAction::kShowEditorPanel,
                              magic_boost::OptInFeatures::kOrcaAndHmr);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature).Times(0);
  EXPECT_CALL(mock_editor_panel_manager(), StartEditingFlow);

  OnDisclaimerAcceptButtonPressed(
      display_id, magic_boost::TransitionAction::kShowEditorPanel);

  EXPECT_EQ(chromeos::HMRConsentStatus::kApproved,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_TRUE(mock_magic_boost_state_->hmr_enabled().value());

  EXPECT_FALSE(controller.disclaimer_widget_for_test());
  testing::Mock::VerifyAndClearExpectations(&mock_editor_panel_manager());
}

TEST_F(MagicBoostControllerTest, OnDisclaimerDeclineButtonPressed) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(HistogramName("Total"), 0);
  histogram_tester->ExpectTotalCount(HistogramName("HmrOnly"), 0);

  controller.ShowDisclaimerUi(display::Screen::Get()->GetPrimaryDisplay().id(),
                              magic_boost::TransitionAction::kDoNothing,
                              magic_boost::OptInFeatures::kHmrOnly);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature).Times(0);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature).Times(0);

  OnDisclaimerDeclineButtonPressed();

  EXPECT_EQ(chromeos::HMRConsentStatus::kDeclined,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_FALSE(mock_magic_boost_state_->hmr_enabled().value());

  EXPECT_FALSE(controller.disclaimer_widget_for_test());

  // Records the `kDeclineButtonPressed` metrics.
  histogram_tester->ExpectTotalCount(HistogramName("Total"), 2);
  histogram_tester->ExpectBucketCount(HistogramName("HmrOnly"),
                                      DisclaimerViewAction::kShow, 1);
  histogram_tester->ExpectBucketCount(
      HistogramName("HmrOnly"), DisclaimerViewAction::kDeclineButtonPressed, 1);
  histogram_tester->ExpectBucketCount(
      HistogramName("Total"), DisclaimerViewAction::kDeclineButtonPressed, 1);
}

TEST_F(MagicBoostControllerTest, OnDisclaimerDeclineButtonPressedIncludeOrca) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(HistogramName("Total"), 0);
  histogram_tester->ExpectTotalCount(HistogramName("OrcaAndHmr"), 0);

  controller.ShowDisclaimerUi(display::Screen::Get()->GetPrimaryDisplay().id(),
                              magic_boost::TransitionAction::kDoNothing,
                              magic_boost::OptInFeatures::kOrcaAndHmr);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature).Times(0);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature);

  OnDisclaimerDeclineButtonPressed();

  EXPECT_EQ(chromeos::HMRConsentStatus::kDeclined,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_FALSE(mock_magic_boost_state_->hmr_enabled().value());

  EXPECT_FALSE(controller.disclaimer_widget_for_test());

  // Records the `kDeclineButtonPressed` metrics.
  histogram_tester->ExpectTotalCount(HistogramName("Total"), 2);
  histogram_tester->ExpectBucketCount(HistogramName("OrcaAndHmr"),
                                      DisclaimerViewAction::kShow, 1);
  histogram_tester->ExpectBucketCount(
      HistogramName("OrcaAndHmr"), DisclaimerViewAction::kDeclineButtonPressed,
      1);
  histogram_tester->ExpectBucketCount(
      HistogramName("Total"), DisclaimerViewAction::kDeclineButtonPressed, 1);
}

TEST_F(MagicBoostControllerTest, ClickingOnLinkClosesWidget) {
  controller.ShowDisclaimerUi(display::Screen::Get()->GetPrimaryDisplay().id(),
                              magic_boost::TransitionAction::kDoNothing,
                              magic_boost::OptInFeatures::kOrcaAndHmr);

  EXPECT_TRUE(controller.disclaimer_widget_for_test());

  OnLinkPressed();

  EXPECT_FALSE(controller.disclaimer_widget_for_test());
}

}  // namespace ash
