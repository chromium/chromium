// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer_impl.h"

#include <stddef.h>

#include "cc/layers/append_quads_data.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace cc {
namespace {

static constexpr viz::FrameSinkId kArbitraryFrameSinkId(1, 1);

TEST(SurfaceLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId(
      9, base::UnguessableToken::Create());

  LayerTreeImplTestBase impl;

  SurfaceLayerImpl* surface_layer_impl = impl.AddLayer<SurfaceLayerImpl>();
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  viz::SurfaceId surface_id(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId);
  surface_layer_impl->SetRange(viz::SurfaceRange(base::nullopt, surface_id),
                               base::nullopt);
  CopyProperties(impl.root_layer(), surface_layer_impl);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(surface_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_TRUE(surface_layer_impl->WillDraw(DRAW_MODE_HARDWARE, nullptr));
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(surface_layer_impl->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(surface_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
    EXPECT_FALSE(surface_layer_impl->WillDraw(DRAW_MODE_HARDWARE, nullptr));
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendQuadsWithOcclusion(surface_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    VerifyQuadsAreOccluded(impl.quad_list(), occluded,
                           &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
    EXPECT_TRUE(surface_layer_impl->WillDraw(DRAW_MODE_HARDWARE, nullptr));
  }
}

// This test verifies that activation_dependencies and the fallback_surface_id
// are populated correctly if primary and fallback surfaces differ.
TEST(SurfaceLayerImplTest, SurfaceLayerImplWithTwoDifferentSurfaces) {
  LayerTreeImplTestBase impl;
  SurfaceLayerImpl* surface_layer_impl = impl.AddLayer<SurfaceLayerImpl>();

  // Populate the primary viz::SurfaceInfo.
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId1(
      9, base::UnguessableToken::Create());
  viz::SurfaceId surface_id1(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId1);

  // Populate the fallback viz::SurfaceId.
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId2(
      7, kArbitraryLocalSurfaceId1.embed_token());
  viz::SurfaceId surface_id2(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId2);

  gfx::Size layer_size(400, 100);

  // Populate the SurfaceLayerImpl ensuring that the primary and fallback
  // SurfaceInfos are different.
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  surface_layer_impl->SetRange(viz::SurfaceRange(surface_id2, surface_id1), 2u);
  surface_layer_impl->SetBackgroundColor(SK_ColorBLUE);
  CopyProperties(impl.root_layer(), surface_layer_impl);

  gfx::Size viewport_size(1000, 1000);
  impl.CalcDrawProps(viewport_size);

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  {
    AppendQuadsData data;
    surface_layer_impl->AppendQuads(render_pass.get(), &data);
    // The the primary viz::SurfaceInfo will be added to
    // activation_dependencies.
    EXPECT_THAT(data.activation_dependencies,
                UnorderedElementsAre(surface_id1));
    EXPECT_EQ(2u, data.deadline_in_frames);
    EXPECT_FALSE(data.use_default_lower_bound_deadline);
  }

  // Update the fallback to an invalid viz::SurfaceInfo. The
  // |activation_dependencies| should still contain the primary
  // viz::SurfaceInfo.
  {
    AppendQuadsData data;
    surface_layer_impl->SetRange(viz::SurfaceRange(base::nullopt, surface_id1),
                                 0u);
    surface_layer_impl->AppendQuads(render_pass.get(), &data);
    // The primary viz::SurfaceInfo should be added to activation_dependencies.
    EXPECT_THAT(data.activation_dependencies,
                UnorderedElementsAre(surface_id1));
    EXPECT_EQ(0u, data.deadline_in_frames);
    EXPECT_FALSE(data.use_default_lower_bound_deadline);
  }

  // Update the primary deadline and fallback viz::SurfaceId and
  // re-emit DrawQuads.
  {
    AppendQuadsData data;
    surface_layer_impl->SetRange(viz::SurfaceRange(surface_id2, surface_id1),
                                 4u);
    surface_layer_impl->AppendQuads(render_pass.get(), &data);
    // The the primary viz::SurfaceInfo will be added to
    // activation_dependencies.
    EXPECT_THAT(data.activation_dependencies,
                UnorderedElementsAre(surface_id1));
    // The primary SurfaceId hasn't changed but a new deadline was explicitly
    // requested in SetRange so we'll use it in the next CompositorFrame.
    EXPECT_EQ(4u, data.deadline_in_frames);
    EXPECT_FALSE(data.use_default_lower_bound_deadline);
  }

  ASSERT_EQ(3u, render_pass->quad_list.size());
  const viz::SurfaceDrawQuad* surface_draw_quad1 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(0));
  ASSERT_TRUE(surface_draw_quad1);
  const viz::SurfaceDrawQuad* surface_draw_quad2 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(1));
  ASSERT_TRUE(surface_draw_quad2);
  const viz::SurfaceDrawQuad* surface_draw_quad3 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(2));
  ASSERT_TRUE(surface_draw_quad3);

  EXPECT_EQ(surface_id1, surface_draw_quad1->surface_range.end());
  EXPECT_EQ(SK_ColorBLUE, surface_draw_quad1->default_background_color);
  EXPECT_EQ(surface_id2, surface_draw_quad1->surface_range.start());

  EXPECT_EQ(surface_id1, surface_draw_quad2->surface_range.end());
  EXPECT_EQ(SK_ColorBLUE, surface_draw_quad2->default_background_color);
  EXPECT_EQ(base::nullopt, surface_draw_quad2->surface_range.start());

  EXPECT_EQ(surface_id1, surface_draw_quad3->surface_range.end());
  EXPECT_EQ(SK_ColorBLUE, surface_draw_quad3->default_background_color);
  EXPECT_EQ(surface_id2, surface_draw_quad3->surface_range.start());
}

// This test verifies that if one SurfaceLayerImpl has a deadline
// and the other uses the default then AppendQuadsData is populated
// correctly.
TEST(SurfaceLayerImplTest, SurfaceLayerImplsWithDeadlines) {
  LayerTreeImplTestBase impl;
  SurfaceLayerImpl* surface_layer_impl = impl.AddLayer<SurfaceLayerImpl>();
  CopyProperties(impl.root_layer(), surface_layer_impl);

  SurfaceLayerImpl* surface_layer_impl2 = impl.AddLayer<SurfaceLayerImpl>();
  CopyProperties(impl.root_layer(), surface_layer_impl2);

  const viz::LocalSurfaceId kArbitraryLocalSurfaceId1(
      1, base::UnguessableToken::Create());
  viz::SurfaceId surface_id1(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId1);

  const viz::LocalSurfaceId kArbitraryLocalSurfaceId2(
      2, kArbitraryLocalSurfaceId1.embed_token());
  viz::SurfaceId surface_id2(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId2);

  gfx::Size viewport_size(1000, 1000);
  impl.CalcDrawProps(viewport_size);

  gfx::Size layer_size(400, 100);

  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  surface_layer_impl->SetRange(viz::SurfaceRange(surface_id1, surface_id2), 1u);

  surface_layer_impl2->SetBounds(layer_size);
  surface_layer_impl2->SetDrawsContent(true);
  surface_layer_impl2->SetRange(viz::SurfaceRange(surface_id1, surface_id2),
                                base::nullopt);

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  AppendQuadsData data;
  surface_layer_impl->AppendQuads(render_pass.get(), &data);
  EXPECT_EQ(1u, data.deadline_in_frames);
  EXPECT_FALSE(data.use_default_lower_bound_deadline);

  surface_layer_impl2->AppendQuads(render_pass.get(), &data);
  EXPECT_EQ(1u, data.deadline_in_frames);
  EXPECT_TRUE(data.use_default_lower_bound_deadline);
}

// This test verifies that one viz::SurfaceDrawQuad is emitted if a
// SurfaceLayerImpl holds the same surface ID for both the primary
// and fallback viz::SurfaceInfo.
TEST(SurfaceLayerImplTest, SurfaceLayerImplWithMatchingPrimaryAndFallback) {
  LayerTreeImplTestBase impl;
  SurfaceLayerImpl* surface_layer_impl = impl.AddLayer<SurfaceLayerImpl>();

  // Populate the primary viz::SurfaceId.
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId1(
      9, base::UnguessableToken::Create());
  viz::SurfaceId surface_id1(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId1);

  gfx::Size layer_size(400, 100);

  // Populate the SurfaceLayerImpl ensuring that the primary and fallback
  // SurfaceInfos are the same.
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  surface_layer_impl->SetRange(viz::SurfaceRange(surface_id1), 1u);
  surface_layer_impl->SetRange(viz::SurfaceRange(surface_id1), 2u);
  surface_layer_impl->SetBackgroundColor(SK_ColorBLUE);
  CopyProperties(impl.root_layer(), surface_layer_impl);

  gfx::Size viewport_size(1000, 1000);
  impl.CalcDrawProps(viewport_size);

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  AppendQuadsData data;
  surface_layer_impl->AppendQuads(render_pass.get(), &data);
  EXPECT_THAT(data.activation_dependencies, UnorderedElementsAre(surface_id1));
  EXPECT_EQ(2u, data.deadline_in_frames);

  ASSERT_EQ(1u, render_pass->quad_list.size());
  const viz::SurfaceDrawQuad* surface_draw_quad1 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(0));
  ASSERT_TRUE(surface_draw_quad1);

  EXPECT_EQ(surface_id1, surface_draw_quad1->surface_range.end());
  EXPECT_EQ(surface_id1, surface_draw_quad1->surface_range.start());
  EXPECT_EQ(SK_ColorBLUE, surface_draw_quad1->default_background_color);
}

TEST(SurfaceLayerImplTest, GetEnclosingRectInTargetSpace) {
  gfx::Size layer_size(902, 1000);
  gfx::Size viewport_size(902, 1000);
  LayerTreeImplTestBase impl;
  SurfaceLayerImpl* surface_layer_impl = impl.AddLayer<SurfaceLayerImpl>();
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  CopyProperties(impl.root_layer(), surface_layer_impl);

  // A device scale of 1.33 and transform of 1.5 were chosen as they produce
  // different results when rounding at each stage, vs applying a single
  // transform.
  gfx::Transform transform;
  transform.Scale(1.5, 1.5);
  impl.host_impl()->active_tree()->SetDeviceScaleFactor(1.33);
  impl.CalcDrawProps(viewport_size);
  surface_layer_impl->draw_properties().target_space_transform = transform;

  // GetEnclosingRectInTargetSpace() and GetScaledEnclosingRectInTargetSpace()
  // should return the same value, otherwise we may not damage the right
  // pixels.
  EXPECT_EQ(surface_layer_impl->GetScaledEnclosingRectInTargetSpace(1.33),
            surface_layer_impl->GetEnclosingRectInTargetSpace());
}

}  // namespace
}  // namespace cc
