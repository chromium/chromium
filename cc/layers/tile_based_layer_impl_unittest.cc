// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_based_layer_impl.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "cc/layers/append_quads_context.h"
#include "cc/layers/append_quads_data.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_layer_tree_host_base.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TestTileBasedLayerImpl : public TileBasedLayerImpl {
 public:
  TestTileBasedLayerImpl(LayerTreeImpl* tree_impl, int id)
      : TileBasedLayerImpl(tree_impl, id) {}
  using TileBasedLayerImpl::AppendSolidQuad;

 private:
  // TileBasedLayerImpl:
  void AppendQuadsSpecialization(const AppendQuadsContext& context,
                                 viz::CompositorRenderPass* render_pass,
                                 AppendQuadsData* append_quads_data) override {
    NOTREACHED();
  }
};

class TileBasedLayerImplTest : public TestLayerTreeHostBase {};

// Verifies that `AppendSolidQuad()` appends a solid-color quad with the
// requested properties.
TEST_F(TileBasedLayerImplTest, AppendSolidQuad_AppendsCorrectQuad) {
  constexpr gfx::Size kLayerBounds(100, 200);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr SkColor4f kLayerColor = SkColors::kBlue;
  constexpr float kOpacity = 0.5f;

  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;
  raw_layer->SetContentsOpaque(true);

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendSolidQuad(render_pass.get(), &data, kLayerColor);

  EXPECT_EQ(render_pass->quad_list.size(), 1u);

  const auto* quad =
      viz::SolidColorDrawQuad::MaterialCast(render_pass->quad_list.front());
  EXPECT_EQ(quad->material, viz::DrawQuad::Material::kSolidColor);
  EXPECT_EQ(quad->rect, kLayerRect);
  EXPECT_EQ(quad->visible_rect, kLayerRect);
  EXPECT_EQ(quad->color, kLayerColor);

  const auto* shared_quad_state = render_pass->shared_quad_state_list.front();
  EXPECT_EQ(shared_quad_state->quad_layer_rect, kLayerRect);
  EXPECT_EQ(shared_quad_state->visible_quad_layer_rect, kLayerRect);
  EXPECT_EQ(shared_quad_state->opacity, kOpacity);
  EXPECT_TRUE(shared_quad_state->are_contents_opaque);
}

// Verifies that `is_backdrop_filter_mask()` returns false by default.
TEST_F(TileBasedLayerImplTest, IsBackdropFilterMask_DefaultsToFalse) {
  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);
  EXPECT_FALSE(layer->is_backdrop_filter_mask());
}

// Verifies that `is_backdrop_filter_mask()` reflects calls to
// `SetIsBackdropFilterMask()`.
TEST_F(TileBasedLayerImplTest, SetIsBackdropFilterMask_GetterReflectsSetter) {
  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);

  layer->SetIsBackdropFilterMask(true);
  EXPECT_TRUE(layer->is_backdrop_filter_mask());

  layer->SetIsBackdropFilterMask(false);
  EXPECT_FALSE(layer->is_backdrop_filter_mask());
}

// Verifies that calling `SetIsBackdropFilterMask` with the same value multiple
// times doesn't change the value.
TEST_F(TileBasedLayerImplTest, SetIsBackdropFilterMask_RedundantCalls) {
  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);

  layer->SetIsBackdropFilterMask(true);
  EXPECT_TRUE(layer->is_backdrop_filter_mask());
  layer->SetIsBackdropFilterMask(true);
  EXPECT_TRUE(layer->is_backdrop_filter_mask());

  layer->SetIsBackdropFilterMask(false);
  EXPECT_FALSE(layer->is_backdrop_filter_mask());
  layer->SetIsBackdropFilterMask(false);
  EXPECT_FALSE(layer->is_backdrop_filter_mask());
}

// Verifies that calling `SetIsBackdropFilterMask` with a new value marks the
// layer for push properties in the pending tree.
TEST_F(TileBasedLayerImplTest,
       SetIsBackdropFilterMask_CallsSetNeedsPushProperties) {
  // NOTE: LayerImpl::SetNeedsPushProperties() is a no-op in the active tree, so
  // we need to put the layer in the pending tree here.
  SetupPendingTree();
  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->pending_tree(), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->pending_tree()->AddLayer(std::move(layer));

  // Clear state before running the test.
  host_impl()->pending_tree()->ClearLayersThatShouldPushProperties();
  raw_layer->ResetChangeTracking();

  raw_layer->SetIsBackdropFilterMask(true);

  const auto& layers =
      host_impl()->pending_tree()->LayersThatShouldPushProperties();
  ASSERT_EQ(layers.size(), 1u);
  EXPECT_NE(layers.find(raw_layer), layers.end());
}

// Verifies that calling `SetIsBackdropFilterMask` with the same value as it
// currently has does not mark the layer for push properties in the pending
// tree.
TEST_F(
    TileBasedLayerImplTest,
    SetIsBackdropFilterMask_DoesNotCallSetNeedsPushPropertiesIfValueUnchanged) {
  // NOTE: LayerImpl::SetNeedsPushProperties() is a no-op in the active tree, so
  // we need to put the layer in the pending tree here.
  SetupPendingTree();
  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->pending_tree(), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->pending_tree()->AddLayer(std::move(layer));

  // Clear state before running the test.
  host_impl()->pending_tree()->ClearLayersThatShouldPushProperties();
  raw_layer->ResetChangeTracking();

  raw_layer->SetIsBackdropFilterMask(false);

  const auto& layers =
      host_impl()->pending_tree()->LayersThatShouldPushProperties();
  EXPECT_EQ(layers.size(), 0u);
}

// Tests that AppendQuads() does not append any quads for a layer serving as
// a backdrop filter mask.
TEST_F(TileBasedLayerImplTest,
       AppendQuadsDoesNotAppendQuadsForBackdropFilterMask) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetIsBackdropFilterMask(true);

  // For the production code to actually append a quad, the layer must have
  // non-zero size and not be completely transparent; ensure that these
  // preconditions are satisfied to avoid this test passing trivially.
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 0u);
}

}  // namespace
}  // namespace cc
