// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_feature_pod_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace {

class TestCastConfigController : public CastConfigController {
 public:
  TestCastConfigController() = default;
  TestCastConfigController(const TestCastConfigController&) = delete;
  TestCastConfigController& operator=(const TestCastConfigController&) = delete;
  ~TestCastConfigController() override = default;

  // CastConfigController:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  bool HasMediaRouterForPrimaryProfile() const override {
    return has_media_router_;
  }
  bool HasSinksAndRoutes() const override { return has_sinks_and_routes_; }
  bool HasActiveRoute() const override { return false; }
  bool AccessCodeCastingEnabled() const override {
    return access_code_casting_enabled_;
  }
  void RequestDeviceRefresh() override {}
  const std::vector<SinkAndRoute>& GetSinksAndRoutes() override {
    return sinks_and_routes_;
  }
  void CastToSink(const std::string& sink_id) override {}
  void StopCasting(const std::string& route_id) override {}
  void FreezeRoute(const std::string& route_id) override {}
  void UnfreezeRoute(const std::string& route_id) override {}

  bool has_media_router_ = true;
  bool has_sinks_and_routes_ = false;
  bool access_code_casting_enabled_ = false;
  std::vector<SinkAndRoute> sinks_and_routes_;
};

class CastFeaturePodControllerTest : public AshTestBase {
 public:
  CastFeaturePodControllerTest() {
    feature_list_.InitAndEnableFeature(features::kQsRevamp);
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    controller_ = std::make_unique<CastFeaturePodController>(
        GetPrimaryUnifiedSystemTray()
            ->bubble()
            ->unified_system_tray_controller());
  }

  base::test::ScopedFeatureList feature_list_;
  TestCastConfigController cast_config_;
  std::unique_ptr<CastFeaturePodController> controller_;
};

TEST_F(CastFeaturePodControllerTest, CreateTile) {
  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_TRUE(tile->GetVisible());
  EXPECT_EQ(tile->GetTooltipText(), u"Show cast devices");
  EXPECT_TRUE(tile->drill_in_button()->GetVisible());
  EXPECT_EQ(tile->drill_in_button()->GetTooltipText(), u"Show cast devices");
}

TEST_F(CastFeaturePodControllerTest, TileNotVisibleWhenNoMediaRouter) {
  cast_config_.has_media_router_ = false;
  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_FALSE(tile->GetVisible());
}

TEST_F(CastFeaturePodControllerTest, SubLabelVisibleWhenSinksAvailable) {
  // When cast devices are available, the sub-label is visible.
  cast_config_.has_sinks_and_routes_ = true;
  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_TRUE(tile->sub_label()->GetVisible());
  EXPECT_EQ(tile->sub_label()->GetText(), u"Devices available");
}

TEST_F(CastFeaturePodControllerTest,
       SubLabelVisibleWhenAccessCodeCastingEnabled) {
  // When access code casting is available, the sub-label is visible.
  cast_config_.access_code_casting_enabled_ = true;
  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_TRUE(tile->sub_label()->GetVisible());
  EXPECT_EQ(tile->sub_label()->GetText(), u"Devices available");
}

TEST_F(CastFeaturePodControllerTest, SubLabelVisibleOnDevicesUpdated) {
  // By default the sub-label is hidden.
  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_FALSE(tile->sub_label()->GetVisible());

  // If cast devices become available while the tray is open, the sub-label
  // becomes visible.
  cast_config_.has_sinks_and_routes_ = true;
  controller_->OnDevicesUpdated({});
  EXPECT_TRUE(tile->sub_label()->GetVisible());
}

}  // namespace
}  // namespace ash
