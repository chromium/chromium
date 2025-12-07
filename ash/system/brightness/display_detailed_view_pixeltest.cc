// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ui/views/view.h"

namespace ash {

// Pixel tests for the quick settings display detailed view.
class DisplayDetailedViewPixelTest
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
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    DisplayDetailedViewPixelTest,
    testing::Bool());

TEST_P(DisplayDetailedViewPixelTest, Basics) {
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowDisplayDetailedView();

  TrayDetailedView* detailed_view =
      system_tray->bubble()
          ->quick_settings_view()
          ->GetDetailedViewForTest<TrayDetailedView>();
  ASSERT_TRUE(detailed_view);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("qs_display_detailed_view"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 13 : 0,
      detailed_view));
}

}  // namespace ash
