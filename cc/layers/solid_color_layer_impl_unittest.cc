// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer_impl.h"

#include <stddef.h>

#include <vector>

#include "cc/animation/animation_host.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class SolidColorLayerImplTest : public LayerTreeImplTestBase,
                                public ::testing::Test {};

TEST_F(SolidColorLayerImplTest, VerifyTilingCompleteAndNoOverlap) {
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  gfx::Size layer_size = gfx::Size(800, 600);
  gfx::Rect visible_layer_rect = gfx::Rect(layer_size);
  root_layer()->SetBounds(layer_size);

  auto* layer = AddLayer<SolidColorLayerImpl>();
  layer->SetBounds(layer_size);
  layer->SetDrawsContent(true);
  layer->SetBackgroundColor(SK_ColorRED);
  CopyProperties(root_layer(), layer);
  CreateEffectNode(layer).render_surface_reason = RenderSurfaceReason::kTest;
  UpdateActiveTreeDrawProperties();
  AppendQuadsData data;
  layer->AppendQuads(render_pass.get(), &data);

  VerifyQuadsExactlyCoverRect(render_pass->quad_list, visible_layer_rect);
}

TEST_F(SolidColorLayerImplTest, VerifyCorrectBackgroundColorInQuad) {
  SkColor test_color = 0xFFA55AFF;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_layer_rect = gfx::Rect(layer_size);
  root_layer()->SetBounds(layer_size);

  auto* layer = AddLayer<SolidColorLayerImpl>();
  layer->SetBounds(layer_size);
  layer->SetDrawsContent(true);
  layer->SetBackgroundColor(test_color);
  CopyProperties(root_layer(), layer);
  CreateEffectNode(layer).render_surface_reason = RenderSurfaceReason::kTest;
  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(visible_layer_rect, layer->draw_properties().visible_layer_rect);

  AppendQuadsData data;
  layer->AppendQuads(render_pass.get(), &data);

  ASSERT_EQ(render_pass->quad_list.size(), 1U);
  EXPECT_EQ(
      viz::SolidColorDrawQuad::MaterialCast(render_pass->quad_list.front())
          ->color,
      test_color);
}

TEST_F(SolidColorLayerImplTest, VerifyCorrectOpacityInQuad) {
  const float opacity = 0.5f;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  gfx::Size layer_size = gfx::Size(100, 100);

  auto* layer = AddLayer<SolidColorLayerImpl>();
  layer->SetDrawsContent(true);
  layer->SetBounds(layer_size);
  layer->SetBackgroundColor(SK_ColorRED);
  CopyProperties(root_layer(), layer);
  auto& effect_node = CreateEffectNode(layer);
  effect_node.opacity = opacity;
  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(opacity, layer->draw_properties().opacity);

  AppendQuadsData data;
  layer->AppendQuads(render_pass.get(), &data);

  ASSERT_EQ(render_pass->quad_list.size(), 1U);
  EXPECT_EQ(opacity, viz::SolidColorDrawQuad::MaterialCast(
                         render_pass->quad_list.front())
                         ->shared_quad_state->opacity);
  EXPECT_TRUE(render_pass->quad_list.front()->ShouldDrawWithBlending());
}

TEST_F(SolidColorLayerImplTest, VerifyCorrectRenderSurfaceOpacityInQuad) {
  const float opacity = 0.5f;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  gfx::Size layer_size = gfx::Size(100, 100);

  auto* layer = AddLayer<SolidColorLayerImpl>();
  layer->SetDrawsContent(true);
  layer->SetBounds(layer_size);
  layer->SetBackgroundColor(SK_ColorRED);
  CopyProperties(root_layer(), layer);
  auto& effect_node = CreateEffectNode(layer);
  effect_node.render_surface_reason = RenderSurfaceReason::kTest;
  effect_node.opacity = opacity;
  UpdateActiveTreeDrawProperties();

  // Opacity is applied on render surface, so the layer doesn't have opacity.
  EXPECT_EQ(1.f, layer->draw_properties().opacity);

  AppendQuadsData data;
  layer->AppendQuads(render_pass.get(), &data);

  ASSERT_EQ(render_pass->quad_list.size(), 1U);
  // Opacity is applied on render surface, so the quad doesn't have opacity.
  EXPECT_EQ(
      1.f, viz::SolidColorDrawQuad::MaterialCast(render_pass->quad_list.front())
               ->shared_quad_state->opacity);
  EXPECT_FALSE(render_pass->quad_list.front()->ShouldDrawWithBlending());
}

TEST_F(SolidColorLayerImplTest, VerifyEliminateTransparentAlpha) {
  SkColor test_color = 0;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  gfx::Size layer_size = gfx::Size(100, 100);

  auto* layer = AddLayer<SolidColorLayerImpl>();
  layer->SetBounds(layer_size);
  layer->SetDrawsContent(true);
  layer->SetBackgroundColor(test_color);
  CopyProperties(root_layer(), layer);
  CreateEffectNode(layer).render_surface_reason = RenderSurfaceReason::kTest;
  UpdateActiveTreeDrawProperties();

  AppendQuadsData data;
  layer->AppendQuads(render_pass.get(), &data);
  EXPECT_EQ(render_pass->quad_list.size(), 0U);
}

