// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/append_quads_data.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/test/mock_occlusion_tracker.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

#include "cc/test/fake_raster_source.h"

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SURFACE_CHANGED(code_to_test) \
  render_surface->ResetPropertyChangedFlags();           \
  code_to_test;                                          \
  EXPECT_TRUE(render_surface->SurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(code_to_test) \
  render_surface->ResetPropertyChangedFlags();                  \
  code_to_test;                                                 \
  EXPECT_FALSE(render_surface->SurfacePropertyChanged())

// The AppendQuads method of this class generates multiple quads and one of them
// does not intersect with visible_layer_rect.
class FakePictureLayerImplForRenderSurfaceTest : public FakePictureLayerImpl {
 public:
  static std::unique_ptr<FakePictureLayerImplForRenderSurfaceTest> Create(
      LayerTreeImpl* tree_impl,
      int id) {
    return base::WrapUnique(
        new FakePictureLayerImplForRenderSurfaceTest(tree_impl, id));
  }

  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) override {
    return base::WrapUnique(
        new FakePictureLayerImplForRenderSurfaceTest(tree_impl, id()));
  }

  void SetQuadRectsForTesting(std::vector<gfx::Rect> quad_rects) {
    quad_rects_ = quad_rects;
  }

  bool HasValidTilePriorities() const override { return false; }

  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    viz::SharedQuadState* shared_quad_state =
        render_pass->CreateAndAppendSharedQuadState();
    float max_contents_scale = 1.f;
    PopulateScaledSharedQuadState(shared_quad_state, max_contents_scale,
                                  contents_opaque());
    bool needs_blending = false;
    for (const auto& rect : quad_rects_) {
      auto* quad = render_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
      quad->SetNew(shared_quad_state, rect, rect, needs_blending, 0,
                   gfx::RectF(rect), bounds(), false, false, false);
    }
  }

 protected:
  FakePictureLayerImplForRenderSurfaceTest(LayerTreeImpl* tree_impl, int id)
      : FakePictureLayerImpl(tree_impl, id) {}

  std::vector<gfx::Rect> quad_rects_;
};

TEST(RenderSurfaceTest, VerifySurfaceChangesAreTrackedProperly) {
  //
  // This test checks that SurfacePropertyChanged() has the correct behavior.
  //

  LayerTreeImplTestBase impl;
  LayerImpl* root = impl.root_layer();
  LayerTreeImpl* active_tree = impl.host_impl()->active_tree();

  LayerImpl* layer = impl.AddLayer<LayerImpl>();
  CopyProperties(root, layer);
  CreateEffectNode(layer).render_surface_reason = RenderSurfaceReason::kTest;

  UpdateDrawProperties(active_tree);
  gfx::Rect test_rect(3, 4, 5, 6);
  active_tree->ResetAllChangeTracking();

  RenderSurfaceImpl* render_surface = GetRenderSurface(layer);
  ASSERT_TRUE(render_surface);

  // Currently, the content_rect, clip_rect, and
  // owning_layer->layerPropertyChanged() are the only sources of change.
  EXECUTE_AND_VERIFY_SURFACE_CHANGED(render_surface->SetClipRect(test_rect));
  EXECUTE_AND_VERIFY_SURFACE_CHANGED(
      render_surface->SetContentRectForTesting(test_rect));

  active_tree->SetOpacityMutated(root->element_id(), 0.5f);
  EXPECT_TRUE(render_surface->SurfacePropertyChanged());
  active_tree->ResetAllChangeTracking();

  // Setting the surface properties to the same values again should not be
  // considered "change".
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetClipRect(test_rect));
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetContentRectForTesting(test_rect));

  constexpr int kArbitraryLayerId = 200;
  std::unique_ptr<LayerImpl> dummy_mask =
      LayerImpl::Create(active_tree, kArbitraryLayerId);
  gfx::Transform dummy_matrix;
  dummy_matrix.Translate(1.0, 2.0);

  // The rest of the surface properties are either internal and should not cause
  // change, or they are already accounted for by the
  // owninglayer->layerPropertyChanged().
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetDrawOpacity(0.5f));
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetDrawTransform(dummy_matrix));
}

