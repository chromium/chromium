// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_cast_config_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

// Pixel tests for the quick settings cast zero-state view.
class CastZeroStateViewPixelTest : public AshTestBase {
 public:
  CastZeroStateViewPixelTest() {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, chromeos::features::kJelly}, {});
  }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  base::test::ScopedFeatureList feature_list_;
  TestCastConfigController cast_config_;
};

TEST_F(CastZeroStateViewPixelTest, Basics) {
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  // By default there are no cast devices, so showing the cast detailed view
  // will show the zero state view.
  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowCastDetailedView();
  TrayDetailedView* detailed_view =
      system_tray->bubble()->quick_settings_view()->GetDetailedViewForTest();
  ASSERT_TRUE(detailed_view);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "cast_zero_state_view",
      /*revision_number=*/7, detailed_view));
}

}  // namespace ash
