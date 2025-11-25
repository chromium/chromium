// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_display_layer_impl.h"

#include <algorithm>

#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_context.h"
#include "cc/layers/append_quads_data.h"
#include "cc/test/test_layer_tree_host_base.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"

using testing::ElementsAre;

namespace cc {

class TileDisplayLayerImplTest : public TestLayerTreeHostBase {};

TEST_F(TileDisplayLayerImplTest, NoQuadAppendedByDefault) {
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));
  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 0u);
}

TEST_F(TileDisplayLayerImplTest,
       AppendQuadsDoesNotAppendQuadsForOccludedTiles) {
  constexpr gfx::Size kLayerBounds(100, 100);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  // Create a tiling with one tile.
  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);

  auto resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents contents =
      TileDisplayLayerTileResource(resource_id, kLayerBounds);
  tiling.SetTileContents(TileIndex{0, 0}, contents, /*update_damage=*/false);

  // Set up occlusion that covers the entire layer. Occlusion is specified in
  // screen space, so we provide an identity transform to make content space
  // the same as screen space.
  gfx::Transform identity_transform;
  SimpleEnclosedRegion screen_occlusion(kLayerRect);
  raw_layer->draw_properties().occlusion_in_content_space =
      Occlusion(identity_transform, screen_occlusion, screen_occlusion);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 0u);
}

TEST_F(TileDisplayLayerImplTest,
       AppendQuadsAppendsClippedQuadsForPartiallyOccludedTiles) {
  const gfx::Rect layer_rect(0, 0, 10, 10);
  const gfx::Rect tile_rect(0, 0, 10, 10);
  const gfx::Rect occluded_rect(0, 0, 5, 10);

  // Setup layer and tiling.
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(layer_rect.size());
  raw_layer->SetRecordedBounds(layer_rect);
  raw_layer->draw_properties().visible_layer_rect = layer_rect;
  raw_layer->draw_properties().occlusion_in_content_space =
      Occlusion(gfx::Transform(), SimpleEnclosedRegion(occluded_rect),
                SimpleEnclosedRegion());

  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(tile_rect.size());
  tiling.SetTilingRect(tile_rect);

  auto resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents contents =
      TileDisplayLayerTileResource(resource_id, tile_rect.size());
  tiling.SetTileContents(TileIndex{0, 0}, contents, /*update_damage=*/false);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  // Append quads.
  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // Verify that one quad is appended and it's clipped.
  ASSERT_EQ(1u, render_pass->quad_list.size());
  const viz::DrawQuad* quad = render_pass->quad_list.front();
  EXPECT_EQ(viz::DrawQuad::Material::kTiledContent, quad->material);

  const auto* tile_quad = viz::TileDrawQuad::MaterialCast(quad);
  EXPECT_EQ(tile_rect, tile_quad->rect);
  const gfx::Rect expected_visible_rect =
      gfx::SubtractRects(tile_rect, occluded_rect);
  EXPECT_EQ(expected_visible_rect, tile_quad->visible_rect);
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
  raw_layer->SetRecordedBounds(kLayerRect);
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
      TileDisplayLayerTileResource(resource_id, kLayerBounds);
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
  raw_layer->SetRecordedBounds(kLayerRect);
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

// Verifies that GetContentsResourceId() handles the error case of being called
// when the layer has no tiles, setting the resource ID to invalid in that case.
TEST_F(TileDisplayLayerImplTest, GetContentsResourceIdHandlesLackOfTiles) {
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  layer->SetIsBackdropFilterMask(true);

  viz::ResourceId mask_resource_id = viz::ResourceId(42);
  gfx::Size mask_texture_size;
  gfx::SizeF mask_uv_size;
  layer->GetContentsResourceId(&mask_resource_id, &mask_texture_size,
                               &mask_uv_size);
  EXPECT_EQ(mask_resource_id, viz::kInvalidResourceId);
}

// Verifies that GetContentsResourceId() returns the correct resource ID for a
// backdrop filter mask.
TEST_F(TileDisplayLayerImplTest,
       GetContentsResourceIdReturnsResourceForBackdropFilter) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetIsBackdropFilterMask(true);

  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);

  auto resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents contents =
      TileDisplayLayerTileResource(resource_id, kLayerBounds);
  tiling.SetTileContents(TileIndex{0, 0}, contents, /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  viz::ResourceId mask_resource_id;
  gfx::Size mask_texture_size;
  gfx::SizeF mask_uv_size;
  raw_layer->GetContentsResourceId(&mask_resource_id, &mask_texture_size,
                                   &mask_uv_size);

  EXPECT_EQ(mask_resource_id, resource_id);
  EXPECT_EQ(mask_texture_size, kLayerBounds);

  // `mask_uv_size` is the ratio between the tile's width/height and that of
  // the resource. Here, the tile and resource have been created with the same
  // size.
  EXPECT_EQ(mask_uv_size, gfx::SizeF(1.0f, 1.0f));
}

