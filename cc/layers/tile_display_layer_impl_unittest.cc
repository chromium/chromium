// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_display_layer_impl.h"

#include "base/check_deref.h"
#include "cc/layers/append_quads_context.h"
#include "cc/layers/append_quads_data.h"
#include "cc/test/test_layer_tree_host_base.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"

namespace cc {

class TileDisplayLayerImplTest : public TestLayerTreeHostBase {};

TEST_F(TileDisplayLayerImplTest, NoQuadAppendedByDefault) {
  TileDisplayLayerImpl layer(CHECK_DEREF(host_impl()->active_tree()),
                             /*id=*/42);

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  layer.AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                    render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 0u);
}

TEST_F(TileDisplayLayerImplTest, SettingSolidColorResultsInSolidColorQuad) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr SkColor4f kLayerColor = SkColors::kRed;
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetSolidColor(kLayerColor);

  // For the production code to actually append a quad, the layer must have
  // non-zero size and not be completely transparent.
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 1u);
  EXPECT_EQ(render_pass->quad_list.front()->rect, kLayerRect);
  EXPECT_EQ(render_pass->quad_list.front()->visible_rect, kLayerRect);
  EXPECT_EQ(render_pass->quad_list.front()->shared_quad_state->opacity,
            kOpacity);
  EXPECT_EQ(render_pass->quad_list.front()->material,
            viz::DrawQuad::Material::kSolidColor);
  EXPECT_EQ(
      viz::SolidColorDrawQuad::MaterialCast(render_pass->quad_list.front())
          ->color,
      kLayerColor);
}

TEST_F(TileDisplayLayerImplTest,
       NonEmptyTilingWithResourceResultsInPictureQuad) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  // For the production code to actually append a quad, the layer must have
  // non-zero size and not be completely transparent.
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);

  auto resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents contents =
      TileDisplayLayerImpl::TileResource(resource_id, kLayerBounds,
                                         /*is_checkered=*/false);
  tiling.SetTileContents(TileIndex{0, 0}, contents, /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 1u);
  EXPECT_EQ(render_pass->quad_list.front()->rect, kLayerRect);
  EXPECT_EQ(render_pass->quad_list.front()->visible_rect, kLayerRect);
  EXPECT_EQ(render_pass->quad_list.front()->shared_quad_state->opacity,
            kOpacity);
  EXPECT_EQ(render_pass->quad_list.front()->resource_id, resource_id);
  EXPECT_EQ(render_pass->quad_list.front()->material,
            viz::DrawQuad::Material::kTiledContent);
  EXPECT_EQ(viz::TileDrawQuad::MaterialCast(render_pass->quad_list.front())
                ->force_anti_aliasing_off,
            false);
}

TEST_F(TileDisplayLayerImplTest,
       NonEmptyTilingWithColorResultsInSolidColorQuad) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr float kOpacity = 1.0;
  constexpr SkColor4f kTileColor = SkColors::kRed;

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  // For the production code to actually append a quad, the layer must have
  // non-zero size and not be completely transparent.
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);

  tiling.SetTileContents(TileIndex{0, 0}, kTileColor, /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 1u);
  EXPECT_EQ(render_pass->quad_list.front()->rect, kLayerRect);
  EXPECT_EQ(render_pass->quad_list.front()->visible_rect, kLayerRect);
  EXPECT_EQ(render_pass->quad_list.front()->shared_quad_state->opacity,
            kOpacity);
  EXPECT_EQ(render_pass->quad_list.front()->material,
            viz::DrawQuad::Material::kSolidColor);
  EXPECT_EQ(
      viz::SolidColorDrawQuad::MaterialCast(render_pass->quad_list.front())
          ->color,
      kTileColor);
  EXPECT_EQ(
      viz::SolidColorDrawQuad::MaterialCast(render_pass->quad_list.front())
          ->force_anti_aliasing_off,
      false);
}

class TileDisplayLayerImplWithEdgeAADisabledTest
    : public TileDisplayLayerImplTest {
 public:
  LayerTreeSettings CreateSettings() override {
    auto settings = TileDisplayLayerImplTest::CreateSettings();
    settings.enable_edge_anti_aliasing = false;
    return settings;
  }
};

TEST_F(TileDisplayLayerImplWithEdgeAADisabledTest,
       EnableEdgeAntiAliasingIsHonored) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);

  auto resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents contents =
      TileDisplayLayerImpl::TileResource(resource_id, kLayerBounds,
                                         /*is_checkered=*/false);
  tiling.SetTileContents(TileIndex{0, 0}, contents, /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 1u);
  EXPECT_EQ(viz::TileDrawQuad::MaterialCast(render_pass->quad_list.front())
                ->force_anti_aliasing_off,
            true);
}

}  // namespace cc
