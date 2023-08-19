// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/accessibility_feature_pod_controller.h"

#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/view.h"

namespace ash {

// Tests manually control their session state.
class AccessibilityFeaturePodControllerTest
    : public NoSessionAshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AccessibilityFeaturePodControllerTest() {
    if (IsQsRevampEnabled()) {
      feature_list_.InitAndEnableFeature(features::kQsRevamp);
    } else {
      feature_list_.InitAndDisableFeature(features::kQsRevamp);
    }
  }

  AccessibilityFeaturePodControllerTest(
      const AccessibilityFeaturePodControllerTest&) = delete;
  AccessibilityFeaturePodControllerTest& operator=(
      const AccessibilityFeaturePodControllerTest&) = delete;

  ~AccessibilityFeaturePodControllerTest() override = default;

  bool IsQsRevampEnabled() const { return GetParam(); }

  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  void TearDown() override {
    button_.reset();
    controller_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  void SetUpButton() {
    controller_ =
        std::make_unique<AccessibilityFeaturePodController>(tray_controller());
    if (IsQsRevampEnabled()) {
      tile_ = controller_->CreateTile();
    } else {
      button_.reset(controller_->CreateButton());
    }
  }

  AccessibilityControllerImpl* GetAccessibilityController() {
    return Shell::Get()->accessibility_controller();
  }

  FeatureTile* GetFeatureTile() { return tile_.get(); }
  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  bool IsButtonVisible() {
    return IsQsRevampEnabled() ? tile_->GetVisible() : button_->GetVisible();
  }
  void PressIcon() { controller_->OnIconPressed(); }

  void PressLabel() { controller_->OnLabelPressed(); }

  const char* GetToggledOnHistogramName() {
    return IsQsRevampEnabled() ? "Ash.QuickSettings.FeaturePod.ToggledOn"
                               : "Ash.UnifiedSystemView.FeaturePod.ToggledOn";
  }

  const char* GetToggledOffHistogramName() {
    return IsQsRevampEnabled() ? "Ash.QuickSettings.FeaturePod.ToggledOff"
                               : "Ash.UnifiedSystemView.FeaturePod.ToggledOff";
  }

  const char* GetDiveInHistogramName() {
    return IsQsRevampEnabled() ? "Ash.QuickSettings.FeaturePod.DiveIn"
                               : "Ash.UnifiedSystemView.FeaturePod.DiveIn";
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AccessibilityFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_;
  std::unique_ptr<FeatureTile> tile_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         AccessibilityFeaturePodControllerTest,
                         testing::Bool());

TEST_P(AccessibilityFeaturePodControllerTest, ButtonVisibilityNotLoggedIn) {
  SetUpButton();
  // If not logged in, it should be always visible.
  EXPECT_TRUE(IsButtonVisible());
}

TEST_P(AccessibilityFeaturePodControllerTest, ButtonVisibilityLoggedIn) {
  CreateUserSessions(1);
  SetUpButton();
  // If logged in, it's not visible by default.
  EXPECT_FALSE(IsButtonVisible());
}

TEST_P(AccessibilityFeaturePodControllerTest, IconUMATracking) {
  SetUpButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*count=*/0);

  // Show a11y detailed view when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount(GetDiveInHistogramName(),
                                      QsFeatureCatalogName::kAccessibility,
                                      /*expected_count=*/1);
}

TEST_P(AccessibilityFeaturePodControllerTest, LabelUMATracking) {
  SetUpButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*count=*/0);

  // Show a11y detailed view when pressing on the label.
  PressLabel();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount(GetDiveInHistogramName(),
                                      QsFeatureCatalogName::kAccessibility,
                                      /*expected_count=*/1);
}

TEST_P(AccessibilityFeaturePodControllerTest, FeatureTileBasicToggleBehavior) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  SetUpButton();

  EXPECT_FALSE(GetFeatureTile()->IsToggled());

  // Enable an accessibility feature and expect the feature tile to be toggled
  // and the sublabel to be visible.
  GetAccessibilityController()
      ->GetFeature(A11yFeatureType::kHighContrast)
      .SetEnabled(true);
  EXPECT_TRUE(GetFeatureTile()->IsToggled());
  EXPECT_TRUE(GetFeatureTile()->sub_label()->GetVisible());

  // Disable an accessibility feature and expect the feature tile to be
  // untoggled and the sublabel to be invisible.
  GetAccessibilityController()
      ->GetFeature(A11yFeatureType::kHighContrast)
      .SetEnabled(false);
  EXPECT_FALSE(GetFeatureTile()->IsToggled());
  EXPECT_FALSE(GetFeatureTile()->sub_label()->GetVisible());
}

// Toggle all accessibility features one by one and make sure the feature tile
// is updated appropriately.
TEST_P(AccessibilityFeaturePodControllerTest, FeatureTileAllFeaturesToggled) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  SetUpButton();

  for (int type = 0; type != static_cast<int>(A11yFeatureType::kFeatureCount);
       type++) {
    auto& feature = GetAccessibilityController()->GetFeature(
        static_cast<A11yFeatureType>(type));
    feature.SetEnabled(true);
    if (!feature.enabled()) {
      continue;
    }
    if (feature.toggleable_in_quicksettings()) {
      EXPECT_TRUE(GetFeatureTile()->IsToggled());
    } else {
      EXPECT_FALSE(GetFeatureTile()->IsToggled());
    }

    feature.SetEnabled(false);
  }
}

// Enable accessibility features one by one until we have double digits in the
// count shown in the `sub_label`.
TEST_P(AccessibilityFeaturePodControllerTest,
       FeatureTileSubLabelCounterBehavior) {
  if (!IsQsRevampEnabled()) {
    return;
  }

  SetUpButton();

  GetAccessibilityController()
      ->GetFeature(A11yFeatureType::kLargeCursor)
      .SetEnabled(true);
  int expected_count = 0;
  auto feature_types = {
      A11yFeatureType::kCaretHighlight, A11yFeatureType::kCursorHighlight,
      A11yFeatureType::kDictation,      A11yFeatureType::kFocusHighlight,
      A11yFeatureType::kHighContrast,   A11yFeatureType::kMonoAudio,
      A11yFeatureType::kLiveCaption,    A11yFeatureType::kFullscreenMagnifier,
      A11yFeatureType::kStickyKeys,     A11yFeatureType::kSwitchAccess};
  for (A11yFeatureType type : feature_types) {
    auto& feature = GetAccessibilityController()->GetFeature(
        static_cast<A11yFeatureType>(type));

    feature.SetEnabled(true);
    expected_count++;

    EXPECT_TRUE(base::EndsWith(GetFeatureTile()->sub_label()->GetText(),
                               base::NumberToString16(expected_count)));
  }

  for (A11yFeatureType type : feature_types) {
    EXPECT_TRUE(base::EndsWith(GetFeatureTile()->sub_label()->GetText(),
                               base::NumberToString16(expected_count)));

    auto& feature = GetAccessibilityController()->GetFeature(
        static_cast<A11yFeatureType>(type));
    expected_count--;

    feature.SetEnabled(false);
  }
}

}  // namespace ash
