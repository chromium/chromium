// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/cast_config_controller.h"
#include "ash/public/cpp/test/test_cast_config_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/view.h"

namespace ash {
namespace {

class CastDetailedViewPixelTest : public AshTestBase {
 public:
  CastDetailedViewPixelTest() {
    feature_list_.InitWithFeatures({chromeos::features::kJelly}, {});
  }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  base::test::ScopedFeatureList feature_list_;
  TestCastConfigController cast_config_;
};

TEST_F(CastDetailedViewPixelTest, Basics) {
  // Set up the cast view with "Connect with a code", an inactive sink, and an
  // active sink so that all the possible UI elements show.
  cast_config_.set_access_code_casting_enabled(true);
  cast_config_.set_has_active_route(true);
  cast_config_.set_has_sinks_and_routes(true);
  SinkAndRoute inactive_sink;
  inactive_sink.sink.id = "id1";
  inactive_sink.sink.name = "Inctive Sink";
  cast_config_.AddSinkAndRoute(inactive_sink);
  SinkAndRoute active_sink;
  active_sink.sink.id = "id2";
  active_sink.sink.name = "Active Sink";
  active_sink.route.id = "id3";
  active_sink.route.is_local_source = true;
  cast_config_.AddSinkAndRoute(active_sink);

  // Show the cast detailed view.
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());
  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowCastDetailedView();

  // Compare pixels.
  TrayDetailedView* detailed_view =
      system_tray->bubble()
          ->quick_settings_view()
          ->GetDetailedViewForTest<TrayDetailedView>();
  ASSERT_TRUE(detailed_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/10, detailed_view));
}

}  // namespace
}  // namespace ash
