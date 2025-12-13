// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Pixel tests for privacy indicators view.
class PrivacyIndicatorsTrayItemViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple</*enable_dark_mode=*/bool, /*enable_system_blur=*/bool>> {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.system_blur_enabled = IsSystemBlurEnabled();
    return init_params;
  }

  void SetUp() override {
    AshTestBase::SetUp();
    DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(
        IsDarkModeEnabled());
  }

  bool IsDarkModeEnabled() const { return std::get<0>(GetParam()); }
  bool IsSystemBlurEnabled() const { return std::get<1>(GetParam()); }

  // Simulates animating the view to its fully expanded state before shrinking
  // into a dot.
  void SimulateAnimateToFullyExpandedState(
      PrivacyIndicatorsTrayItemView* privacy_indicators_view) {
    privacy_indicators_view->expand_animation_->End();
  }

  // Simulates completing the animation.
  void SimulateAnimationEnded(
      PrivacyIndicatorsTrayItemView* privacy_indicators_view) {
    privacy_indicators_view->AnimationEnded(
        privacy_indicators_view->shorter_side_shrink_animation_.get());
  }

  std::string GenerateScreenshotName(const std::string& title) override {
    std::string prefix;
    prefix += (IsDarkModeEnabled() ? "_dark_mode" : "_light_mode");
    return pixel_test_helper()->GenerateScreenshotName(title + prefix);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrivacyIndicatorsTrayItemViewPixelTest,
    testing::Combine(/*enable_dark_mode=*/testing::Bool(),
                     /*enable_system_blur=*/testing::Bool()));

TEST_P(PrivacyIndicatorsTrayItemViewPixelTest, Basics) {
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      /*app_id=*/"app_id", /*app_name=*/u"App Name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true,
      base::MakeRefCounted<PrivacyIndicatorsNotificationDelegate>(),
      PrivacyIndicatorsSource::kApps);

  auto* notification_center_tray = GetPrimaryNotificationCenterTray();
  ASSERT_TRUE(notification_center_tray->GetVisible());

  auto* privacy_indicators_view =
      notification_center_tray->privacy_indicators_view();
  ASSERT_TRUE(privacy_indicators_view->GetVisible());

  SimulateAnimateToFullyExpandedState(privacy_indicators_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("full_view"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 3 : 0,
      notification_center_tray));

  SimulateAnimationEnded(privacy_indicators_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("dot_view"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 2 : 0,
      notification_center_tray));
}

}  // namespace ash