// Verifies that GetContentsResourceId() returns the correct mask UV size when
// the tile and resource sizes differ.
TEST_F(TileDisplayLayerImplTest,
       GetContentsResourceIdComputesUVMaskSizeCorrectlyForBackdropFilter) {
  constexpr gfx::Size kLayerBounds(100, 200);
  constexpr gfx::Size kResourceSize(200, 400);
  constexpr gfx::Rect kLayerRect(kLayerBounds);

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetIsBackdropFilterMask(true);

  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);

  auto resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents contents =
      TileDisplayLayerTileResource(resource_id, kResourceSize);
  tiling.SetTileContents(TileIndex{0, 0}, contents, /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  viz::ResourceId mask_resource_id;
  gfx::Size mask_texture_size;
  gfx::SizeF mask_uv_size;
  raw_layer->GetContentsResourceId(&mask_resource_id, &mask_texture_size,
                                   &mask_uv_size);

  EXPECT_EQ(mask_resource_id, resource_id);
  EXPECT_EQ(mask_texture_size, kResourceSize);

  // `mask_uv_size` is the ratio between the tile's width/height and that of
  // the resource. Here, the tile has been created to be half the size of the
  // resource in each dimension.
  EXPECT_EQ(mask_uv_size, gfx::SizeF(0.5f, 0.5f));
}

// Tests that GetContentsResourceId() returns viz::kInvalidResourceId if the
// layer has more than one tiling, as masks are only supported if they fit on a
// single tile.
TEST_F(TileDisplayLayerImplTest,
       GetContentsResourceIdReturnsInvalidIdForMultipleTilings) {
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetIsBackdropFilterMask(true);

  // Create two tilings.
  raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  raw_layer->GetOrCreateTilingFromScaleKey(2.0);

  viz::ResourceId mask_resource_id;
  gfx::Size mask_texture_size;
  gfx::SizeF mask_uv_size;
  raw_layer->GetContentsResourceId(&mask_resource_id, &mask_texture_size,
                                   &mask_uv_size);

  EXPECT_EQ(mask_resource_id, viz::kInvalidResourceId);
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
       EnableEdgeAntiAliasingIsHonoredForPictureQuads) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
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
      TileDisplayLayerTileResource(resource_id, kLayerBounds);
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

TEST_F(TileDisplayLayerImplWithEdgeAADisabledTest,
       EnableEdgeAntiAliasingIsHonoredForSolidColorQuads) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr float kOpacity = 1.0;
  constexpr SkColor4f kTileColor = SkColors::kRed;

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
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
  EXPECT_EQ(
      viz::SolidColorDrawQuad::MaterialCast(render_pass->quad_list.front())
          ->force_anti_aliasing_off,
      true);
}

TEST_F(TileDisplayLayerImplTest, MissingTileResultsInCheckerBoardQuad) {
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
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  // Add a tiling, but don't give it any tile contents.
  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // Verify that the layer appended a checkerboard quad for the missing tile.
  // Checkerboard quads are solid-color quads whose color is the safe background
  // opaque color.
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
      raw_layer->safe_opaque_background_color());
}

TEST_F(TileDisplayLayerImplTest, OomTileResultsInSolidColorQuad) {
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
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  // Add a tiling with an OOM tile.
  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);
  tiling.SetTileContents(
      TileIndex{0, 0},
      TileDisplayLayerNoContents{mojom::MissingTileReason::kOutOfMemory},
      /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // Verify that the layer appended a solid color quad for the OOM tile.
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
      raw_layer->safe_opaque_background_color());
}

