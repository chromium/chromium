// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_cast_config_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"

namespace ash {

// Pixel tests for the quick settings cast zero-state view.
class CastZeroStateViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface</*enable_system_blur=*/bool> {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.system_blur_enabled = GetParam();
    return init_params;
  }

  TestCastConfigController cast_config_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CastZeroStateViewPixelTest,
    testing::Bool());

TEST_P(CastZeroStateViewPixelTest, Basics) {
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  // By default there are no cast devices, so showing the cast detailed view
  // will show the zero state view.
  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowCastDetailedView();
  TrayDetailedView* detailed_view =
      system_tray->bubble()
          ->quick_settings_view()
          ->GetDetailedViewForTest<TrayDetailedView>();
  ASSERT_TRUE(detailed_view);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("cast_zero_state_view"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 14 : 0,
      detailed_view));
}

}  // namespace ash
