// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "cc/layers/append_quads_data.h"
#include "cc/layers/ui_resource_layer_impl.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_ui_resource_layer_tree_host_impl.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/viz/common/quads/draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

UIResourceLayerImpl* GenerateUIResourceLayer(
    FakeUIResourceLayerTreeHostImpl* host_impl,
    const gfx::Size& bitmap_size,
    const gfx::Size& layer_size,
    bool opaque,
    UIResourceId uid) {
  gfx::Rect visible_layer_rect(layer_size);
  std::unique_ptr<UIResourceLayerImpl> layer =
      UIResourceLayerImpl::Create(host_impl->active_tree(), 1);
  layer->draw_properties().visible_layer_rect = visible_layer_rect;
  layer->SetBounds(layer_size);

  UIResourceBitmap bitmap(bitmap_size, opaque);

  host_impl->CreateUIResource(uid, bitmap);
  layer->SetUIResourceId(uid);

  host_impl->active_tree()->property_trees()->clear();
  auto* layer_ptr = layer.get();
  SetupRootProperties(layer_ptr);
  host_impl->active_tree()->SetRootLayerForTesting(std::move(layer));
  UpdateDrawProperties(host_impl->active_tree());

  return layer_ptr;
}

void QuadSizeTest(FakeUIResourceLayerTreeHostImpl* host_impl,
                  UIResourceLayerImpl* layer,
                  size_t expected_quad_size) {
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  AppendQuadsData data;
  host_impl->active_tree()->root_layer()->AppendQuads(render_pass.get(), &data);

  // Verify quad rects
  const viz::QuadList& quads = render_pass->quad_list;
  EXPECT_EQ(expected_quad_size, quads.size());

  host_impl->active_tree()->DetachLayers();
}

TEST(UIResourceLayerImplTest, VerifyDrawQuads) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink =
      FakeLayerTreeFrameSink::Create3d();
  FakeUIResourceLayerTreeHostImpl host_impl(&task_runner_provider,
                                            &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeFrameSink(layer_tree_frame_sink.get());

  // Make sure we're appending quads when there are valid values.
  gfx::Size bitmap_size(100, 100);
  gfx::Size layer_size(100, 100);
  size_t expected_quad_size = 1;
  bool opaque = true;
  UIResourceId uid = 1;
  auto* layer =
      GenerateUIResourceLayer(&host_impl, bitmap_size, layer_size, opaque, uid);
  QuadSizeTest(&host_impl, layer, expected_quad_size);
  host_impl.DeleteUIResource(uid);

  // Make sure we're not appending quads when there are invalid values.
  expected_quad_size = 0;
  uid = 0;
  layer = GenerateUIResourceLayer(&host_impl,
                                  bitmap_size,
                                  layer_size,
                                  opaque,
                                  uid);
  QuadSizeTest(&host_impl, layer, expected_quad_size);
  host_impl.DeleteUIResource(uid);
}

void NeedsBlendingTest(FakeUIResourceLayerTreeHostImpl* host_impl,
                       UIResourceLayerImpl* layer,
                       bool needs_blending) {
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  AppendQuadsData data;
  host_impl->active_tree()->root_layer()->AppendQuads(render_pass.get(), &data);

  // Verify needs_blending is set appropriately.
  const viz::QuadList& quads = render_pass->quad_list;
  EXPECT_GE(quads.size(), (size_t)0);
  EXPECT_EQ(needs_blending, quads.front()->needs_blending);
  EXPECT_EQ(quads.front()->needs_blending,
            !quads.front()->shared_quad_state->are_contents_opaque);

  host_impl->active_tree()->DetachLayers();
}

TEST(UIResourceLayerImplTest, VerifySetOpaqueOnSkBitmap) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink =
      FakeLayerTreeFrameSink::Create3d();
  FakeUIResourceLayerTreeHostImpl host_impl(&task_runner_provider,
                                            &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeFrameSink(layer_tree_frame_sink.get());

  gfx::Size bitmap_size(100, 100);
  gfx::Size layer_size(100, 100);
  bool opaque = false;
  UIResourceId uid = 1;
  auto* layer =
      GenerateUIResourceLayer(&host_impl, bitmap_size, layer_size, opaque, uid);
  NeedsBlendingTest(&host_impl, layer, !opaque);
  host_impl.DeleteUIResource(uid);

  opaque = true;
  layer = GenerateUIResourceLayer(&host_impl,
                                  bitmap_size,
                                  layer_size,
                                  opaque,
                                  uid);
  NeedsBlendingTest(&host_impl, layer, !opaque);
  host_impl.DeleteUIResource(uid);
}

TEST(UIResourceLayerImplTest, VerifySetOpaqueOnLayer) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink =
      FakeLayerTreeFrameSink::Create3d();
  FakeUIResourceLayerTreeHostImpl host_impl(&task_runner_provider,
                                            &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeFrameSink(layer_tree_frame_sink.get());

  gfx::Size bitmap_size(100, 100);
  gfx::Size layer_size(100, 100);
  bool skbitmap_opaque = false;
  UIResourceId uid = 1;
  auto* layer = GenerateUIResourceLayer(&host_impl, bitmap_size, layer_size,
                                        skbitmap_opaque, uid);
  bool opaque = false;
  layer->SetContentsOpaque(opaque);
  NeedsBlendingTest(&host_impl, layer, !opaque);
  host_impl.DeleteUIResource(uid);

  opaque = true;
  layer = GenerateUIResourceLayer(
      &host_impl, bitmap_size, layer_size, skbitmap_opaque, uid);
  layer->SetContentsOpaque(true);
  NeedsBlendingTest(&host_impl, layer, !opaque);
  host_impl.DeleteUIResource(uid);
}

TEST(UIResourceLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTreeImplTestBase impl;

  SkBitmap sk_bitmap;
  sk_bitmap.allocN32Pixels(10, 10);
  sk_bitmap.setImmutable();
  UIResourceId uid = 5;
  UIResourceBitmap bitmap(sk_bitmap);
  impl.host_impl()->CreateUIResource(uid, bitmap);

  UIResourceLayerImpl* ui_resource_layer_impl =
      impl.AddLayer<UIResourceLayerImpl>();
  ui_resource_layer_impl->SetBounds(layer_size);
  ui_resource_layer_impl->SetDrawsContent(true);
  ui_resource_layer_impl->SetUIResourceId(uid);
  CopyProperties(impl.root_layer(), ui_resource_layer_impl);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(ui_resource_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(ui_resource_layer_impl->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(ui_resource_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendQuadsWithOcclusion(ui_resource_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    VerifyQuadsAreOccluded(impl.quad_list(), occluded,
                           &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

}  // namespace
}  // namespace cc