// Verifies that the layer appends quads from the highest-resolution tiling
// when multiple tilings are available.
TEST_F(TileDisplayLayerImplTest, AppendsQuadsFromHighestResolutionTilingByDefault) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  // Create two tilings with different scales.
  auto& low_res_tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  low_res_tiling.SetTileSize(kLayerBounds);
  low_res_tiling.SetTilingRect(kLayerRect);
  auto& high_res_tiling = raw_layer->GetOrCreateTilingFromScaleKey(2.0);
  high_res_tiling.SetTileSize(kLayerBounds);
  high_res_tiling.SetTilingRect(kLayerRect);

  // Set content for the high-res tiling only.
  auto resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents contents =
      TileDisplayLayerTileResource(resource_id, kLayerBounds);
  high_res_tiling.SetTileContents(TileIndex{0, 0}, contents,
                                  /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // Verify that the quad is from the high-res tiling.
  EXPECT_EQ(render_pass->quad_list.size(), 1u);
  EXPECT_EQ(render_pass->quad_list.front()->resource_id, resource_id);
}

// Verifies that the layer can be forced to append quads from a
// lower-resolution tiling if the ideal contents scale matches that tiling.
TEST_F(TileDisplayLayerImplTest, AppendsQuadsFromIdealResolutionTiling) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  // Create two tilings with different scales.
  auto& low_res_tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  low_res_tiling.SetTileSize(kLayerBounds);
  low_res_tiling.SetTilingRect(kLayerRect);
  auto& high_res_tiling = raw_layer->GetOrCreateTilingFromScaleKey(2.0);
  high_res_tiling.SetTileSize(kLayerBounds);
  high_res_tiling.SetTilingRect(kLayerRect);

  // Set content for the low-resolution tiling only.
  auto low_res_resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents low_res_contents =
      TileDisplayLayerTileResource(low_res_resource_id, kLayerBounds);
  low_res_tiling.SetTileContents(TileIndex{0, 0}, low_res_contents,
                                 /*update_damage=*/true);

  // With an identity transform, the ideal contents scale is 1.0, so the
  // low-resolution tiling should be chosen.
  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // Verify that the quad is from the low-res tiling.
  EXPECT_EQ(render_pass->quad_list.size(), 1u);
  EXPECT_EQ(render_pass->quad_list.front()->resource_id, low_res_resource_id);
}

// Verifies that RemoveTiling correctly removes a tiling.
TEST_F(TileDisplayLayerImplTest, RemoveTilingRemovesTiling) {
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  // Add a tiling.
  raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  ASSERT_NE(raw_layer->GetTilingForTesting(1.0), nullptr);

  // Remove the tiling.
  raw_layer->RemoveTiling(1.0);
  EXPECT_EQ(raw_layer->GetTilingForTesting(1.0), nullptr);
}

// Verifies that removing one of multiple tilings leaves the others intact.
TEST_F(TileDisplayLayerImplTest, RemoveOneOfMultipleTilings) {
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  // Add two tilings.
  raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  raw_layer->GetOrCreateTilingFromScaleKey(2.0);
  ASSERT_NE(raw_layer->GetTilingForTesting(1.0), nullptr);
  ASSERT_NE(raw_layer->GetTilingForTesting(2.0), nullptr);

  // Remove one tiling and verify that that tiling and only that tiling was
  // removed.
  raw_layer->RemoveTiling(1.0);
  EXPECT_EQ(raw_layer->GetTilingForTesting(1.0), nullptr);
  EXPECT_NE(raw_layer->GetTilingForTesting(2.0), nullptr);
}

// Verifies that calling RemoveTiling() for a tiling that doesn't exist doesn't
// crash.
TEST_F(TileDisplayLayerImplTest, RemoveTilingOnNonExistentTilingDoesNotCrash) {
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  ASSERT_EQ(raw_layer->GetTilingForTesting(1.0), nullptr);

  // This should not crash.
  raw_layer->RemoveTiling(1.0);
  EXPECT_EQ(raw_layer->GetTilingForTesting(1.0), nullptr);
}

