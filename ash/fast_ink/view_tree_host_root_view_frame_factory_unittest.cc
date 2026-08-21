// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/view_tree_host_root_view_frame_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/frame_sink/ui_resource.h"
#include "ash/frame_sink/ui_resource_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "cc/resources/resource_pool.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr viz::SharedImageFormat kTestSharedImageFormat =
    SK_B32_SHIFT ? viz::SinglePlaneFormat::kRGBA_8888
                 : viz::SinglePlaneFormat::kBGRA_8888;
constexpr UiSourceId kTestSourceId = 1u;
constexpr gfx::Rect kTestContentRect = gfx::Rect(0, 0, 200, 100);
constexpr gfx::Rect kTestTotalDamageRect = gfx::Rect(0, 0, 50, 25);

class ViewTreeHostRootViewFrameFactoryTestBase : public AshTestBase {
 public:
  ViewTreeHostRootViewFrameFactoryTestBase() = default;

  ViewTreeHostRootViewFrameFactoryTestBase(
      const ViewTreeHostRootViewFrameFactoryTestBase&) = delete;
  ViewTreeHostRootViewFrameFactoryTestBase& operator=(
      const ViewTreeHostRootViewFrameFactoryTestBase&) = delete;

  ~ViewTreeHostRootViewFrameFactoryTestBase() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    widget_ = CreateTestWidget(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
        kShellWindowId_OverlayContainer, gfx::Rect(0, 0, 200, 100));
    factory_ =
        std::make_unique<ViewTreeHostRootViewFrameFactory>(widget_.get());
  }

  // AshTestBase:
  void TearDown() override {
    resource_manager_.ClearAvailableResources();
    resource_manager_.LostExportedResources();
    client_resource_provider_.ShutdownAndReleaseAllResources();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<ViewTreeHostRootViewFrameFactory> factory_;
  UiResourceManager resource_manager_;
  viz::ClientResourceProvider client_resource_provider_;
  std::unique_ptr<cc::ResourcePool> resource_pool_ =
      std::make_unique<cc::ResourcePool>(
          &client_resource_provider_,
          nullptr,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          cc::ResourcePool::kDefaultExpirationDelay,
          false);
  std::unique_ptr<views::Widget> widget_;
};

class ViewTreeHostRootViewFrameFactoryTest
    : public ViewTreeHostRootViewFrameFactoryTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ViewTreeHostRootViewFrameFactoryTest() {
    scoped_feature_list_.InitWithFeatureStates({
        {features::kFrameSinkHostNewBackend, GetParam()},
        {features::kViewTreeHostRootViewNewBackend, GetParam()},
    });
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ViewTreeHostRootViewFrameFactoryTest,
       CompositorFrameHasCorrectStructure) {
  UpdateDisplay("1920x1080");
  auto frame = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRect,
      kTestTotalDamageRect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  auto primary_display = display::Screen::Get()->GetPrimaryDisplay();

  // We should only have the root render pass.
  EXPECT_EQ(frame->render_pass_list.size(), 1u);

  // Frame size should be the size of content_rect in pixels.
  EXPECT_EQ(frame->size_in_pixels(), gfx::Size(200, 100));

  // We should have a single resource.
  EXPECT_EQ(frame->resource_list.size(), 1u);

  if (GetParam()) {
    EXPECT_EQ(client_resource_provider_.num_resources_for_testing(), 1u);
    EXPECT_EQ(resource_pool_->GetBusyResourceCountForTesting(), 1u);
  } else {
    EXPECT_EQ(resource_manager_.exported_resources_count(), 1u);
  }

  auto& quad_list = frame->render_pass_list.front()->quad_list;

  // We should have created a single quad.
  EXPECT_EQ(quad_list.size(), 1u);

  auto& shared_quad_state_list =
      frame->render_pass_list.front()->shared_quad_state_list;

  // We should create a single shared_quad_state.
  EXPECT_EQ(shared_quad_state_list.size(), 1u);

  EXPECT_EQ(frame->device_scale_factor(),
            primary_display.device_scale_factor());
}

