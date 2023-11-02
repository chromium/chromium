// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_feature_pod_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"

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

    test_delegate_ = static_cast<TestNearbyShareDelegate*>(
        Shell::Get()->nearby_share_delegate());
    nearby_share_controller_ = Shell::Get()->nearby_share_controller();

    test_delegate_->set_is_pod_button_visible(true);

    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  void TearDown() override {
    button_.reset();
    pod_controller_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  void SetUpButton() {
    pod_controller_ =
        std::make_unique<NearbyShareFeaturePodController>(tray_controller());
    button_.reset(pod_controller_->CreateButton());
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  FeaturePodButton* button() { return button_.get(); }

  void PressIcon() { pod_controller_->OnIconPressed(); }

  void PressLabel() { pod_controller_->OnLabelPressed(); }

  std::unique_ptr<NearbyShareFeaturePodController> pod_controller_;
  std::unique_ptr<FeaturePodButton> button_;

  TestNearbyShareDelegate* test_delegate_ = nullptr;
  NearbyShareController* nearby_share_controller_ = nullptr;
};

TEST_F(NearbyShareFeaturePodControllerTest, ButtonVisibilityNotLoggedIn) {
  SetUpButton();
  // If not logged in, it should not be visible.
  EXPECT_FALSE(button()->GetVisible());
}

TEST_F(NearbyShareFeaturePodControllerTest, ButtonVisibilityLoggedIn) {
  CreateUserSessions(1);
  SetUpButton();
  // If logged in, it should be visible.
  EXPECT_TRUE(button()->GetVisible());
}

TEST_F(NearbyShareFeaturePodControllerTest, ButtonVisibilityLocked) {
  CreateUserSessions(1);
  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOCK_SCREEN);
  SetUpButton();
  // If locked, it should not be visible.
  EXPECT_FALSE(button()->GetVisible());
}

TEST_F(NearbyShareFeaturePodControllerTest, ButtonVisibilityLoginScreen) {
  CreateUserSessions(1);
  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOGIN_SCREEN);
  SetUpButton();
  // If the login screen is showing (e.g. multi-user signin), it should not be
  // visible, regardless of whether an active user is signed in.
  EXPECT_FALSE(button()->GetVisible());
}

TEST_F(NearbyShareFeaturePodControllerTest, ButtonVisiblilityHiddenByDelegate) {
  CreateUserSessions(1);
  test_delegate_->set_is_pod_button_visible(false);
  SetUpButton();
  // If NearbyShareDelegate::IsPodButtonVisible() returns false, it should
  // not be visible.
  EXPECT_FALSE(button()->GetVisible());
}

TEST_F(NearbyShareFeaturePodControllerTest,
       ButtonToggledByHighVisibilityEnabledEvent) {
  CreateUserSessions(1);
  SetUpButton();
  ASSERT_FALSE(button()->IsToggled());
  nearby_share_controller_->HighVisibilityEnabledChanged(true);
  EXPECT_TRUE(button()->IsToggled());
  nearby_share_controller_->HighVisibilityEnabledChanged(false);
  EXPECT_FALSE(button()->IsToggled());
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

TEST_F(NearbyShareFeaturePodControllerTest, IconUMATracking) {
  CreateUserSessions(1);
  SetUpButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Toggle on nearby share feature when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      QsFeatureCatalogName::kNearbyShare,
      /*expected_count=*/1);
}

TEST_F(NearbyShareFeaturePodControllerTest, LabelUMATracking) {
  CreateUserSessions(1);
  SetUpButton();
  nearby_share_controller_->HighVisibilityEnabledChanged(true);

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Show nearby share detailed view (setting) when pressing on the label.
  PressLabel();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kNearbyShare,
                                      /*expected_count=*/1);
}

}  // namespace ash
