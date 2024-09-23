// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"

namespace ash {

// Pixel tests for the quick settings accessibility detailed view.
class AccessibilityDetailedViewPixelTest : public AshTestBase {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }
};

TEST_F(AccessibilityDetailedViewPixelTest, Basics) {
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowAccessibilityDetailedView();
  views::View* detailed_view_container;
  detailed_view_container =
      system_tray->bubble()->quick_settings_view()->detailed_view_container();

  ASSERT_TRUE(detailed_view_container);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/12, detailed_view_container));
}

}  // namespace ash