TEST_P(ViewTreeHostRootViewFrameFactoryTest, HasValidSourceId) {
  UpdateDisplay("1920x1080*2");
  auto frame = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRect,
      kTestTotalDamageRect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  ASSERT_EQ(frame->resource_list.size(), 1u);
  viz::ResourceId resource_id = frame->resource_list.back().id;

  if (!GetParam()) {
    EXPECT_NE(resource_manager_.PeekExportedResource(resource_id)->ui_source_id,
              kInvalidUiSourceId);
  } else {
    EXPECT_NE(resource_id, viz::kInvalidResourceId);
  }
}

TEST_P(ViewTreeHostRootViewFrameFactoryTest, FrameDamage) {
  UpdateDisplay("1920x1080*2");
  auto frame = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRect,
      kTestTotalDamageRect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  EXPECT_EQ(frame->render_pass_list.front()->damage_rect,
            gfx::Rect(0, 0, 100, 50));

  // If total damage is more than content_rect, we crop the damage to
  // content_rect.
  frame = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRect,
      gfx::Rect(0, 0, 201, 101),
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  EXPECT_EQ(frame->render_pass_list.front()->damage_rect,
            gfx::Rect(0, 0, 400, 200));
}

TEST_P(ViewTreeHostRootViewFrameFactoryTest, CorrectQuadConfigured) {
  UpdateDisplay("1920x1080*2");
  auto frame = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRect,
      kTestTotalDamageRect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  auto& quad_list = frame->render_pass_list.front()->quad_list;

  ASSERT_EQ(quad_list.size(), 1u);

  viz::DrawQuad* quad = quad_list.back();
  EXPECT_EQ(quad->material, viz::DrawQuad::Material::kTextureContent);

  // Both should be same as content_rect in pixels.
  EXPECT_EQ(quad->rect, gfx::Rect(400, 200));
  EXPECT_EQ(quad->visible_rect, gfx::Rect(400, 200));

  auto* shared_quad_state =
      frame->render_pass_list.front()->shared_quad_state_list.front();

  EXPECT_EQ(shared_quad_state->quad_layer_rect, gfx::Rect(400, 200));
  EXPECT_EQ(shared_quad_state->visible_quad_layer_rect, gfx::Rect(400, 200));
}

TEST_P(ViewTreeHostRootViewFrameFactoryTest,
       ResourcePoolReusesReclaimedResources) {
  if (!GetParam()) {
    return;
  }

  UpdateDisplay("1920x1080");
  auto frame1 = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRect,
      kTestTotalDamageRect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  ASSERT_EQ(frame1->resource_list.size(), 1u);
  EXPECT_EQ(resource_pool_->GetTotalResourceCountForTesting(), 1u);
  EXPECT_EQ(resource_pool_->GetBusyResourceCountForTesting(), 1u);

  // Reclaim the resource.
  std::vector<viz::ReturnedResource> returned_resources;
  returned_resources.push_back(
      frame1->resource_list.back().ToReturnedResource());
  client_resource_provider_.ReceiveReturnsFromParent(
      std::move(returned_resources));

  // The resource is now available for reuse in the resource pool.
  EXPECT_EQ(resource_pool_->GetBusyResourceCountForTesting(), 0u);

  auto frame2 = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRect,
      kTestTotalDamageRect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  ASSERT_EQ(frame2->resource_list.size(), 1u);
  // Total resource count in pool should not increase because the resource was
  // reused.
  EXPECT_EQ(resource_pool_->GetTotalResourceCountForTesting(), 1u);
  EXPECT_EQ(resource_pool_->GetBusyResourceCountForTesting(), 1u);
}

struct ViewTreeHostRootViewFrameResourceTestParams {
  std::string display_spec;
  gfx::Rect content_rect;
  gfx::Size expected_resource_size;
};

class ViewTreeHostRootViewFrameResourceTest
    : public ViewTreeHostRootViewFrameFactoryTestBase,
      public ::testing::WithParamInterface<
          ViewTreeHostRootViewFrameResourceTestParams> {};