// Verifies that setting tile contents with `update_damage=true` records the
// correct damage rect on the layer.
TEST_F(TileDisplayLayerImplTest,
       SetTileContentsRecordsDamageWhenUpdateDamageIsTrue) {
  // Configure the layer to have 5 tiles to be able to test damage from
  // individual tile updates.
  constexpr gfx::Size kLayerBounds(100, 100);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr gfx::Size kTileSize(20, 20);
  const TileIndex kTileIndex1{1, 2};
  const TileIndex kTileIndex2{3, 0};

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);

  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kTileSize);
  tiling.SetTilingRect(kLayerRect);

  // When SetTileContents is called with update_damage=true, it calculates the
  // area that needs to be redrawn (the damage). This calculation happens in the
  // tile's coordinate system first. However, the final damage must be recorded
  // on the layer in the layer's coordinate system. TileDisplayLayerImpl uses
  // the inverse of the raster transform to map the tile's damage rectangle
  // back into the layer's coordinate space. Explicitly initialize the raster
  // transform to be the identity transform (it is not explicitly initialized by
  // default).
  tiling.SetRasterTransform(gfx::AxisTransform2d());

  // Set content for a tile and check that the damage rect is updated.
  tiling.SetTileContents(kTileIndex1, SkColors::kRed,
                         /*update_damage=*/true);
  EXPECT_EQ(
      raw_layer->GetDamageRect(),
      tiling.tiling_data()->TileBoundsWithBorder(kTileIndex1.i, kTileIndex1.j));

  // Set content for another tile and check that the damage rect is expanded.
  tiling.SetTileContents(kTileIndex2, SkColors::kBlue,
                         /*update_damage=*/true);
  gfx::Rect expected_damage_rect;
  expected_damage_rect.Union(
      tiling.tiling_data()->TileBoundsWithBorder(kTileIndex1.i, kTileIndex1.j));
  expected_damage_rect.Union(
      tiling.tiling_data()->TileBoundsWithBorder(kTileIndex2.i, kTileIndex2.j));
  EXPECT_EQ(raw_layer->GetDamageRect(), expected_damage_rect);

  // Reset change tracking and check that the damage rect is cleared.
  raw_layer->ResetChangeTracking();
  EXPECT_TRUE(raw_layer->GetDamageRect().IsEmpty());
}

// Verifies that setting tile contents with `update_damage=false` does not
// record damage on the layer.
TEST_F(TileDisplayLayerImplTest,
       SetTileContentsDoesntRecordDamageWhenUpdateDamageIsFalse) {
  // Configure the layer to have 5 tiles to be able to test damage from
  // individual tile updates.
  constexpr gfx::Size kLayerBounds(100, 100);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr gfx::Size kTileSize(20, 20);
  const TileIndex kTileIndex{1, 2};

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);

  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kTileSize);
  tiling.SetTilingRect(kLayerRect);

  tiling.SetTileContents(kTileIndex, SkColors::kRed,
                         /*update_damage=*/false);
  EXPECT_TRUE(raw_layer->GetDamageRect().IsEmpty());
}

// Verifies that when Tiling::SetTileContents is called with NoContents and the
// reason is MissingTileReason::kTileDeleted, the corresponding tile is removed
// from the tiling.
TEST_F(TileDisplayLayerImplTest,
       SetTileContentsWithNoContentsAndTileDeletedReasonRemovesTile) {
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  const TileIndex kTileIndex{0, 0};

  // Add a tile.
  tiling.SetTileContents(kTileIndex, SkColors::kRed,
                         /*update_damage=*/false);
  ASSERT_NE(tiling.TileAt(kTileIndex), nullptr);

  // Set the tile's contents to NoContents with kTileDeleted as the reason and
  // verify that the tile is deleted.
  tiling.SetTileContents(
      kTileIndex,
      TileDisplayLayerNoContents{mojom::MissingTileReason::kTileDeleted},
      /*update_damage=*/false);
  EXPECT_EQ(tiling.TileAt(kTileIndex), nullptr);
}

