// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

// Pixel tests for the quick settings accessibility detailed view.
class AccessibilityDetailedViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AccessibilityDetailedViewPixelTest() {
    feature_list_.InitWithFeatureStates(
        {{features::kQsRevamp, IsQsRevampEnabled()},
         {chromeos::features::kJelly, IsQsRevampEnabled()}});
  }

  bool IsQsRevampEnabled() { return GetParam(); }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         AccessibilityDetailedViewPixelTest,
                         testing::Bool());

TEST_P(AccessibilityDetailedViewPixelTest, Basics) {
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowAccessibilityDetailedView();
  views::View* detailed_view_container;
  if (IsQsRevampEnabled()) {
    detailed_view_container =
        system_tray->bubble()->quick_settings_view()->detailed_view_container();
  } else {
    detailed_view_container =
        system_tray->bubble()->unified_view()->detailed_view_container();
  }
  ASSERT_TRUE(detailed_view_container);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/1, detailed_view_container));
}

}  // namespace ash
