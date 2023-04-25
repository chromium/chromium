// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/cast_config_controller.h"
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
namespace {

// A CastConfigController must exist for the CastDetailedView to be created.
class TestCastConfigController : public CastConfigController {
 public:
  TestCastConfigController() = default;
  TestCastConfigController(const TestCastConfigController&) = delete;
  TestCastConfigController& operator=(const TestCastConfigController&) = delete;
  ~TestCastConfigController() override = default;

  // CastConfigController:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  bool HasMediaRouterForPrimaryProfile() const override { return false; }
  bool HasSinksAndRoutes() const override { return false; }
  bool HasActiveRoute() const override { return false; }
  bool AccessCodeCastingEnabled() const override { return false; }
  void RequestDeviceRefresh() override {}
  const std::vector<SinkAndRoute>& GetSinksAndRoutes() override {
    return sinks_and_routes_;
  }
  void CastToSink(const std::string& sink_id) override {}
  void StopCasting(const std::string& route_id) override {}
  void FreezeRoute(const std::string& route_id) override {}
  void UnfreezeRoute(const std::string& route_id) override {}

  // Exists even though it is empty because it is returned by reference.
  std::vector<SinkAndRoute> sinks_and_routes_;
};

}  // namespace

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
  views::View* detailed_view =
      system_tray->bubble()->quick_settings_view()->detailed_view();
  ASSERT_TRUE(detailed_view);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "cast_zero_state_view",
      /*revision_number=*/0, detailed_view));
}

}  // namespace ash