// Verifies that when Tiling::SetTileContents is called with NoContents and a
// reason other than MissingTileReason::kTileDeleted, the tile's contents are
// updated to NoContents.
TEST_F(TileDisplayLayerImplTest,
       SetTileContentsWithNoContentsAndOtherReasonUpdatesTile) {
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  const TileIndex kTileIndex{0, 0};

  // Add a tile.
  tiling.SetTileContents(kTileIndex, SkColors::kRed,
                         /*update_damage=*/false);
  ASSERT_NE(tiling.TileAt(kTileIndex), nullptr);

  // Set the tile's contents to NoContents with a reason other than
  // kTileDeleted.
  tiling.SetTileContents(
      kTileIndex,
      TileDisplayLayerNoContents{mojom::MissingTileReason::kResourceNotReady},
      /*update_damage=*/false);

  // Verify that the tile still exists and its contents are NoContents.
  auto* tile = tiling.TileAt(kTileIndex);
  EXPECT_NE(tile, nullptr);
  EXPECT_TRUE(
      std::holds_alternative<TileDisplayLayerNoContents>(tile->contents()));
}

// Verifies that last_append_quads_scales_ is correctly updated after
// AppendQuads.
TEST_F(TileDisplayLayerImplTest, LastAppendQuadsScalesUpdated) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = 1.0;

  // Create two tilings with different scales.
  auto& low_res_tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  low_res_tiling.SetTileSize(kLayerBounds);
  low_res_tiling.SetTilingRect(kLayerRect);
  auto& high_res_tiling = raw_layer->GetOrCreateTilingFromScaleKey(2.0);
  high_res_tiling.SetTileSize(kLayerBounds);
  high_res_tiling.SetTilingRect(kLayerRect);

  // Set content for both tilings.
  auto low_res_resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents low_res_contents =
      TileDisplayLayerImpl::TileResource(low_res_resource_id, kLayerBounds);
  low_res_tiling.SetTileContents(TileIndex{0, 0}, low_res_contents,
                                 /*update_damage=*/true);

  auto high_res_resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents high_res_contents =
      TileDisplayLayerTileResource(high_res_resource_id, kLayerBounds);
  high_res_tiling.SetTileContents(TileIndex{0, 0}, high_res_contents,
                                  /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // AppendQuads should use the ideal resolution tiling (1.0), so
  // last_append_quads_scales_ should contain 1.0.
  const auto& last_append_scales = raw_layer->LastAppendQuadsScalesForTesting();
  ASSERT_EQ(last_append_scales.size(), 1u);
  EXPECT_EQ(last_append_scales[0], 1.0f);
}

// Verifies that GetSafeToDeleteTilings returns the correct set of scales for
// tilings that are safe to delete.
TEST_F(TileDisplayLayerImplTest, GetSafeToDeleteTilingsBasic) {
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  // Case 1: Basic scenario - some tilings used, some not.
  raw_layer->LastAppendQuadsScalesForTesting() = {2.0f};
  raw_layer->SetProposedTilingScalesForDeletion({1.0f, 2.0f, 3.0f});

  std::vector<float> safe_to_delete = raw_layer->GetSafeToDeleteTilings();
  EXPECT_THAT(safe_to_delete, ElementsAre(1.0f, 3.0f));

  // Case 2: No tilings were used in the last frame.
  raw_layer->LastAppendQuadsScalesForTesting().clear();
  raw_layer->SetProposedTilingScalesForDeletion({1.0f, 2.0f, 3.0f});

  safe_to_delete = raw_layer->GetSafeToDeleteTilings();
  EXPECT_THAT(safe_to_delete, ElementsAre(1.0f, 2.0f, 3.0f));

  // Case 3: All proposed tilings were used in the last frame.
  raw_layer->LastAppendQuadsScalesForTesting() = {1.0f, 2.0f, 3.0f};
  raw_layer->SetProposedTilingScalesForDeletion({1.0f, 2.0f, 3.0f});

  safe_to_delete = raw_layer->GetSafeToDeleteTilings();
  EXPECT_TRUE(safe_to_delete.empty());

  // Case 4: Proposed tilings include some that don't exist in
  // last_append_quads_scales_.
  raw_layer->LastAppendQuadsScalesForTesting() = {2.0f};

  // 4.0 is not in last_append_quads_scales_
  raw_layer->SetProposedTilingScalesForDeletion({1.0f, 4.0f});

  safe_to_delete = raw_layer->GetSafeToDeleteTilings();
  EXPECT_THAT(safe_to_delete, ElementsAre(1.0f, 4.0f));
}

