// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/render_surface_impl.h"

#include <stddef.h>

#include "cc/layers/append_quads_data.h"
#include "cc/test/fake_mask_layer_impl.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(RenderSurfaceLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTreeImplTestBase impl;

  LayerImpl* owning_layer_impl = impl.AddLayerInActiveTree<LayerImpl>();
  owning_layer_impl->SetBounds(layer_size);
  owning_layer_impl->SetDrawsContent(true);
  CopyProperties(impl.root_layer(), owning_layer_impl);
  CreateEffectNode(owning_layer_impl).render_surface_reason =
      RenderSurfaceReason::kTest;

  impl.CalcDrawProps(viewport_size);

  RenderSurfaceImpl* render_surface_impl = GetRenderSurface(owning_layer_impl);
  ASSERT_TRUE(render_surface_impl);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendSurfaceQuadsWithOcclusion(render_surface_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(owning_layer_impl->visible_layer_rect());
    impl.AppendSurfaceQuadsWithOcclusion(render_surface_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendSurfaceQuadsWithOcclusion(render_surface_impl, occluded);

    size_t partially_occluded_count = 0;
    VerifyQuadsAreOccluded(impl.quad_list(), occluded,
                           &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

static std::unique_ptr<viz::CompositorRenderPass> DoAppendQuadsWithScaledMask(
    DrawMode draw_mode,
    float device_scale_factor) {
  gfx::Size layer_size(1000, 1000);
  gfx::Rect viewport_rect(1000, 1000);
  float scale_factor = 2;
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilledSolidColor(layer_size);

  LayerTreeImplTestBase impl(CommitToActiveTreeLayerListSettings());
  auto* root = impl.root_layer();

  auto* surface = impl.AddLayerInActiveTree<LayerImpl>();
  surface->SetBounds(layer_size);
  gfx::Transform scale;
  scale.Scale(scale_factor, scale_factor);
  CopyProperties(root, surface);
  CreateTransformNode(surface).local = scale;
  CreateEffectNode(surface).render_surface_reason = RenderSurfaceReason::kTest;

  auto* mask_layer =
      impl.AddLayerInActiveTree<FakeMaskLayerImpl>(raster_source);
  mask_layer->set_resource_size(
      gfx::ScaleToCeiledSize(layer_size, scale_factor));
  SetupMaskProperties(surface, mask_layer);

  auto* child = impl.AddLayerInActiveTree<LayerImpl>();
  child->SetDrawsContent(true);
  child->SetBounds(layer_size);
  CopyProperties(surface, child);

  LayerTreeImpl* active_tree = impl.host_impl()->active_tree();
  active_tree->SetDeviceScaleFactor(device_scale_factor);
  active_tree->SetDeviceViewportRect(viewport_rect);
  UpdateDrawProperties(active_tree);

  RenderSurfaceImpl* render_surface_impl = GetRenderSurface(surface);
  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData append_quads_data;
  render_surface_impl->AppendQuads(draw_mode, render_pass.get(),
                                   &append_quads_data);
  return render_pass;
}

TEST(RenderSurfaceLayerImplTest, AppendQuadsWithScaledMask) {
  std::unique_ptr<viz::CompositorRenderPass> render_pass =
      DoAppendQuadsWithScaledMask(DRAW_MODE_HARDWARE, 1.f);
  DCHECK(render_pass->quad_list.front());
  const viz::CompositorRenderPassDrawQuad* quad =
      viz::CompositorRenderPassDrawQuad::MaterialCast(
          render_pass->quad_list.front());
  // Mask layers don't use quad's mask functionality.
  EXPECT_EQ(gfx::RectF(), quad->mask_uv_rect);
  EXPECT_EQ(gfx::Vector2dF(2.f, 2.f), quad->filters_scale);
  EXPECT_EQ(viz::kInvalidResourceId, quad->mask_resource_id());
}

TEST(RenderSurfaceLayerImplTest, ResourcelessAppendQuadsSkipMask) {
  std::unique_ptr<viz::CompositorRenderPass> render_pass =
      DoAppendQuadsWithScaledMask(DRAW_MODE_RESOURCELESS_SOFTWARE, 1.f);
  DCHECK(render_pass->quad_list.front());
  const viz::CompositorRenderPassDrawQuad* quad =
      viz::CompositorRenderPassDrawQuad::MaterialCast(
          render_pass->quad_list.front());
  EXPECT_EQ(viz::kInvalidResourceId, quad->mask_resource_id());
}

TEST(RenderSurfaceLayerImplTest,
     AppendQuadsWithSolidColorMaskAndDeviceScaleFactor) {
  std::unique_ptr<viz::CompositorRenderPass> render_pass =
      DoAppendQuadsWithScaledMask(DRAW_MODE_HARDWARE, 2.f);
  DCHECK(render_pass->quad_list.front());
  const viz::CompositorRenderPassDrawQuad* quad =
      viz::CompositorRenderPassDrawQuad::MaterialCast(
          render_pass->quad_list.front());
  EXPECT_EQ(gfx::Transform(),
            quad->shared_quad_state->quad_to_target_transform);
  // With tiled mask layer, we only generate mask quads for visible rect. In
  // this case |quad_layer_rect| is not fully covered, but
  // |visible_quad_layer_rect| is fully covered.
  VerifyQuadsExactlyCoverRect(render_pass->quad_list,
                              quad->shared_quad_state->visible_quad_layer_rect);
}

}  // namespace
}  // namespace cc
