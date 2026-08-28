// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_frame_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/rounded_display/rounded_display_gutter.h"
#include "ash/rounded_display/rounded_display_gutter_factory.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/resources/resource_pool.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/quad_list.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/returned_resource.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ash {
namespace {

constexpr gfx::Size kTestDisplaySize(1920, 1080);
constexpr gfx::RoundedCornersF kTestPanelRadii(10);

using RoundedDisplayMasksInfo = viz::TextureDrawQuad::RoundedDisplayMasksInfo;

class RoundedDisplayFrameFactoryTestBase : public AshTestBase {
 public:
  RoundedDisplayFrameFactoryTestBase() = default;

  RoundedDisplayFrameFactoryTestBase(
      const RoundedDisplayFrameFactoryTestBase&) = delete;
  RoundedDisplayFrameFactoryTestBase& operator=(
      const RoundedDisplayFrameFactoryTestBase&) = delete;

  ~RoundedDisplayFrameFactoryTestBase() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    gutter_factory_ = std::make_unique<RoundedDisplayGutterFactory>();
    frame_factory_ = std::make_unique<RoundedDisplayFrameFactory>();

    host_window_ = std::make_unique<aura::Window>(/*delegate*/ nullptr);
    host_window_->Init(ui::LayerType::LAYER_SOLID_COLOR);

    auto* root_window = ash_test_helper()->GetHost()->window();
    root_window->AddChild(host_window_.get());
  }

  // AshTestBase:
  void TearDown() override {
    auto* root_window = ash_test_helper()->GetHost()->window();
    root_window->RemoveChild(host_window_.get());
    client_resource_provider_.ShutdownAndReleaseAllResources();
    AshTestBase::TearDown();
  }

  const std::vector<RoundedDisplayGutter*> GetGutters() {
    std::vector<RoundedDisplayGutter*> gutters;
    gutters.reserve(gutters_.size());

    for (const auto& entry : gutters_) {
      gutters.push_back(entry.get());
    }

    return gutters;
  }

  // Creates vertical gutters and appends them to `gutters_`.
  void AppendVerticalOverlayGutters(const gfx::Size& display_size_in_pixels,
                                    const gfx::RoundedCornersF& panel_radii) {
    auto overlay_gutters = gutter_factory_->CreateOverlayGutters(
        display_size_in_pixels, panel_radii,
        /*create_vertical_gutters=*/true);

    for (auto& gutter : overlay_gutters) {
      gutters_.push_back(std::move(gutter));
    }
  }

  // Creates horizontal gutters and appends them to `gutters_`.
  void AppendHorizontalOverlayGutters(const gfx::Size& display_size_in_pixels,
                                      const gfx::RoundedCornersF& panel_radii) {
    auto overlay_gutters = gutter_factory_->CreateOverlayGutters(
        display_size_in_pixels, panel_radii,
        /*create_vertical_gutters=*/false);

    for (auto& gutter : overlay_gutters) {
      gutters_.push_back(std::move(gutter));
    }
  }

 protected:
  std::unique_ptr<RoundedDisplayGutterFactory> gutter_factory_;
  std::unique_ptr<RoundedDisplayFrameFactory> frame_factory_;
  std::vector<std::unique_ptr<RoundedDisplayGutter>> gutters_;
  viz::ClientResourceProvider client_resource_provider_;
  std::unique_ptr<cc::ResourcePool> resource_pool_ =
      std::make_unique<cc::ResourcePool>(
          &client_resource_provider_,
          nullptr,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          cc::ResourcePool::kDefaultExpirationDelay,
          false);
  std::unique_ptr<aura::Window> host_window_;
};

using RoundedDisplayFrameFactoryTest = RoundedDisplayFrameFactoryTestBase;

// TODO(zoraiznaeem): Add more unittest coverage.
TEST_F(RoundedDisplayFrameFactoryTest, CompositorFrameHasCorrectStructure) {
  AppendVerticalOverlayGutters(kTestDisplaySize, kTestPanelRadii);

  const auto& gutters = GetGutters();

  auto frame = frame_factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), *host_window_,
      client_resource_provider_, *resource_pool_, gutters);

  // We should only have the root render pass.
  EXPECT_EQ(frame->render_pass_list.size(), 1u);

  EXPECT_EQ(frame->size_in_pixels(), GetPrimaryDisplay().GetSizeInPixel());

  // We should have a resource for each gutter.
  EXPECT_EQ(frame->resource_list.size(), gutters.size());

  EXPECT_EQ(client_resource_provider_.num_resources_for_testing(),
            gutters.size());
  EXPECT_EQ(resource_pool_->GetBusyResourceCountForTesting(), gutters.size());

  auto& quad_list = frame->render_pass_list.front()->quad_list;

  // We should have created a draw quad for each gutter.
  EXPECT_EQ(quad_list.size(), gutters.size());

  auto& shared_quad_state_list =
      frame->render_pass_list.front()->shared_quad_state_list;

  // We should create a shared_quad_state for each draw quad.
  EXPECT_EQ(shared_quad_state_list.size(), gutters.size());
}