// Verifies that CleanUpTilings returns the correct set of scales for tilings
// that are safe to delete. This is similar to above but more of an integration
// style test since we are using ::AppendQuad() here and not directly using
// LastAppendQuadsScalesForTesting().
TEST_F(TileDisplayLayerImplTest, GetSafeToDeleteTilingsIntegration) {
  constexpr gfx::Size kLayerBounds(100, 100);
  constexpr gfx::Rect kLayerRect(kLayerBounds);

  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = 1.0;

  // Create three tilings.
  auto& tiling_1 = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling_1.SetTileSize(kLayerBounds);
  tiling_1.SetTilingRect(kLayerRect);
  tiling_1.SetTileContents(TileIndex{0, 0}, SkColors::kRed, true);

  auto& tiling_2 = raw_layer->GetOrCreateTilingFromScaleKey(2.0);
  tiling_2.SetTileSize(kLayerBounds);
  tiling_2.SetTilingRect(kLayerRect);
  tiling_2.SetTileContents(TileIndex{0, 0}, SkColors::kGreen, true);

  auto& tiling_3 = raw_layer->GetOrCreateTilingFromScaleKey(3.0);
  tiling_3.SetTileSize(kLayerBounds);
  tiling_3.SetTilingRect(kLayerRect);
  tiling_3.SetTileContents(TileIndex{0, 0}, SkColors::kBlue, true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // By default, the ideal resolution tiling (1.0) is used.
  raw_layer->SetProposedTilingScalesForDeletion({1.0, 2.0});
  std::vector<float> safe_to_delete = raw_layer->GetSafeToDeleteTilings();
  EXPECT_THAT(safe_to_delete, ElementsAre(2.0f));

  raw_layer->SetProposedTilingScalesForDeletion({1.0, 3.0});
  safe_to_delete = raw_layer->GetSafeToDeleteTilings();
  EXPECT_THAT(safe_to_delete, ElementsAre(3.0f));

  raw_layer->SetProposedTilingScalesForDeletion({3.0});
  safe_to_delete = raw_layer->GetSafeToDeleteTilings();
  EXPECT_THAT(safe_to_delete, ElementsAre(3.0f));

  raw_layer->SetProposedTilingScalesForDeletion({1.0, 2.0, 3.0});
  safe_to_delete = raw_layer->GetSafeToDeleteTilings();
  EXPECT_THAT(safe_to_delete, ElementsAre(2.0f, 3.0f));
}

// Verifies that AppendQuads() appends debug borders for a solid-color tile when
// they are enabled.
TEST_F(TileDisplayLayerImplTest,
       AppendQuadsAppendsDebugBordersForSolidColorTile) {
  // Enable debug borders.
  LayerTreeDebugState debug_state;
  debug_state.show_debug_borders.set(DebugBorderType::LAYER);
  host_impl()->SetDebugState(debug_state);

  // Set up the layer.
  constexpr gfx::Size kLayerBounds(100, 100);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = 1.0f;

  // Add a tiling with a solid-color tile.
  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);
  tiling.SetTileContents(TileIndex{0, 0}, SkColors::kRed,
                         /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  // Append quads.
  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // Verify that a layer debug border, a tile debug border, and a content quad
  // were appended.
  ASSERT_EQ(render_pass->quad_list.size(), 3u);
  auto it = render_pass->quad_list.begin();
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kDebugBorder);
  EXPECT_EQ(viz::DebugBorderDrawQuad::MaterialCast(*it)->color,
            DebugColors::ContainerLayerBorderColor());
  ++it;
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kDebugBorder);
  EXPECT_EQ(viz::DebugBorderDrawQuad::MaterialCast(*it)->color,
            DebugColors::SolidColorTileBorderColor());
  ++it;
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kSolidColor);
  EXPECT_EQ(viz::SolidColorDrawQuad::MaterialCast(*it)->color, SkColors::kRed);
}

