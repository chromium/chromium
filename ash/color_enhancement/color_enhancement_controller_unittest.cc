// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/color_enhancement/color_enhancement_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/flash_screen_controller.h"
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
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {
const int kDelayToCheckStateChangeMs = 300;
const int kBlueColor = 0x0000ff;

// Indices in a FilterOperation::Matrix that map a channel to itself.
const int kRedToRedIndex = 0;
const int kGreenToGreenIndex = 6;
const int kBlueToBlueIndex = 12;
const int kAlphaToAlphaIndex = 18;
}  // namespace

// Tests that the color enhancement controller sets the appropriate values
// on the root window layer and updates cursor compositing as needed.
class ColorEnhancementControllerTest : public AshTestBase {
 public:
  ColorEnhancementControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ColorEnhancementControllerTest(const ColorEnhancementControllerTest&) =
      delete;
  ColorEnhancementControllerTest& operator=(
      const ColorEnhancementControllerTest&) = delete;

  ~ColorEnhancementControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityFlashScreenFeature);
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

  void FastForwardAnimationTo(int milliseconds) {
    gfx::AnimationTestApi animation_api(
        AccessibilityController::Get()
            ->GetFlashScreenControllerForTesting()
            ->GetAnimationForTesting());
    base::TimeTicks now = base::TimeTicks::Now();
    animation_api.SetStartTime(now);
    animation_api.Step(now + base::Milliseconds(milliseconds));
  }

  void ShowNotification() {
    const std::string notification_id("id");
    const std::string notification_title("title");
    message_center::MessageCenter::Get()->AddNotification(
        std::make_unique<message_center::Notification>(
            message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
            base::UTF8ToUTF16(notification_title), u"test message",
            ui::ImageModel(), std::u16string() /* display_source */, GURL(),
            message_center::NotifierId(),
            message_center::RichNotificationData(),
            new message_center::NotificationDelegate()));
  }

  void ExpectNoCustomColorMatrix() {
    for (aura::Window* root_window : Shell::GetAllRootWindows()) {
      const cc::FilterOperation::Matrix* matrix =
          root_window->layer()->GetLayerCustomColorMatrix();
      EXPECT_FALSE(matrix);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ColorEnhancementControllerTest, HighContrast) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityHighContrastEnabled, true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_TRUE(root_window->layer()->layer_inverted());
  }
  prefs->SetBoolean(prefs::kAccessibilityHighContrastEnabled, false);
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FALSE(root_window->layer()->layer_inverted());
  }
  EXPECT_FALSE(IsCursorCompositingEnabled());
}

TEST_F(ColorEnhancementControllerTest, Greyscale) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityColorCorrectionEnabled, true);
  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 0);
  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionType,
                    ColorVisionCorrectionType::kGrayscale);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.f, root_window->layer()->layer_grayscale());
    // No other color filters were set.
    EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
  }

  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 100);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(1, root_window->layer()->layer_grayscale());
    // No other color filters were set.
    EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
  }

  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 50);
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_grayscale());
    // No other color filters were set.
    EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
  }

  // Greyscale larger than 100% or smaller than 0% does nothing.
  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 500);
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_grayscale());
    // No other color filters were set.
    EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
  }

  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, -10);
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_grayscale());
    // No other color filters were set.
    EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
  }
}

TEST_F(ColorEnhancementControllerTest, ColorVisionCorrectionFilters) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityColorCorrectionEnabled, true);

  // Try for each of the color correction types.
  for (int i = 0; i < 3; i++) {
    prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionType, i);

    // With severity at 0, no matrix should be applied.
    prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 0);
    for (aura::Window* root_window : Shell::GetAllRootWindows()) {
      EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
      EXPECT_FLOAT_EQ(0.f, root_window->layer()->layer_grayscale());
    }

    // With a non-zero severity, a matrix should be applied.
    prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 50);
    for (aura::Window* root_window : Shell::GetAllRootWindows()) {
      EXPECT_TRUE(root_window->layer()->LayerHasCustomColorMatrix());
      // Grayscale was not impacted.
      EXPECT_FLOAT_EQ(0.f, root_window->layer()->layer_grayscale());
    }
    prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 100);
    for (aura::Window* root_window : Shell::GetAllRootWindows()) {
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

TEST_F(ColorEnhancementControllerTest, GrayscaleBehindColorCorrectionOption) {
  PrefService* prefs = GetPrefs();
  // Color filtering off.
  prefs->SetBoolean(prefs::kAccessibilityColorCorrectionEnabled, false);
  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 50);
  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionType,
                    ColorVisionCorrectionType::kGrayscale);

  // Default values.
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_grayscale());
    EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
  }

  // Turn on color filtering, values should now be from prefs.
  prefs->SetBoolean(prefs::kAccessibilityColorCorrectionEnabled, true);
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.5f, root_window->layer()->layer_grayscale());
    EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
  }

  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionType,
                    ColorVisionCorrectionType::kDeuteranomaly);
  prefs->SetBoolean(prefs::kAccessibilityColorCorrectionEnabled, true);
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_grayscale());
    EXPECT_TRUE(root_window->layer()->LayerHasCustomColorMatrix());
  }

  // Turn it off again, expect defaults to be restored.
  prefs->SetBoolean(prefs::kAccessibilityColorCorrectionEnabled, false);
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FLOAT_EQ(0.0f, root_window->layer()->layer_grayscale());
    EXPECT_FALSE(root_window->layer()->LayerHasCustomColorMatrix());
  }
}

