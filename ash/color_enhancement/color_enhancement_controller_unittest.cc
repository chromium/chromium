// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/color_enhancement/color_enhancement_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/compositor/layer.h"

namespace ash {

// Tests that the color enhancement controller sets the appropriate values
// on the root window layer and updates cursor compositing as needed.
class ColorEnhancementControllerTest : public AshTestBase {
 public:
  ColorEnhancementControllerTest() = default;

  ColorEnhancementControllerTest(const ColorEnhancementControllerTest&) =
      delete;
  ColorEnhancementControllerTest& operator=(
      const ColorEnhancementControllerTest&) = delete;

  ~ColorEnhancementControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        ::features::kExperimentalAccessibilityColorEnhancementSettings, true);

    AshTestBase::SetUp();
  }

  bool IsCursorCompositingEnabled() const {
    return Shell::Get()
        ->window_tree_host_manager()
        ->cursor_window_controller()
        ->is_cursor_compositing_enabled();
  }

  PrefService* GetPrefs() const {
    return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ColorEnhancementControllerTest, HighContrast) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityHighContrastEnabled, true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_TRUE(root_window->layer()->layer_inverted());
  }
  prefs->SetBoolean(prefs::kAccessibilityHighContrastEnabled, false);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FALSE(root_window->layer()->layer_inverted());
  }
  EXPECT_FALSE(IsCursorCompositingEnabled());
}

TEST_F(ColorEnhancementControllerTest, Greyscale) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityColorFiltering, true);
  prefs->SetInteger(prefs::kAccessibilityGreyscaleAmount, 0);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.f, root_window->layer()->layer_grayscale());
  }

  prefs->SetInteger(prefs::kAccessibilityGreyscaleAmount, 100);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(1, root_window->layer()->layer_grayscale());
  }

  prefs->SetInteger(prefs::kAccessibilityGreyscaleAmount, 50);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_grayscale());
  }

  // Greyscale larger than 100% or smaller than 0% does nothing.
  prefs->SetInteger(prefs::kAccessibilityGreyscaleAmount, 500);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_grayscale());
  }

  prefs->SetInteger(prefs::kAccessibilityGreyscaleAmount, -10);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_grayscale());
  }
}

TEST_F(ColorEnhancementControllerTest, Saturation) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityColorFiltering, true);
  prefs->SetInteger(prefs::kAccessibilitySaturationAmount, 50);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_saturation());
  }

  prefs->SetInteger(prefs::kAccessibilitySaturationAmount, 500);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(5.f, root_window->layer()->layer_saturation());
  }

  // Saturation smaller than 0% does nothing.
  prefs->SetInteger(prefs::kAccessibilityGreyscaleAmount, -100);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(5.f, root_window->layer()->layer_saturation());
  }
}

TEST_F(ColorEnhancementControllerTest, HueRotation) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityColorFiltering, true);
  prefs->SetInteger(prefs::kAccessibilityHueRotationAmount, 42);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(42.f, root_window->layer()->layer_hue_rotation());
  }

  prefs->SetInteger(prefs::kAccessibilityHueRotationAmount, 180);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(180.f, root_window->layer()->layer_hue_rotation());
  }

  // Hue rotation greater than 359 or smaller than 0 does nothing.
  prefs->SetInteger(prefs::kAccessibilityHueRotationAmount, 1972);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(180.f, root_window->layer()->layer_hue_rotation());
  }
  prefs->SetInteger(prefs::kAccessibilityHueRotationAmount, -10);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(180.f, root_window->layer()->layer_hue_rotation());
  }
}

TEST_F(ColorEnhancementControllerTest, Sepia) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityColorFiltering, true);
  prefs->SetInteger(prefs::kAccessibilitySepiaAmount, 10);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.1f, root_window->layer()->layer_sepia());
  }

  prefs->SetInteger(prefs::kAccessibilitySepiaAmount, 99);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.99f, root_window->layer()->layer_sepia());
  }

  prefs->SetInteger(prefs::kAccessibilitySepiaAmount, 100);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(1.0f, root_window->layer()->layer_sepia());
  }

  prefs->SetInteger(prefs::kAccessibilitySepiaAmount, 50);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_sepia());
  }

  prefs->SetInteger(prefs::kAccessibilitySepiaAmount, 0);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_sepia());
  }

  // Sepia smaller than 0 or lareger than 100% does nothing.
  prefs->SetInteger(prefs::kAccessibilitySepiaAmount, -10);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_sepia());
  }

  prefs->SetInteger(prefs::kAccessibilitySepiaAmount, 150);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_sepia());
  }
}

TEST_F(ColorEnhancementControllerTest, ColorVisionDeficiencyFilters) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityColorFiltering, true);

  // Try for each of the color deficiency types.
  for (int i = 0; i < 3; i++) {
    prefs->SetInteger(prefs::kAccessibilityColorVisionDeficiencyType, i);

    // With severity at 0, no matrix should be applied.
    prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 0);
    for (auto* root_window : Shell::GetAllRootWindows()) {
      EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
    }

    // With a non-zero severity, a matrix should be applied.
    prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 50);
    for (auto* root_window : Shell::GetAllRootWindows()) {
      EXPECT_TRUE(root_window->layer()->LayerHasCustomColorMatrix());
    }
    prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 100);
    for (auto* root_window : Shell::GetAllRootWindows()) {
      const cc::FilterOperation::Matrix* matrix =
          root_window->layer()->GetLayerCustomColorMatrix();
      EXPECT_TRUE(matrix);
      // For protanopes (i == 1), the first row in the resulting matrix should
      // have a 1 for red and a zero for the other colors. Similarly with
      // deuteranopes (i == 2) and tritanopes (i == 3). This ensures we are
      // correcting around the right axis.
      for (int j = 0; j < 3; j++) {
        if (i == j) {
          EXPECT_EQ(1, matrix->at(i * 5 + j));
        } else {
          EXPECT_EQ(0, matrix->at(i * 5 + j));
        }
      }
    }
  }
}

TEST_F(ColorEnhancementControllerTest, ColorFiltersBehindColorFilteringOption) {
  PrefService* prefs = GetPrefs();
  // Color filtering off.
  prefs->SetBoolean(prefs::kAccessibilityColorFiltering, false);
  prefs->SetInteger(prefs::kAccessibilitySepiaAmount, 10);
  prefs->SetInteger(prefs::kAccessibilityGreyscaleAmount, 50);
  prefs->SetInteger(prefs::kAccessibilitySaturationAmount, 50);
  prefs->SetInteger(prefs::kAccessibilityHueRotationAmount, 42);

  // Default values.
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_sepia());
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_grayscale());
    EXPECT_FLOAT_EQ(1.0f, root_window->layer()->layer_saturation());
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_hue_rotation());
    EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
  }

  // Turn on color filtering, values should now be from prefs.
  prefs->SetBoolean(prefs::kAccessibilityColorFiltering, true);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.1f, root_window->layer()->layer_sepia());
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_grayscale());
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_saturation());
    EXPECT_FLOAT_EQ(42.f, root_window->layer()->layer_hue_rotation());
    EXPECT_TRUE(root_window->layer()->LayerHasCustomColorMatrix());
  }

  // Turn it off again, expect defaults to be restored.
  prefs->SetBoolean(prefs::kAccessibilityColorFiltering, false);
  for (auto* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_sepia());
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_grayscale());
    EXPECT_FLOAT_EQ(1.0f, root_window->layer()->layer_saturation());
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_hue_rotation());
    EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
  }
}
}  // namespace ash