TEST(RenderSurfaceTest, SanityCheckSurfaceCreatesCorrectSharedQuadState) {
  LayerTreeImplTestBase impl;
  LayerImpl* root = impl.root_layer();

  LayerImpl* layer = impl.AddLayer<LayerImpl>();
  CopyProperties(root, layer);
  auto& effect_node = CreateEffectNode(layer);
  effect_node.render_surface_reason = RenderSurfaceReason::kBlendMode;
  SkBlendMode blend_mode = SkBlendMode::kSoftLight;
  effect_node.blend_mode = blend_mode;

  UpdateDrawProperties(impl.host_impl()->active_tree());

  RenderSurfaceImpl* render_surface = GetRenderSurface(layer);
  ASSERT_TRUE(render_surface);

  gfx::Rect content_rect(0, 0, 50, 50);
  gfx::Rect clip_rect(5, 5, 40, 40);
  gfx::Transform origin;
  origin.Translate(30, 40);

  render_surface->SetContentRectForTesting(content_rect);
  render_surface->SetClipRect(clip_rect);
  render_surface->SetDrawOpacity(1.f);
  render_surface->SetDrawTransform(origin);

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  AppendQuadsData append_quads_data;

  render_surface->AppendQuads(DRAW_MODE_HARDWARE, render_pass.get(),
                              &append_quads_data);

  ASSERT_EQ(1u, render_pass->shared_quad_state_list.size());
  viz::SharedQuadState* shared_quad_state =
      render_pass->shared_quad_state_list.front();

  EXPECT_EQ(
      30.0,
      shared_quad_state->quad_to_target_transform.matrix().getDouble(0, 3));
  EXPECT_EQ(
      40.0,
      shared_quad_state->quad_to_target_transform.matrix().getDouble(1, 3));
  EXPECT_EQ(content_rect,
            gfx::Rect(shared_quad_state->visible_quad_layer_rect));
  EXPECT_EQ(1.f, shared_quad_state->opacity);
  EXPECT_EQ(blend_mode, shared_quad_state->blend_mode);
}

TEST(RenderSurfaceTest, SanityCheckSurfaceCreatesCorrectRenderPass) {
  LayerTreeImplTestBase impl;
  LayerImpl* root = impl.root_layer();

  LayerImpl* layer = impl.AddLayer<LayerImpl>();
  CopyProperties(root, layer);
  auto& effect_node = CreateEffectNode(layer);
  effect_node.render_surface_reason = RenderSurfaceReason::kTest;

  UpdateDrawProperties(impl.host_impl()->active_tree());

  RenderSurfaceImpl* render_surface = GetRenderSurface(layer);
  ASSERT_TRUE(render_surface);

  gfx::Rect content_rect(0, 0, 50, 50);
  gfx::Transform origin;
  origin.Translate(30.0, 40.0);

  render_surface->SetScreenSpaceTransform(origin);
  render_surface->SetContentRectForTesting(content_rect);

  auto pass = render_surface->CreateRenderPass();

  EXPECT_EQ(2u, pass->id);
  EXPECT_EQ(content_rect, pass->output_rect);
  EXPECT_EQ(origin, pass->transform_to_root_target);
}

TEST(RenderSurfaceTest, SanityCheckSurfaceIgnoreMaskLayerOcclusion) {
  LayerTreeImplTestBase impl;
  LayerImpl* root = impl.root_layer();
  // Set a big enough viewport to show the entire render pass.
  impl.host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(1000, 1000));

  auto* layer = impl.AddLayer<LayerImpl>();
  layer->SetBounds(gfx::Size(200, 100));
  layer->SetDrawsContent(true);
  CopyProperties(root, layer);
  CreateEffectNode(layer);

  auto* mask_layer = impl.AddLayer<FakePictureLayerImplForRenderSurfaceTest>();
  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(mask_layer->bounds()));
  mask_layer->SetRasterSource(raster_source, Region());
  std::vector<gfx::Rect> quad_rects;
  quad_rects.push_back(gfx::Rect(0, 0, 100, 100));
  quad_rects.push_back(gfx::Rect(100, 0, 100, 100));
  mask_layer->SetQuadRectsForTesting(quad_rects);
  SetupMaskProperties(layer, mask_layer);

  UpdateDrawProperties(impl.host_impl()->active_tree());

  RenderSurfaceImpl* render_surface = GetRenderSurface(layer);
  ASSERT_TRUE(render_surface);

  gfx::Rect content_rect(0, 0, 200, 100);
  gfx::Rect occluded(0, 0, 200, 100);

  render_surface->SetContentRectForTesting(content_rect);
  mask_layer->draw_properties().occlusion_in_content_space =
      Occlusion(gfx::Transform(), SimpleEnclosedRegion(occluded),
                SimpleEnclosedRegion(occluded));

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  AppendQuadsData append_quads_data;

  render_surface->AppendQuads(DRAW_MODE_HARDWARE, render_pass.get(),
                              &append_quads_data);

  ASSERT_EQ(1u, render_pass->shared_quad_state_list.size());
  viz::SharedQuadState* shared_quad_state =
      render_pass->shared_quad_state_list.front();

  EXPECT_EQ(content_rect,
            gfx::Rect(shared_quad_state->visible_quad_layer_rect));
  ASSERT_EQ(1u, render_pass->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 100).ToString(),
            render_pass->quad_list.front()->rect.ToString());
}

}  // namespace
}  // namespace cc
