// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_feature_pod_controller.h"

#include "ash/public/cpp/test/test_cast_config_controller.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace {

// Returns a SinkAndRoute as if the local machine was casting.
SinkAndRoute MakeLocalSinkAndRoute() {
  SinkAndRoute sink_and_route;
  sink_and_route.route.id = "route_id";
  sink_and_route.route.is_local_source = true;
  return sink_and_route;
}

class CastFeaturePodControllerTest : public AshTestBase {
 public:
  CastFeaturePodControllerTest() = default;

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
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_EQ(tile->label()->GetText(), u"Cast screen");
  EXPECT_EQ(tile->GetTooltipText(), u"Show cast devices");
  ASSERT_TRUE(tile->drill_in_arrow());
  EXPECT_TRUE(tile->drill_in_arrow()->GetVisible());
  ASSERT_TRUE(tile->sub_label());
  EXPECT_FALSE(tile->sub_label()->GetVisible());
}

TEST_F(CastFeaturePodControllerTest, TileNotVisibleWhenNoMediaRouter) {
  cast_config_.set_has_media_router(false);
  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_FALSE(tile->GetVisible());
}

TEST_F(CastFeaturePodControllerTest, SubLabelVisibleWhenSinksAvailable) {
  // When cast devices are available, the sub-label is visible.
  cast_config_.set_has_sinks_and_routes(true);
  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_TRUE(tile->sub_label()->GetVisible());
  EXPECT_EQ(tile->sub_label()->GetText(), u"Devices available");
}

TEST_F(CastFeaturePodControllerTest,
       SubLabelVisibleWhenAccessCodeCastingEnabled) {
  // When access code casting is available, the sub-label is visible.
  cast_config_.set_access_code_casting_enabled(true);
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
  cast_config_.set_has_sinks_and_routes(true);
  controller_->OnDevicesUpdated({});
  EXPECT_TRUE(tile->sub_label()->GetVisible());
}

TEST_F(CastFeaturePodControllerTest, TileStateWhenCastingScreen) {
  cast_config_.set_has_active_route(true);
  cast_config_.set_has_sinks_and_routes(true);
  SinkAndRoute sink_and_route = MakeLocalSinkAndRoute();
  sink_and_route.sink.name = "Sony TV";
  sink_and_route.route.content_source = ContentSource::kDesktop;
  cast_config_.AddSinkAndRoute(sink_and_route);

  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_TRUE(tile->IsToggled());
  EXPECT_EQ(tile->label()->GetText(), u"Casting screen");
  EXPECT_TRUE(tile->sub_label()->GetVisible());
  EXPECT_EQ(tile->sub_label()->GetText(), u"Sony TV");
}

TEST_F(CastFeaturePodControllerTest, CompactTileStateWhenCastingScreen) {
  cast_config_.set_has_active_route(true);
  cast_config_.set_has_sinks_and_routes(true);
  SinkAndRoute sink_and_route = MakeLocalSinkAndRoute();
  sink_and_route.sink.name = "Sony TV";
  sink_and_route.route.content_source = ContentSource::kDesktop;
  cast_config_.AddSinkAndRoute(sink_and_route);

  std::unique_ptr<FeatureTile> tile = controller_->CreateTile(/*compact=*/true);
  EXPECT_TRUE(tile->IsToggled());
  EXPECT_EQ(tile->label()->GetText(), u"Casting screen");
  EXPECT_FALSE(tile->sub_label()->GetVisible());
}

TEST_F(CastFeaturePodControllerTest, TileStateWhenCastingTab) {
  cast_config_.set_has_active_route(true);
  cast_config_.set_has_sinks_and_routes(true);
  SinkAndRoute sink_and_route = MakeLocalSinkAndRoute();
  sink_and_route.sink.name = "Sony TV";
  sink_and_route.route.content_source = ContentSource::kTab;
  cast_config_.AddSinkAndRoute(sink_and_route);

  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_TRUE(tile->IsToggled());
  EXPECT_EQ(tile->label()->GetText(), u"Casting tab");
  EXPECT_TRUE(tile->sub_label()->GetVisible());
  EXPECT_EQ(tile->sub_label()->GetText(), u"Sony TV");
}

TEST_F(CastFeaturePodControllerTest, TileStateWhenCastingUnknownSource) {
  cast_config_.set_has_active_route(true);
  cast_config_.set_has_sinks_and_routes(true);
  SinkAndRoute sink_and_route = MakeLocalSinkAndRoute();
  sink_and_route.sink.name = "Sony TV";
  sink_and_route.route.content_source = ContentSource::kUnknown;
  cast_config_.AddSinkAndRoute(sink_and_route);

  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_TRUE(tile->IsToggled());
  EXPECT_EQ(tile->label()->GetText(), u"Casting");
  EXPECT_TRUE(tile->sub_label()->GetVisible());
  EXPECT_EQ(tile->sub_label()->GetText(), u"Sony TV");
}

TEST_F(CastFeaturePodControllerTest, SubLabelHiddenWhenSinkHasNoName) {
  cast_config_.set_has_active_route(true);
  cast_config_.set_has_sinks_and_routes(true);
  SinkAndRoute sink_and_route = MakeLocalSinkAndRoute();
  sink_and_route.sink.name = "";
  cast_config_.AddSinkAndRoute(sink_and_route);

  std::unique_ptr<FeatureTile> tile = controller_->CreateTile();
  EXPECT_FALSE(tile->sub_label()->GetVisible());
}

}  // namespace
}  // namespace ash