// Verifies that AppendQuads() appends debug borders for a resource tile when
// debug borders are enabled.
TEST_F(TileDisplayLayerImplTest,
       AppendQuadsAppendsDebugBordersForResourceTile) {
  // Enable debug borders.
  LayerTreeDebugState debug_state;
  debug_state.show_debug_borders.set(DebugBorderType::LAYER);
  host_impl()->SetDebugState(debug_state);

  // Set up the layer.
  constexpr gfx::Size kLayerBounds(100, 100);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = 1.0f;

  // Add a tiling with a resource tile.
  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);

  auto resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents contents_resource =
      TileDisplayLayerTileResource(resource_id, kLayerBounds);
  tiling.SetTileContents(TileIndex{0, 0}, contents_resource,
                         /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  // Append quads.
  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // Verify that a layer debug border, a tile debug border, and a content quad
  // were appended.
  ASSERT_EQ(render_pass->quad_list.size(), 3u);
  auto it = render_pass->quad_list.begin();

  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kDebugBorder);
  EXPECT_EQ(viz::DebugBorderDrawQuad::MaterialCast(*it)->color,
            DebugColors::ContainerLayerBorderColor());
  ++it;
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kDebugBorder);
  EXPECT_EQ(viz::DebugBorderDrawQuad::MaterialCast(*it)->color,
            DebugColors::HighResTileBorderColor());
  ++it;
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kTiledContent);
  EXPECT_EQ(viz::TileDrawQuad::MaterialCast(*it)->resource_id, resource_id);
}

// Verifies that AppendQuads() appends debug borders for an extra-high-res
// resource tile when debug borders are enabled.
TEST_F(TileDisplayLayerImplTest,
       AppendQuadsAppendsDebugBordersForExtraHighResResourceTile) {
  // Enable debug borders.
  LayerTreeDebugState debug_state;
  debug_state.show_debug_borders.set(DebugBorderType::LAYER);
  host_impl()->SetDebugState(debug_state);

  // Set up the layer.
  constexpr gfx::Size kLayerBounds(100, 100);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = 1.0f;

  // Add an above-high-res tiling with a resource tile.
  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(2.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);

  auto resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents contents_resource =
      TileDisplayLayerTileResource(resource_id, kLayerBounds);
  tiling.SetTileContents(TileIndex{0, 0}, contents_resource,
                         /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  // Append quads.
  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // Verify that a layer debug border, a tile debug border, and a content quad
  // were appended.
  ASSERT_EQ(render_pass->quad_list.size(), 3u);
  auto it = render_pass->quad_list.begin();

  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kDebugBorder);
  EXPECT_EQ(viz::DebugBorderDrawQuad::MaterialCast(*it)->color,
            DebugColors::ContainerLayerBorderColor());
  ++it;
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kDebugBorder);
  EXPECT_EQ(viz::DebugBorderDrawQuad::MaterialCast(*it)->color,
            DebugColors::AboveHighResTileBorderColor());
  ++it;
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kTiledContent);
  EXPECT_EQ(viz::TileDrawQuad::MaterialCast(*it)->resource_id, resource_id);
}

// Verifies that AppendQuads() appends debug borders for an OOM tile when debug
// borders are enabled.
TEST_F(TileDisplayLayerImplTest, AppendQuadsAppendsDebugBordersForOomTile) {
  // Enable debug borders.
  LayerTreeDebugState debug_state;
  debug_state.show_debug_borders.set(DebugBorderType::LAYER);
  host_impl()->SetDebugState(debug_state);

  // Set up the layer.
  constexpr gfx::Size kLayerBounds(100, 100);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = 1.0f;

  // Add a tiling with an OOM tile.
  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);
  tiling.SetTileContents(
      TileIndex{0, 0},
      TileDisplayLayerNoContents{mojom::MissingTileReason::kOutOfMemory},
      /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  // Append quads.
  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // Verify that a layer debug border, an OOM tile debug border, and a solid
  // color quad were appended.
  ASSERT_EQ(render_pass->quad_list.size(), 3u);
  auto it = render_pass->quad_list.begin();

  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kDebugBorder);
  EXPECT_EQ(viz::DebugBorderDrawQuad::MaterialCast(*it)->color,
            DebugColors::ContainerLayerBorderColor());
  ++it;
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kDebugBorder);
  EXPECT_EQ(viz::DebugBorderDrawQuad::MaterialCast(*it)->color,
            DebugColors::OOMTileBorderColor());
  ++it;
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kSolidColor);
  EXPECT_EQ(viz::SolidColorDrawQuad::MaterialCast(*it)->color,
            raw_layer->safe_opaque_background_color());
}