TEST_F(SolidColorLayerImplTest, VerifyEliminateTransparentOpacity) {
  SkColor test_color = 0xFFA55AFF;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  gfx::Size layer_size = gfx::Size(100, 100);

  auto* layer = AddLayer<SolidColorLayerImpl>();
  layer->SetBounds(layer_size);
  layer->SetDrawsContent(true);
  layer->SetBackgroundColor(test_color);
  CopyProperties(root_layer(), layer);
  auto& effect_node = CreateEffectNode(layer);
  effect_node.render_surface_reason = RenderSurfaceReason::kTest;
  effect_node.opacity = 0.f;
  UpdateActiveTreeDrawProperties();

  AppendQuadsData data;
  layer->AppendQuads(render_pass.get(), &data);
  EXPECT_EQ(render_pass->quad_list.size(), 0U);
}

TEST_F(SolidColorLayerImplTest, VerifyNeedsBlending) {
  gfx::Size layer_size = gfx::Size(100, 100);

  scoped_refptr<SolidColorLayer> layer = SolidColorLayer::Create();
  layer->SetBounds(layer_size);
  layer->SetForceRenderSurfaceForTesting(true);

  scoped_refptr<Layer> root = Layer::Create();
  root->AddChild(layer);

  FakeLayerTreeHostClient client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(root);

  UpdateDrawProperties(host.get());

  EXPECT_FALSE(layer->contents_opaque());
  layer->SetBackgroundColor(SkColorSetARGB(255, 10, 20, 30));
  EXPECT_TRUE(layer->contents_opaque());
  {
    DebugScopedSetImplThread scoped_impl_thread(host->GetTaskRunnerProvider());
    host->FinishCommitOnImplThread(host->host_impl());
    LayerImpl* layer_impl =
        host->host_impl()->active_tree()->LayerById(layer->id());

    // The impl layer should call itself opaque as well.
    EXPECT_TRUE(layer_impl->contents_opaque());

    // Impl layer has 1 opacity, and the color is opaque, so the needs_blending
    // should be the false.
    layer_impl->draw_properties().opacity = 1;

    std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

    AppendQuadsData data;
    layer_impl->AppendQuads(render_pass.get(), &data);

    ASSERT_EQ(render_pass->quad_list.size(), 1U);
    EXPECT_FALSE(render_pass->quad_list.front()->needs_blending);
    EXPECT_TRUE(
        render_pass->quad_list.front()->shared_quad_state->are_contents_opaque);
  }

  EXPECT_TRUE(layer->contents_opaque());
  layer->SetBackgroundColor(SkColorSetARGB(254, 10, 20, 30));
  EXPECT_FALSE(layer->contents_opaque());
  {
    DebugScopedSetImplThread scoped_impl_thread(host->GetTaskRunnerProvider());
    host->FinishCommitOnImplThread(host->host_impl());
    LayerImpl* layer_impl =
        host->host_impl()->active_tree()->LayerById(layer->id());

    // The impl layer should not call itself opaque anymore.
    EXPECT_FALSE(layer_impl->contents_opaque());

    // Impl layer has 1 opacity, but the color is not opaque, so the
    // needs_blending should be true.
    layer_impl->draw_properties().opacity = 1;

    std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

    AppendQuadsData data;
    layer_impl->AppendQuads(render_pass.get(), &data);

    ASSERT_EQ(render_pass->quad_list.size(), 1U);
    EXPECT_TRUE(render_pass->quad_list.front()->needs_blending);
    EXPECT_FALSE(
        render_pass->quad_list.front()->shared_quad_state->are_contents_opaque);
  }
}

TEST_F(SolidColorLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  auto* solid_color_layer_impl = AddLayer<SolidColorLayerImpl>();
  solid_color_layer_impl->SetBackgroundColor(SkColorSetARGB(255, 10, 20, 30));
  solid_color_layer_impl->SetBounds(layer_size);
  solid_color_layer_impl->SetDrawsContent(true);
  CopyProperties(root_layer(), solid_color_layer_impl);

  CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    AppendQuadsWithOcclusion(solid_color_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(quad_list(), gfx::Rect(layer_size));
    EXPECT_EQ(1u, quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(solid_color_layer_impl->visible_layer_rect());
    AppendQuadsWithOcclusion(solid_color_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(quad_list(), gfx::Rect());
    EXPECT_EQ(quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    AppendQuadsWithOcclusion(solid_color_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    VerifyQuadsAreOccluded(quad_list(), occluded, &partially_occluded_count);
    // 4 quads are completely occluded, 8 are partially occluded.
    EXPECT_EQ(1u, quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

}  // namespace
}  // namespace cc
