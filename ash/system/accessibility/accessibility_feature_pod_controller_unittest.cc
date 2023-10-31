// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/accessibility_feature_pod_controller.h"

#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/views/view.h"

namespace ash {

// Tests manually control their session state.
class AccessibilityFeaturePodControllerTest
    : public NoSessionAshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AccessibilityFeaturePodControllerTest() = default;

  AccessibilityFeaturePodControllerTest(
      const AccessibilityFeaturePodControllerTest&) = delete;
  AccessibilityFeaturePodControllerTest& operator=(
      const AccessibilityFeaturePodControllerTest&) = delete;

  ~AccessibilityFeaturePodControllerTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  void TearDown() override {
    controller_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  void SetUpButton() {
    controller_ =
        std::make_unique<AccessibilityFeaturePodController>(tray_controller());
    tile_ = controller_->CreateTile();
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

  bool IsButtonVisible() { return tile_->GetVisible(); }
  void PressIcon() { controller_->OnIconPressed(); }

  void PressLabel() { controller_->OnLabelPressed(); }

  const char* GetToggledOnHistogramName() {
    return "Ash.QuickSettings.FeaturePod.ToggledOn";
  }

  const char* GetToggledOffHistogramName() {
    return "Ash.QuickSettings.FeaturePod.ToggledOff";
  }

  const char* GetDiveInHistogramName() {
    return "Ash.QuickSettings.FeaturePod.DiveIn";
  }

 private:
  std::unique_ptr<AccessibilityFeaturePodController> controller_;
  std::unique_ptr<FeatureTile> tile_;
};

TEST_F(AccessibilityFeaturePodControllerTest, ButtonVisibilityNotLoggedIn) {
  SetUpButton();
  // If not logged in, it should be always visible.
  EXPECT_TRUE(IsButtonVisible());
}

TEST_F(AccessibilityFeaturePodControllerTest, ButtonVisibilityLoggedIn) {
  CreateUserSessions(1);
  SetUpButton();
  // If logged in, it's not visible by default.
  EXPECT_FALSE(IsButtonVisible());
}

TEST_F(AccessibilityFeaturePodControllerTest, IconUMATracking) {
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

TEST_F(AccessibilityFeaturePodControllerTest, LabelUMATracking) {
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

TEST_F(AccessibilityFeaturePodControllerTest, FeatureTileBasicToggleBehavior) {
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
TEST_F(AccessibilityFeaturePodControllerTest, FeatureTileAllFeaturesToggled) {
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
TEST_F(AccessibilityFeaturePodControllerTest,
       FeatureTileSubLabelCounterBehavior) {
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
