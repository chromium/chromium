// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "cc/layers/append_quads_data.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void CheckDrawLayer(HeadsUpDisplayLayerImpl* layer,
                    LayerTreeFrameSink* frame_sink,
                    viz::ClientResourceProvider* resource_provider,
                    viz::ContextProvider* context_provider,
                    DrawMode draw_mode) {
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  AppendQuadsData data;
  bool will_draw = layer->WillDraw(draw_mode, resource_provider);
  if (will_draw)
    layer->AppendQuads(render_pass.get(), &data);
  viz::RenderPassList pass_list;
  pass_list.push_back(std::move(render_pass));
  layer->UpdateHudTexture(draw_mode, frame_sink, resource_provider,
                          context_provider, pass_list);
  if (will_draw)
    layer->DidDraw(resource_provider);

  size_t expected_quad_list_size = will_draw ? 1 : 0;
  EXPECT_EQ(expected_quad_list_size, pass_list.back()->quad_list.size());
  EXPECT_EQ(0u, data.num_missing_tiles);
  EXPECT_EQ(0u, data.num_incomplete_tiles);
}

class HeadsUpDisplayLayerImplTest : public LayerTreeImplTestBase,
                                    public ::testing::Test {};

TEST_F(HeadsUpDisplayLayerImplTest, ResourcelessSoftwareDrawAfterResourceLoss) {
  host_impl()->CreatePendingTree();
  auto* root = EnsureRootLayerInPendingTree();
  auto* layer = AddLayerInPendingTree<HeadsUpDisplayLayerImpl>();
  layer->SetBounds(gfx::Size(100, 100));
  layer->set_visible_layer_rect(gfx::Rect(100, 100));
  CopyProperties(root, layer);

  UpdatePendingTreeDrawProperties();

  // Check regular hardware draw is ok.
  CheckDrawLayer(layer, layer_tree_frame_sink(), resource_provider(),
                 layer_tree_frame_sink()->context_provider(),
                 DRAW_MODE_HARDWARE);

  // Simulate a resource loss on transitioning to resourceless software mode.
  layer->ReleaseResources();

  // Should skip resourceless software draw and not crash in UpdateHudTexture.
  CheckDrawLayer(layer, layer_tree_frame_sink(), resource_provider(),
                 layer_tree_frame_sink()->context_provider(),
                 DRAW_MODE_RESOURCELESS_SOFTWARE);
}

TEST_F(HeadsUpDisplayLayerImplTest, CPUAndGPURasterCanvas) {
  host_impl()->CreatePendingTree();
  auto* root = EnsureRootLayerInPendingTree();
  auto* layer = AddLayerInPendingTree<HeadsUpDisplayLayerImpl>();
  layer->SetBounds(gfx::Size(100, 100));
  CopyProperties(root, layer);

  UpdatePendingTreeDrawProperties();

  // Check Ganesh canvas drawing is ok.
  CheckDrawLayer(layer, layer_tree_frame_sink(), resource_provider(),
                 layer_tree_frame_sink()->context_provider(),
                 DRAW_MODE_HARDWARE);

  host_impl()->ReleaseLayerTreeFrameSink();
  auto layer_tree_frame_sink = FakeLayerTreeFrameSink::CreateSoftware();
  host_impl()->InitializeFrameSink(layer_tree_frame_sink.get());

  // Check SW canvas drawing is ok.
  CheckDrawLayer(layer, layer_tree_frame_sink.get(), resource_provider(),
                 nullptr, DRAW_MODE_SOFTWARE);
  host_impl()->ReleaseLayerTreeFrameSink();
}

}  // namespace
}  // namespace cc