TEST_P(ViewTreeHostRootViewFrameResourceTest, CorrectResourceCreated) {
  const auto& params = GetParam();
  UpdateDisplay(params.display_spec);
  auto frame = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), params.content_rect,
      params.content_rect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  ASSERT_EQ(frame->resource_list.size(), 1u);
  auto& resource = frame->resource_list.back();
  EXPECT_EQ(resource.GetSize(), params.expected_resource_size);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ViewTreeHostRootViewFrameResourceTest,
    testing::Values(
        ViewTreeHostRootViewFrameResourceTestParams{
            .display_spec = "500x400",
            .content_rect = gfx::Rect(200, 100),
            .expected_resource_size = gfx::Size(200, 100)},
        ViewTreeHostRootViewFrameResourceTestParams{
            .display_spec = "500x400*2",
            .content_rect = gfx::Rect(200, 100),
            .expected_resource_size = gfx::Size(400, 200)},
        // Display is rotated by 90 degrees clockwise.
        ViewTreeHostRootViewFrameResourceTestParams{
            .display_spec = "500x400/r",
            .content_rect = gfx::Rect(200, 100),
            .expected_resource_size = gfx::Size(100, 200)},
        // Display is rotated by 180 degrees clockwise.
        ViewTreeHostRootViewFrameResourceTestParams{
            .display_spec = "500x400/u",
            .content_rect = gfx::Rect(200, 100),
            .expected_resource_size = gfx::Size(200, 100)},
        // Display is rotated by 270 degrees clockwise.
        ViewTreeHostRootViewFrameResourceTestParams{
            .display_spec = "500x400/l",
            .content_rect = gfx::Rect(200, 100),
            .expected_resource_size = gfx::Size(100, 200)},
        // Display is rotated by 90 degrees clockwise and has device scale
        // factor of 2.
        ViewTreeHostRootViewFrameResourceTestParams{
            .display_spec = "500x400*2/r",
            .content_rect = gfx::Rect(200, 100),
            .expected_resource_size = gfx::Size(200, 400)}));

TEST_P(ViewTreeHostRootViewFrameFactoryTest,
       OnlyCreateNewResourcesWhenNecessary) {
  if (GetParam()) {
    return;
  }

  // Populate resources in the resource manager.
  constexpr gfx::Size kResourceSizes[4] = {
      {200, 100}, {200, 100}, {250, 150}, {50, 25}};
  for (const auto& size : kResourceSizes) {
    resource_manager_.OfferResourceForTesting(
        ViewTreeHostRootViewFrameFactory::CreateUiResource(
            size, kTestSharedImageFormat, kTestSourceId,
            /*is_overlay_candidate=*/false));
  }

  EXPECT_EQ(resource_manager_.available_resources_count(), 4u);

  UpdateDisplay("1920x1080");
  auto frame = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRect,
      kTestTotalDamageRect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  // We reuse one of the matching available resources.
  EXPECT_EQ(resource_manager_.available_resources_count(), 3u);
  EXPECT_EQ(resource_manager_.exported_resources_count(), 1u);

  frame = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRect,
      kTestTotalDamageRect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  // We again reuse one of the matching available resources.
  EXPECT_EQ(resource_manager_.available_resources_count(), 2u);
  EXPECT_EQ(resource_manager_.exported_resources_count(), 2u);

  frame = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), kTestContentRect,
      kTestTotalDamageRect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  // Now the factory create a new resource since any available resource does not
  // match our requirements. The total number of resources in the manager has
  // increased by 1.
  EXPECT_EQ(resource_manager_.available_resources_count(), 2u);
  EXPECT_EQ(resource_manager_.exported_resources_count(), 3u);

  frame = factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), gfx::Rect(50, 25),
      kTestTotalDamageRect,
      /*use_overlays=*/true, resource_manager_, client_resource_provider_,
      *resource_pool_);

  // We do not create a resource since there is an available resource for the
  // new needed size.
  EXPECT_EQ(resource_manager_.available_resources_count(), 1u);
  EXPECT_EQ(resource_manager_.exported_resources_count(), 4u);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ViewTreeHostRootViewFrameFactoryTest,
                         testing::Bool());

}  // namespace
}  // namespace ash