MATCHER_P(IsRoundedDisplayMasksInfoEqual, value, "") {
  return arg.is_horizontally_positioned == value.is_horizontally_positioned &&
         arg.radii[RoundedDisplayMasksInfo::kOriginRoundedDisplayMaskIndex] ==
             value.radii
                 [RoundedDisplayMasksInfo::kOriginRoundedDisplayMaskIndex] &&
         arg.radii[RoundedDisplayMasksInfo::kOtherRoundedDisplayMaskIndex] ==
             value
                 .radii[RoundedDisplayMasksInfo::kOtherRoundedDisplayMaskIndex];
}

TEST_F(RoundedDisplayFrameFactoryTest,
       CorrectRoundedDisplayInfo_VerticalGuttersWithTwoCorners) {
  const auto panel_radii = gfx::RoundedCornersF(10, 0, 0, 15);
  AppendVerticalOverlayGutters(kTestDisplaySize, panel_radii);

  // `gutter_factory_` will only create left overlay gutter.
  EXPECT_EQ(gutters_.size(), 1u);

  auto frame = frame_factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), *host_window_,
      client_resource_provider_, *resource_pool_, GetGutters());

  const viz::QuadList& quad_list = frame->render_pass_list.front()->quad_list;
  ASSERT_EQ(quad_list.size(), 1u);

  EXPECT_THAT(viz::TextureDrawQuad::MaterialCast(quad_list.ElementAt(0))
                  ->rounded_display_masks_info,
              IsRoundedDisplayMasksInfoEqual(
                  RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                      /*origin_rounded_display_mask_radius=*/10,
                      /*other_rounded_display_mask_radius=*/15,
                      /*is_horizontally_positioned=*/false)));
}

TEST_F(RoundedDisplayFrameFactoryTest,
       CorrectRoundedDisplayInfo_HorizontalGuttersWithTwoCorners) {
  const auto panel_radii = gfx::RoundedCornersF(15, 10, 0, 0);
  AppendHorizontalOverlayGutters(kTestDisplaySize, panel_radii);

  // `gutter_factory_` will only create upper overlay gutter.
  EXPECT_EQ(gutters_.size(), 1u);

  auto frame = frame_factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), *host_window_,
      client_resource_provider_, *resource_pool_, GetGutters());

  const viz::QuadList& quad_list = frame->render_pass_list.front()->quad_list;
  ASSERT_EQ(quad_list.size(), 1u);

  EXPECT_THAT(viz::TextureDrawQuad::MaterialCast(quad_list.ElementAt(0))
                  ->rounded_display_masks_info,
              IsRoundedDisplayMasksInfoEqual(
                  RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                      /*origin_rounded_display_mask_radius=*/15,
                      /*other_rounded_display_mask_radius=*/10,
                      /*is_horizontally_positioned=*/true)));
}

TEST_F(RoundedDisplayFrameFactoryTest,
       CorrectRoundedDisplayInfo_GuttersWithOneCorner) {
  const auto panel_radii = gfx::RoundedCornersF(10, 0, 0, 0);
  AppendHorizontalOverlayGutters(kTestDisplaySize, panel_radii);

  // `gutter_factory_` will only create upper overlay gutter.
  EXPECT_EQ(gutters_.size(), 1u);

  auto frame = frame_factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), *host_window_,
      client_resource_provider_, *resource_pool_, GetGutters());

  const viz::QuadList& quad_list = frame->render_pass_list.front()->quad_list;
  ASSERT_EQ(quad_list.size(), 1u);

  EXPECT_THAT(viz::TextureDrawQuad::MaterialCast(quad_list.ElementAt(0))
                  ->rounded_display_masks_info,
              IsRoundedDisplayMasksInfoEqual(
                  RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                      /*origin_rounded_display_mask_radius=*/10,
                      /*other_rounded_display_mask_radius=*/0,
                      /*is_horizontally_positioned=*/true)));
}

TEST_F(RoundedDisplayFrameFactoryTest, ResourcePoolReusesReclaimedResources) {
  AppendVerticalOverlayGutters(kTestDisplaySize, kTestPanelRadii);
  const auto& gutters = GetGutters();

  auto frame1 = frame_factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), *host_window_,
      client_resource_provider_, *resource_pool_, gutters);

  ASSERT_EQ(frame1->resource_list.size(), gutters.size());
  EXPECT_EQ(resource_pool_->GetTotalResourceCountForTesting(), gutters.size());
  EXPECT_EQ(resource_pool_->GetBusyResourceCountForTesting(), gutters.size());

  // Reclaim the resources.
  std::vector<viz::ReturnedResource> returned_resources;
  for (const auto& resource : frame1->resource_list) {
    returned_resources.push_back(resource.ToReturnedResource());
  }
  client_resource_provider_.ReceiveReturnsFromParent(
      std::move(returned_resources));

  // The resources are now available for reuse in the resource pool.
  EXPECT_EQ(resource_pool_->GetBusyResourceCountForTesting(), 0u);

  auto frame2 = frame_factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), *host_window_,
      client_resource_provider_, *resource_pool_, gutters);

  ASSERT_EQ(frame2->resource_list.size(), gutters.size());
  // Total resource count in pool should not increase because resources were
  // reused.
  EXPECT_EQ(resource_pool_->GetTotalResourceCountForTesting(), gutters.size());
  EXPECT_EQ(resource_pool_->GetBusyResourceCountForTesting(), gutters.size());
}

}  // namespace
}  // namespace ash
