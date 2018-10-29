// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer_impl.h"

#include <stddef.h>

#include <vector>

#include "cc/animation/animation_host.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(SolidColorLayerImplTest, VerifyTilingCompleteAndNoOverlap) {
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  gfx::Size layer_size = gfx::Size(800, 600);
  gfx::Rect visible_layer_rect = gfx::Rect(layer_size);

  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  std::unique_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_layer_rect = visible_layer_rect;
  layer->draw_properties().opacity = 1.f;
  layer->SetBounds(layer_size);
  layer->SetBackgroundColor(SK_ColorRED);
  layer->test_properties()->force_render_surface = true;
  host_impl.active_tree()->SetRootLayerForTesting(std::move(layer));
  host_impl.active_tree()->BuildPropertyTreesForTesting();
  AppendQuadsData data;
  host_impl.active_tree()->root_layer_for_testing()->AppendQuads(
      render_pass.get(), &data);

  LayerTestCommon::VerifyQuadsExactlyCoverRect(render_pass->quad_list,
                                               visible_layer_rect);
}

TEST(SolidColorLayerImplTest, VerifyCorrectBackgroundColorInQuad) {
  SkColor test_color = 0xFFA55AFF;

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_layer_rect = gfx::Rect(layer_size);

  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  std::unique_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_layer_rect = visible_layer_rect;
  layer->draw_properties().opacity = 1.f;
  layer->SetBounds(layer_size);
  layer->SetBackgroundColor(test_color);
  layer->test_properties()->force_render_surface = true;
  host_impl.active_tree()->SetRootLayerForTesting(std::move(layer));
  host_impl.active_tree()->BuildPropertyTreesForTesting();
  AppendQuadsData data;
  host_impl.active_tree()->root_layer_for_testing()->AppendQuads(
      render_pass.get(), &data);

  ASSERT_EQ(render_pass->quad_list.size(), 1U);
  EXPECT_EQ(
      viz::SolidColorDrawQuad::MaterialCast(render_pass->quad_list.front())
          ->color,
      test_color);
}

TEST(SolidColorLayerImplTest, VerifyCorrectOpacityInQuad) {
  const float opacity = 0.5f;

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_layer_rect = gfx::Rect(layer_size);

  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  std::unique_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_layer_rect = visible_layer_rect;
  layer->SetBounds(layer_size);
  layer->draw_properties().opacity = opacity;
  layer->test_properties()->force_render_surface = true;
  layer->SetBackgroundColor(SK_ColorRED);
  host_impl.active_tree()->SetRootLayerForTesting(std::move(layer));
  host_impl.active_tree()->BuildPropertyTreesForTesting();
  AppendQuadsData data;
  host_impl.active_tree()->root_layer_for_testing()->AppendQuads(
      render_pass.get(), &data);

  ASSERT_EQ(render_pass->quad_list.size(), 1U);
  EXPECT_EQ(opacity, viz::SolidColorDrawQuad::MaterialCast(
                         render_pass->quad_list.front())
                         ->shared_quad_state->opacity);
  EXPECT_TRUE(render_pass->quad_list.front()->ShouldDrawWithBlending());
}

TEST(SolidColorLayerImplTest, VerifyEliminateTransparentAlpha) {
  SkColor test_color = 0;

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_layer_rect = gfx::Rect(layer_size);

  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  std::unique_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_layer_rect = visible_layer_rect;
  layer->draw_properties().opacity = 1.f;
  layer->SetBounds(layer_size);
  layer->SetBackgroundColor(test_color);
  layer->test_properties()->force_render_surface = true;
  host_impl.active_tree()->SetRootLayerForTesting(std::move(layer));
  host_impl.active_tree()->BuildPropertyTreesForTesting();
  AppendQuadsData data;
  host_impl.active_tree()->root_layer_for_testing()->AppendQuads(
      render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 0U);
}

TEST(SolidColorLayerImplTest, VerifyEliminateTransparentOpacity) {
  SkColor test_color = 0xFFA55AFF;

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  gfx::Size layer_size = gfx::Size(100, 100);
  gfx::Rect visible_layer_rect = gfx::Rect(layer_size);

  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  std::unique_ptr<SolidColorLayerImpl> layer =
      SolidColorLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_layer_rect = visible_layer_rect;
  layer->draw_properties().opacity = 0.f;
  layer->SetBounds(layer_size);
  layer->SetBackgroundColor(test_color);
  layer->test_properties()->force_render_surface = true;
  host_impl.active_tree()->SetRootLayerForTesting(std::move(layer));
  host_impl.active_tree()->BuildPropertyTreesForTesting();
  AppendQuadsData data;
  host_impl.active_tree()->root_layer_for_testing()->AppendQuads(
      render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 0U);
}

TEST(SolidColorLayerImplTest, VerifyNeedsBlending) {
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

  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), gfx::Size(500, 500));
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

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

TEST(SolidColorLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTestCommon::LayerImplTest impl;

  SolidColorLayerImpl* solid_color_layer_impl =
      impl.AddChildToRoot<SolidColorLayerImpl>();
  solid_color_layer_impl->SetBackgroundColor(SkColorSetARGB(255, 10, 20, 30));
  solid_color_layer_impl->SetBounds(layer_size);
  solid_color_layer_impl->SetDrawsContent(true);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(solid_color_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                 gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(solid_color_layer_impl->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(solid_color_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendQuadsWithOcclusion(solid_color_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    LayerTestCommon::VerifyQuadsAreOccluded(
        impl.quad_list(), occluded, &partially_occluded_count);
    // 4 quads are completely occluded, 8 are partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

}  // namespace
}  // namespace cc