TEST_F(ColorEnhancementControllerTest, FlashNotifications) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityFlashNotificationsEnabled, true);

  // Show a normal notification. Flashing should occur.
  ShowNotification();

  // The color should be shown for kDelayToCheckStateChangeMs duration, then be
  // off.
  for (int i = 1; i < kDelayToCheckStateChangeMs; i++) {
    FastForwardAnimationTo(i);
    // A custom color matrix has been shown.
    for (aura::Window* root_window : Shell::GetAllRootWindows()) {
      const cc::FilterOperation::Matrix* matrix =
          root_window->layer()->GetLayerCustomColorMatrix();
      ASSERT_TRUE(matrix);
      // The default flash color is yellow, which has r == g == 1.0, b = 0.0,
      // and alpha = 1.0. b however is not set to 0.0 since we use 30% of the
      // original color.
      EXPECT_EQ((*matrix)[kRedToRedIndex], 1.0f);
      EXPECT_EQ((*matrix)[kGreenToGreenIndex], 1.0f);
      EXPECT_NE((*matrix)[kBlueToBlueIndex], 1.0f);
      EXPECT_NE((*matrix)[kBlueToBlueIndex], 0.0f);
      EXPECT_EQ((*matrix)[kAlphaToAlphaIndex], 1.0f);
    }
  }
  FastForwardAnimationTo(kDelayToCheckStateChangeMs);

  // Off: Completed a throb animation.
  ExpectNoCustomColorMatrix();

  // Note: The throb animation does not play well with the AnimationTestAPI so
  // we can only test the first throb cycle.
}

TEST_F(ColorEnhancementControllerTest, FlashNotificationsColorPref) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityFlashNotificationsEnabled, true);

  // Set the color to blue.
  prefs->SetInteger(prefs::kAccessibilityFlashNotificationsColor, 0x0000ff);

  // Show a normal notification. Flashing should occur.
  ShowNotification();
  FastForwardAnimationTo(1);

  // A custom color matrix has been shown.
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    const cc::FilterOperation::Matrix* matrix =
        root_window->layer()->GetLayerCustomColorMatrix();
    ASSERT_TRUE(matrix);
    // Blue has r == g !== 1.0, b = 1.0, and alpha = 1.0.
    EXPECT_NE((*matrix)[kRedToRedIndex], 1.0f);
    EXPECT_NE((*matrix)[kGreenToGreenIndex], 1.0f);
    EXPECT_EQ((*matrix)[kRedToRedIndex], (*matrix)[kGreenToGreenIndex]);
    EXPECT_EQ((*matrix)[kBlueToBlueIndex], 1.0f);
    EXPECT_EQ((*matrix)[kAlphaToAlphaIndex], 1.0f);
  }

  FastForwardAnimationTo(kDelayToCheckStateChangeMs);

  // Should be off now.
  ExpectNoCustomColorMatrix();
}

TEST_F(ColorEnhancementControllerTest,
       FlashNotificationsResetsColorCorrectionOnComplete) {
  PrefService* prefs = GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityFlashNotificationsEnabled, true);
  prefs->SetInteger(prefs::kAccessibilityFlashNotificationsColor, kBlueColor);

  prefs->SetBoolean(prefs::kAccessibilityColorCorrectionEnabled, true);
  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionAmount, 50);
  prefs->SetInteger(prefs::kAccessibilityColorVisionCorrectionType,
                    ColorVisionCorrectionType::kDeuteranomaly);

  // Color correction matrix is set.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  const cc::FilterOperation::Matrix* color_correction_matrix =
      root_window->layer()->GetLayerCustomColorMatrix();
  ASSERT_TRUE(color_correction_matrix);
  float r = (*color_correction_matrix)[kRedToRedIndex];
  float g = (*color_correction_matrix)[kGreenToGreenIndex];
  float b = (*color_correction_matrix)[kBlueToBlueIndex];

  // Show a normal notification. Flashing should occur.
  ShowNotification();
  FastForwardAnimationTo(1);

  // Overlay color matrix is shown instead.
  const cc::FilterOperation::Matrix* matrix =
      root_window->layer()->GetLayerCustomColorMatrix();
  ASSERT_TRUE(matrix);
  EXPECT_NE(r, (*matrix)[kRedToRedIndex]);
  EXPECT_NE(g, (*matrix)[kGreenToGreenIndex]);
  EXPECT_NE(b, (*matrix)[kBlueToBlueIndex]);

  FastForwardAnimationTo(kDelayToCheckStateChangeMs);
  // Flash is off, color correction matrix is shown again.
  matrix = root_window->layer()->GetLayerCustomColorMatrix();
  EXPECT_EQ(r, (*matrix)[kRedToRedIndex]);
  EXPECT_EQ(g, (*matrix)[kGreenToGreenIndex]);
  EXPECT_EQ(b, (*matrix)[kBlueToBlueIndex]);
}

}  // namespace ash
