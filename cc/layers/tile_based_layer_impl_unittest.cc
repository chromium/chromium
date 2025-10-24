// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_based_layer_impl.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
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

}  // namespace
}  // namespace cc