// Verifies that AppendQuads() appends debug borders for a not-ready tile when
// debug borders are enabled.
TEST_F(TileDisplayLayerImplTest,
       AppendQuadsAppendsDebugBordersForNotReadyTile) {
  // Enable debug borders.
  LayerTreeDebugState debug_state;
  debug_state.show_debug_borders.set(DebugBorderType::LAYER);
  host_impl()->SetDebugState(debug_state);

  // Set up the layer.
  constexpr gfx::Size kLayerBounds(100, 100);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->SetRecordedBounds(kLayerRect);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = 1.0f;

  // Add a tiling with a not-ready tile.
  auto& tiling = raw_layer->GetOrCreateTilingFromScaleKey(1.0);
  tiling.SetTileSize(kLayerBounds);
  tiling.SetTilingRect(kLayerRect);
  tiling.SetTileContents(
      TileIndex{0, 0},
      TileDisplayLayerNoContents{mojom::MissingTileReason::kResourceNotReady},
      /*update_damage=*/true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  // Append quads.
  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // Verify that a layer debug border, a missing tile debug border, and a
  // checkerboarded quad for the missing content were appended.
  ASSERT_EQ(render_pass->quad_list.size(), 3u);
  auto it = render_pass->quad_list.begin();

  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kDebugBorder);
  EXPECT_EQ(viz::DebugBorderDrawQuad::MaterialCast(*it)->color,
            DebugColors::ContainerLayerBorderColor());
  ++it;
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kDebugBorder);
  EXPECT_EQ(viz::DebugBorderDrawQuad::MaterialCast(*it)->color,
            DebugColors::MissingTileBorderColor());
  ++it;
  EXPECT_EQ((*it)->material, viz::DrawQuad::Material::kSolidColor);
  EXPECT_EQ(viz::SolidColorDrawQuad::MaterialCast(*it)->color,
            raw_layer->safe_opaque_background_color());
}

TEST_F(TileDisplayLayerImplTest, TileResourceIsOOM) {
  auto layer = std::make_unique<TileDisplayLayerImpl>(
      CHECK_DEREF(host_impl()->active_tree()), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  // Missing tile due to OOM.
  TileDisplayLayerImpl::TileContents oom_contents{
      TileDisplayLayerNoContents(mojom::MissingTileReason::kOutOfMemory)};
  TileDisplayLayerTile oom_tile(*raw_layer, oom_contents);
  EXPECT_TRUE(oom_tile.is_oom());

  // OOM tiles should be regarded as ready to draw.
  EXPECT_TRUE(oom_tile.IsReadyToDraw());

  // Missing tile due to resource not being ready.
  TileDisplayLayerImpl::TileContents resource_not_ready_contents{
      TileDisplayLayerNoContents(mojom::MissingTileReason::kResourceNotReady)};
  TileDisplayLayerTile not_oom_tile(*raw_layer, resource_not_ready_contents);
  EXPECT_FALSE(not_oom_tile.is_oom());

  // Non-OOM missing tiles should not be regarded as ready to draw.
  EXPECT_FALSE(not_oom_tile.IsReadyToDraw());

  // Solid color tile.
  TileDisplayLayerImpl::TileContents color_contents(SkColors::kRed);
  TileDisplayLayerTile color_tile(*raw_layer, color_contents);
  EXPECT_FALSE(color_tile.is_oom());
  EXPECT_TRUE(color_tile.IsReadyToDraw());

  // Resource tile.
  auto resource_id = host_impl()->resource_provider()->ImportResource(
      viz::TransferableResource::Make(
          gpu::ClientSharedImage::CreateForTesting(),
          viz::TransferableResource::ResourceSource::kTest, gpu::SyncToken()),
      base::DoNothing());
  TileDisplayLayerImpl::TileContents resource_contents =
      TileDisplayLayerTileResource(resource_id, gfx::Size(1, 1));
  TileDisplayLayerTile resource_tile(*raw_layer, resource_contents);
  EXPECT_FALSE(resource_tile.is_oom());
  EXPECT_TRUE(resource_tile.IsReadyToDraw());
}

}  // namespace cc
