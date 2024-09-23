// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/accessibility_feature_pod_controller.h"

#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/base/metadata/metadata_utils.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

// Tests manually control their session state.
class AccessibilityFeaturePodControllerTest : public NoSessionAshTestBase {
 public:
  AccessibilityFeaturePodControllerTest() = default;

  AccessibilityFeaturePodControllerTest(
      const AccessibilityFeaturePodControllerTest&) = delete;
  AccessibilityFeaturePodControllerTest& operator=(
      const AccessibilityFeaturePodControllerTest&) = delete;

  ~AccessibilityFeaturePodControllerTest() override = default;

  void TearDown() override {
    GetPrimaryUnifiedSystemTray()->CloseBubble();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  AccessibilityController* GetAccessibilityController() {
    return Shell::Get()->accessibility_controller();
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  void PressIcon() {
    for (auto& controller : tray_controller()->feature_pod_controllers_) {
      if (controller->GetCatalogName() ==
          QsFeatureCatalogName::kAccessibility) {
        controller->OnIconPressed();
        return;
      }
    }
  }

  void PressLabel() {
    for (auto& controller : tray_controller()->feature_pod_controllers_) {
      if (controller->GetCatalogName() ==
          QsFeatureCatalogName::kAccessibility) {
        controller->OnLabelPressed();
        return;
      }
    }
  }

  const char* GetToggledOnHistogramName() {
    return "Ash.QuickSettings.FeaturePod.ToggledOn";
  }

  const char* GetToggledOffHistogramName() {
    return "Ash.QuickSettings.FeaturePod.ToggledOff";
  }

  const char* GetDiveInHistogramName() {
    return "Ash.QuickSettings.FeaturePod.DiveIn";
  }
};

TEST_F(AccessibilityFeaturePodControllerTest, ButtonVisibilityNotLoggedIn) {
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  auto* tile = views::AsViewClass<FeatureTile>(
      GetPrimaryUnifiedSystemTray()->bubble()->GetBubbleView()->GetViewByID(
          VIEW_ID_FEATURE_TILE_ACCESSIBILITY));
  // If not logged in, it should be always visible.
  EXPECT_TRUE(tile->GetVisible());
}

TEST_F(AccessibilityFeaturePodControllerTest, ButtonVisibilityLoggedIn) {
  CreateUserSessions(1);
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  auto* tile = views::AsViewClass<FeatureTile>(
      GetPrimaryUnifiedSystemTray()->bubble()->GetBubbleView()->GetViewByID(
          VIEW_ID_FEATURE_TILE_ACCESSIBILITY));
  // If logged in, it's not visible by default.
  EXPECT_FALSE(tile->GetVisible());
}

TEST_F(AccessibilityFeaturePodControllerTest, IconUMATracking) {
  GetPrimaryUnifiedSystemTray()->ShowBubble();
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
  GetPrimaryUnifiedSystemTray()->ShowBubble();
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
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  auto* tile = views::AsViewClass<FeatureTile>(
      GetPrimaryUnifiedSystemTray()->bubble()->GetBubbleView()->GetViewByID(
          VIEW_ID_FEATURE_TILE_ACCESSIBILITY));
  EXPECT_FALSE(tile->IsToggled());

  // Enable an accessibility feature and expect the feature tile to be toggled
  // and the sublabel to be visible.
  GetAccessibilityController()
      ->GetFeature(A11yFeatureType::kHighContrast)
      .SetEnabled(true);
  EXPECT_TRUE(tile->IsToggled());
  EXPECT_TRUE(tile->sub_label()->GetVisible());

  // Disable an accessibility feature and expect the feature tile to be
  // untoggled and the sublabel to be invisible.
  GetAccessibilityController()
      ->GetFeature(A11yFeatureType::kHighContrast)
      .SetEnabled(false);
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(tile->sub_label()->GetVisible());
}

TEST_F(AccessibilityFeaturePodControllerTest, WithMultipleFeatureToggled) {
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  auto* tile = views::AsViewClass<FeatureTile>(
      GetPrimaryUnifiedSystemTray()->bubble()->GetBubbleView()->GetViewByID(
          VIEW_ID_FEATURE_TILE_ACCESSIBILITY));

  EXPECT_FALSE(tile->IsToggled());

  // Enable an accessibility feature and expect the feature tile to be toggled
  // and the sublabel to be visible.
  GetAccessibilityController()
      ->GetFeature(A11yFeatureType::kSpokenFeedback)
      .SetEnabled(true);

  EXPECT_TRUE(tile->IsToggled());
  EXPECT_TRUE(tile->sub_label()->GetVisible());
  EXPECT_EQ(tile->sub_label()->GetText(), u"ChromeVox (spoken feedback)");

  GetAccessibilityController()
      ->GetFeature(A11yFeatureType::kDockedMagnifier)
      .SetEnabled(true);
  EXPECT_TRUE(tile->IsToggled());
  EXPECT_TRUE(tile->sub_label()->GetVisible());
  // Here we only check the "...", which is u"\x2026", and the trailing count
  // are created. We don't check the first specific a11y name, since the a11y
  // names are stored in a map so the order of them is not fixed.
  EXPECT_TRUE(base::Contains(tile->sub_label()->GetText(), u"\x2026, +1"));

  GetAccessibilityController()
      ->GetFeature(A11yFeatureType::kVirtualKeyboard)
      .SetEnabled(true);
  EXPECT_TRUE(tile->IsToggled());
  EXPECT_TRUE(tile->sub_label()->GetVisible());
  EXPECT_TRUE(base::Contains(tile->sub_label()->GetText(), u"\x2026, +2"));
}

// Toggle all accessibility features one by one and make sure the feature tile
// is updated appropriately.
TEST_F(AccessibilityFeaturePodControllerTest, FeatureTileAllFeaturesToggled) {
  // Disables the features that have confirmation dialog when enabling/disabling
  // them. Otherwise they will block the other tests after them.
  GetAccessibilityController()->DisableAutoClickConfirmationDialogForTest();
  GetAccessibilityController()
      ->DisableSwitchAccessDisableConfirmationDialogTesting();
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  auto* tile = views::AsViewClass<FeatureTile>(
      GetPrimaryUnifiedSystemTray()->bubble()->GetBubbleView()->GetViewByID(
          VIEW_ID_FEATURE_TILE_ACCESSIBILITY));

  EXPECT_FALSE(tile->IsToggled());

  for (int type = 0; type != static_cast<int>(A11yFeatureType::kFeatureCount);
       type++) {
    auto& feature = GetAccessibilityController()->GetFeature(
        static_cast<A11yFeatureType>(type));
    feature.SetEnabled(true);
    if (!feature.enabled()) {
      continue;
    }

    if (feature.toggleable_in_quicksettings()) {
      EXPECT_TRUE(tile->IsToggled());
    } else {
      EXPECT_FALSE(tile->IsToggled());
    }

    feature.SetEnabled(false);
  }
}

// Enable accessibility features one by one until we have double digits in the
// count shown in the `sub_label`.
TEST_F(AccessibilityFeaturePodControllerTest,
       FeatureTileSubLabelCounterBehavior) {
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  auto* tile = views::AsViewClass<FeatureTile>(
      GetPrimaryUnifiedSystemTray()->bubble()->GetBubbleView()->GetViewByID(
          VIEW_ID_FEATURE_TILE_ACCESSIBILITY));

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

    EXPECT_TRUE(base::EndsWith(tile->sub_label()->GetText(),
                               base::NumberToString16(expected_count)));
  }

  for (A11yFeatureType type : feature_types) {
    EXPECT_TRUE(base::EndsWith(tile->sub_label()->GetText(),
                               base::NumberToString16(expected_count)));

    auto& feature = GetAccessibilityController()->GetFeature(
        static_cast<A11yFeatureType>(type));
    expected_count--;

    feature.SetEnabled(false);
  }
}

}  // namespace ash
