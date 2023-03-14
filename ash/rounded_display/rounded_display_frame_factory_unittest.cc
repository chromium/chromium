// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_frame_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/frame_sink/ui_resource_manager.h"
#include "ash/rounded_display/rounded_display_gutter.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "rounded_display_gutter_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ash {
namespace {

constexpr viz::ResourceFormat kTestResourceFormat =
    SK_B32_SHIFT ? viz::RGBA_8888 : viz::BGRA_8888;
constexpr gfx::Size kTestDisplaySize(1920, 1080);

class RoundedDisplayFrameFactoryTest : public AshTestBase {
 public:
  RoundedDisplayFrameFactoryTest() = default;

  RoundedDisplayFrameFactoryTest(const RoundedDisplayFrameFactoryTest&) =
      delete;
  RoundedDisplayFrameFactoryTest& operator=(
      const RoundedDisplayFrameFactoryTest&) = delete;

  ~RoundedDisplayFrameFactoryTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    gutter_factory_ = std::make_unique<RoundedDisplayGutterFactory>();
    frame_factory_ = std::make_unique<RoundedDisplayFrameFactory>();

    host_window_ = std::make_unique<aura::Window>(/*delegate*/ nullptr);
    host_window_->Init(ui::LayerType::LAYER_SOLID_COLOR);

    auto* root_window = ash_test_helper()->GetHost()->window();
    root_window->AddChild(host_window_.get());

    gutters_ = CreateGutters(kTestDisplaySize, gfx::RoundedCornersF(10),
                             /*create_vertical_gutters=*/true);
  }

  // AshTestBase:
  void TearDown() override {
    auto* root_window = ash_test_helper()->GetHost()->window();
    root_window->RemoveChild(host_window_.get());
    resource_manager_.LostExportedResources();
    resource_manager_.ClearAvailableResources();
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

  std::vector<std::unique_ptr<RoundedDisplayGutter>> CreateGutters(
      const gfx::Size& display_size_in_pixels,
      const gfx::RoundedCornersF& display_radii,
      bool create_vertical_gutters) {
    std::vector<std::unique_ptr<RoundedDisplayGutter>> gutters;

    auto overlay_gutters = gutter_factory_->CreateOverlayGutters(
        display_size_in_pixels, display_radii, create_vertical_gutters);

    for (auto& gutter : overlay_gutters) {
      gutters.push_back(std::move(gutter));
    }

    auto non_overlay_gutters = gutter_factory_->CreateNonOverlayGutters(
        display_size_in_pixels, display_radii);

    for (auto& gutter : non_overlay_gutters) {
      gutters.push_back(std::move(gutter));
    }

    return gutters;
  }

 protected:
  std::unique_ptr<RoundedDisplayGutterFactory> gutter_factory_;
  std::unique_ptr<RoundedDisplayFrameFactory> frame_factory_;
  std::vector<std::unique_ptr<RoundedDisplayGutter>> gutters_;
  UiResourceManager resource_manager_;
  std::unique_ptr<aura::Window> host_window_;
};

// TODO(zoraiznaeem): Add more unittest coverage.
TEST_F(RoundedDisplayFrameFactoryTest, CompositorFrameHasCorrectStructure) {
  const auto& gutters = GetGutters();

  auto frame = frame_factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), *host_window_,
      resource_manager_, gutters);

  // We should only have the root render pass.
  EXPECT_EQ(frame->render_pass_list.size(), 1u);

  EXPECT_EQ(frame->size_in_pixels(), GetPrimaryDisplay().GetSizeInPixel());

  // We should have a resource for each gutter.
  EXPECT_EQ(frame->resource_list.size(), gutters.size());
  EXPECT_EQ(resource_manager_.exported_resources_count(), gutters.size());

  auto& quad_list = frame->render_pass_list.front()->quad_list;

  // We should have created a draw quad for each gutter.
  EXPECT_EQ(quad_list.size(), gutters.size());

  auto& shared_quad_state_list =
      frame->render_pass_list.front()->shared_quad_state_list;

  // We should create a shared_quad_state for each draw quad.
  EXPECT_EQ(shared_quad_state_list.size(), gutters.size());
}

TEST_F(RoundedDisplayFrameFactoryTest, OnlyCreateNewResourcesWhenNecessary) {
  const auto& gutters = GetGutters();

  // Populate resources in the resource manager.
  for (const auto* gutter : gutters) {
    resource_manager_.OfferResource(
        RoundedDisplayFrameFactory::CreateUiResource(gutter->bounds().size(),
                                                     kTestResourceFormat,
                                                     gutter->ui_source_id(),
                                                     /*is_overlay=*/false));
  }

  EXPECT_EQ(resource_manager_.available_resources_count(), 6u);

  frame_factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), *host_window_,
      resource_manager_, gutters);

  // Should have reused all the resources.
  EXPECT_EQ(resource_manager_.available_resources_count(), 0u);
  // Should have exported six resources as we have six gutters.
  EXPECT_EQ(resource_manager_.exported_resources_count(), 6u);

  resource_manager_.LostExportedResources();

  // Adding more resources.
  for (int index : {0, 0, 4, 5}) {
    const auto* gutter = gutters.at(index);
    resource_manager_.OfferResource(
        RoundedDisplayFrameFactory::CreateUiResource(gutter->bounds().size(),
                                                     kTestResourceFormat,
                                                     gutter->ui_source_id(),
                                                     /*is_overlay=*/false));
  }

  EXPECT_EQ(resource_manager_.available_resources_count(), 4u);

  frame_factory_->CreateCompositorFrame(
      viz::BeginFrameAck::CreateManualAckWithDamage(), *host_window_,
      resource_manager_, gutters);

  // We end up using the available resources and are left with the extra
  // resource that was available. We also must have created resources for
  // gutters for which we did not have any available resources.
  EXPECT_EQ(resource_manager_.available_resources_count(), 1u);

  // Should have exported six resources as we have six gutters.
  EXPECT_EQ(resource_manager_.exported_resources_count(), 6u);
}

}  // namespace
}  // namespace ash
