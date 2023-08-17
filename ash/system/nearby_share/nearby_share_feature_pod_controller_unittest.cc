// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_feature_pod_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

// Tests manually control their session state.
class NearbyShareFeaturePodControllerTest
    : public NoSessionAshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  NearbyShareFeaturePodControllerTest() = default;
  NearbyShareFeaturePodControllerTest(NearbyShareFeaturePodControllerTest&) =
      delete;
  NearbyShareFeaturePodControllerTest& operator=(
      NearbyShareFeaturePodControllerTest&) = delete;
  ~NearbyShareFeaturePodControllerTest() override = default;

  bool IsQsRevampEnabled() const { return GetParam(); }

  void SetUp() override {
    if (IsQsRevampEnabled()) {
      feature_list_.InitAndEnableFeature(features::kQsRevamp);
    } else {
      feature_list_.InitAndDisableFeature(features::kQsRevamp);
    }

    NoSessionAshTestBase::SetUp();

    test_delegate_ = static_cast<TestNearbyShareDelegate*>(
        Shell::Get()->nearby_share_delegate());
    nearby_share_controller_ = Shell::Get()->nearby_share_controller();

    test_delegate_->set_is_pod_button_visible(true);

    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  void TearDown() override {
    if (IsQsRevampEnabled()) {
      tile_.reset();
    } else {
      button_.reset();
    }
    pod_controller_.reset();
    NoSessionAshTestBase::TearDown();
  }

  bool IsButtonVisible() {
    return IsQsRevampEnabled() ? tile_->GetVisible() : button_->GetVisible();
  }

  bool IsButtonToggled() {
    return IsQsRevampEnabled() ? tile_->IsToggled() : button_->IsToggled();
  }

 protected:
  void SetUpButton() {
    pod_controller_ =
        std::make_unique<NearbyShareFeaturePodController>(tray_controller());
    if (IsQsRevampEnabled()) {
      tile_ = pod_controller_->CreateTile();
    } else {
      button_.reset(pod_controller_->CreateButton());
    }
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  void PressIcon() { pod_controller_->OnIconPressed(); }

  void PressLabel() { pod_controller_->OnLabelPressed(); }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<NearbyShareFeaturePodController> pod_controller_;
  std::unique_ptr<FeaturePodButton> button_;
  std::unique_ptr<FeatureTile> tile_;

  raw_ptr<TestNearbyShareDelegate, DanglingUntriaged | ExperimentalAsh>
      test_delegate_ = nullptr;
  raw_ptr<NearbyShareController, DanglingUntriaged | ExperimentalAsh>
      nearby_share_controller_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         NearbyShareFeaturePodControllerTest,
                         testing::Bool());

TEST_P(NearbyShareFeaturePodControllerTest, ButtonVisibilityNotLoggedIn) {
  SetUpButton();
  // If not logged in, it should not be visible.
  EXPECT_FALSE(IsButtonVisible());
}

TEST_P(NearbyShareFeaturePodControllerTest, ButtonVisibilityLoggedIn) {
  CreateUserSessions(1);
  SetUpButton();
  // If logged in, it should be visible.
  EXPECT_TRUE(IsButtonVisible());
}

TEST_P(NearbyShareFeaturePodControllerTest, ButtonVisibilityLocked) {
  CreateUserSessions(1);
  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOCK_SCREEN);

  // Showing the lock screen closes the system tray bubble, so re-show it before
  // setting up the button.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  SetUpButton();

  // If locked, it should not be visible.
  EXPECT_FALSE(IsButtonVisible());
}

TEST_P(NearbyShareFeaturePodControllerTest, ButtonVisibilityLoginScreen) {
  CreateUserSessions(1);
  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOGIN_SCREEN);
  SetUpButton();
  // If the login screen is showing (e.g. multi-user signin), it should not be
  // visible, regardless of whether an active user is signed in.
  EXPECT_FALSE(IsButtonVisible());
}

TEST_P(NearbyShareFeaturePodControllerTest, ButtonVisiblilityHiddenByDelegate) {
  CreateUserSessions(1);
  test_delegate_->set_is_pod_button_visible(false);
  SetUpButton();
  // If NearbyShareDelegate::IsPodButtonVisible() returns false, it should
  // not be visible.
  EXPECT_FALSE(IsButtonVisible());
}

TEST_P(NearbyShareFeaturePodControllerTest,
       ButtonToggledByHighVisibilityEnabledEvent) {
  CreateUserSessions(1);
  SetUpButton();
  ASSERT_FALSE(IsButtonToggled());
  nearby_share_controller_->HighVisibilityEnabledChanged(true);
  EXPECT_TRUE(IsButtonToggled());
  nearby_share_controller_->HighVisibilityEnabledChanged(false);
  EXPECT_FALSE(IsButtonToggled());
}

TEST_P(NearbyShareFeaturePodControllerTest, ButtonPressTogglesHighVisibility) {
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

TEST_P(NearbyShareFeaturePodControllerTest, IconUMATracking) {
  CreateUserSessions(1);
  SetUpButton();

  std::string histogram_prefix;
  if (IsQsRevampEnabled()) {
    histogram_prefix = "Ash.QuickSettings.FeaturePod.";
  } else {
    histogram_prefix = "Ash.UnifiedSystemView.FeaturePod.";
  }

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(histogram_prefix + "ToggledOn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(histogram_prefix + "ToggledOff",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(histogram_prefix + "DiveIn",
                                     /*expected_count=*/0);

  // Toggle on nearby share feature when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(histogram_prefix + "ToggledOn",
                                     /*expected_count=*/1);
  histogram_tester->ExpectTotalCount(histogram_prefix + "ToggledOff",
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(histogram_prefix + "DiveIn",
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount(histogram_prefix + "ToggledOn",
                                      QsFeatureCatalogName::kNearbyShare,
                                      /*expected_count=*/1);
}

TEST_P(NearbyShareFeaturePodControllerTest, LabelUMATracking) {
  // Revamped view does not have `OnLabelPressed` action.
  if (IsQsRevampEnabled()) {
    return;
  }

  CreateUserSessions(1);
  SetUpButton();
  nearby_share_controller_->HighVisibilityEnabledChanged(true);

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*expected_count=*/0);

  // Show nearby share detailed view (setting) when pressing on the label.
  PressLabel();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*expected_count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kNearbyShare,
                                      /*expected_count=*/1);
}

TEST_P(NearbyShareFeaturePodControllerTest, ButtonEnabledStateVisibility) {
  CreateUserSessions(1);
  test_delegate_->set_is_enabled(false);
  SetUpButton();
  // If NearbyShareDelegate::IsEnabled() returns false, the button should
  // not be visible in the revamped view.
  if (IsQsRevampEnabled()) {
    EXPECT_FALSE(IsButtonVisible());
  } else {
    EXPECT_TRUE(IsButtonVisible());
  }
}

}  // namespace ash
