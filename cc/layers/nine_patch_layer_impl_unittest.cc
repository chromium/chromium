// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "cc/layers/append_quads_data.h"
#include "cc/layers/nine_patch_layer_impl.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_ui_resource_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

gfx::Rect ToRoundedIntRect(const gfx::RectF& rect_f) {
  return gfx::Rect(gfx::ToRoundedInt(rect_f.x()),
                   gfx::ToRoundedInt(rect_f.y()),
                   gfx::ToRoundedInt(rect_f.width()),
                   gfx::ToRoundedInt(rect_f.height()));
}

void NinePatchLayerLayoutTest(const gfx::Size& bitmap_size,
                              const gfx::Rect& aperture_rect,
                              const gfx::Size& layer_size,
                              const gfx::Rect& border,
                              bool fill_center,
                              size_t expected_quad_size) {
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  gfx::Rect visible_layer_rect(layer_size);
  gfx::Rect expected_remaining(border.x(), border.y(),
                               layer_size.width() - border.width(),
                               layer_size.height() - border.height());

  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink =
      FakeLayerTreeFrameSink::Create3d();
  FakeUIResourceLayerTreeHostImpl host_impl(&task_runner_provider,
                                            &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeFrameSink(layer_tree_frame_sink.get());

  std::unique_ptr<NinePatchLayerImpl> layer =
      NinePatchLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_layer_rect = visible_layer_rect;
  layer->SetBounds(layer_size);
  SetupRootProperties(layer.get());

  UIResourceId uid = 1;
  bool is_opaque = false;
  UIResourceBitmap bitmap(bitmap_size, is_opaque);

  host_impl.CreateUIResource(uid, bitmap);
  layer->SetUIResourceId(uid);
  layer->SetImageBounds(bitmap_size);
  layer->SetLayout(aperture_rect, border, gfx::Rect(), fill_center, false);
  host_impl.active_tree()->SetRootLayerForTesting(std::move(layer));
  UpdateDrawProperties(host_impl.active_tree());

  AppendQuadsData data;
  host_impl.active_tree()->root_layer()->AppendQuads(render_pass.get(), &data);

  // Verify quad rects
  const auto& quads = render_pass->quad_list;
  EXPECT_EQ(expected_quad_size, quads.size());

  Region layer_remaining(visible_layer_rect);
  for (auto iter = quads.cbegin(); iter != quads.cend(); ++iter) {
    gfx::Rect quad_rect = iter->rect;

    EXPECT_TRUE(visible_layer_rect.Contains(quad_rect)) << iter.index();
    EXPECT_TRUE(layer_remaining.Contains(quad_rect)) << iter.index();
    EXPECT_EQ(iter->needs_blending,
              !iter->shared_quad_state->are_contents_opaque);
    layer_remaining.Subtract(Region(quad_rect));
  }

  // Check if the left-over quad is the same size as the mapped aperture quad in
  // layer space.
  if (!fill_center) {
    EXPECT_EQ(expected_remaining, layer_remaining.bounds());
  } else {
    EXPECT_TRUE(layer_remaining.bounds().IsEmpty());
  }

  // Verify UV rects
  gfx::Rect bitmap_rect(bitmap_size);
  Region tex_remaining(bitmap_rect);
  for (auto* quad : quads) {
    const viz::TextureDrawQuad* tex_quad =
        viz::TextureDrawQuad::MaterialCast(quad);
    gfx::RectF tex_rect =
        gfx::BoundingRect(tex_quad->uv_top_left, tex_quad->uv_bottom_right);
    tex_rect.Scale(bitmap_size.width(), bitmap_size.height());
    tex_remaining.Subtract(Region(ToRoundedIntRect(tex_rect)));
  }

  if (!fill_center) {
    EXPECT_EQ(aperture_rect, tex_remaining.bounds());
    Region aperture_region(aperture_rect);
    EXPECT_EQ(aperture_region, tex_remaining);
  } else {
    EXPECT_TRUE(layer_remaining.bounds().IsEmpty());
  }

  host_impl.DeleteUIResource(uid);
}

void NinePatchLayerLayoutTestWithOcclusion(const gfx::Size& bitmap_size,
                                           const gfx::Rect& aperture_rect,
                                           const gfx::Size& layer_size,
                                           const gfx::Rect& border,
                                           const gfx::Rect& occlusion,
                                           bool fill_center,
                                           size_t expected_quad_size) {
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  gfx::Rect visible_layer_rect(layer_size);
  int border_left = std::min(border.x(), occlusion.x()),
      border_top = std::min(border.y(), occlusion.y()),
      border_right = std::min(border.width() - border.x(),
                              layer_size.width() - occlusion.right()),
      border_bottom = std::min(border.height() - border.y(),
                               layer_size.height() - occlusion.bottom());
  gfx::Rect expected_layer_remaining(
      border_left, border_top, layer_size.width() - border_left - border_right,
      layer_size.height() - border_top - border_bottom);
  float ratio_left = border_left == 0 ? 0 : (aperture_rect.x() / border.x()),
        ratio_top = border_top == 0 ? 0 : (aperture_rect.y() / border.y()),
        ratio_right = border_right == 0
                          ? 0
                          : ((bitmap_size.width() - aperture_rect.right()) /
                             (border.width() - border.x())),
        ratio_bottom = border_bottom == 0
                           ? 0
                           : ((bitmap_size.height() - aperture_rect.bottom()) /
                              (border.height() - border.y()));
  int image_remaining_left = border_left * ratio_left,
      image_remaining_top = border_top * ratio_top,
      image_remaining_right = border_right * ratio_right,
      image_remaining_bottom = border_bottom * ratio_bottom;
  gfx::Rect expected_tex_remaining(
      image_remaining_left, image_remaining_top,
      bitmap_size.width() - image_remaining_right - image_remaining_left,
      bitmap_size.height() - image_remaining_bottom - image_remaining_top);

  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink =
      FakeLayerTreeFrameSink::Create3d();
  FakeUIResourceLayerTreeHostImpl host_impl(&task_runner_provider,
                                            &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeFrameSink(layer_tree_frame_sink.get());

  std::unique_ptr<NinePatchLayerImpl> layer =
      NinePatchLayerImpl::Create(host_impl.active_tree(), 1);
  layer->draw_properties().visible_layer_rect = visible_layer_rect;
  layer->SetBounds(layer_size);
  SetupRootProperties(layer.get());

  UIResourceId uid = 1;
  bool is_opaque = false;
  UIResourceBitmap bitmap(bitmap_size, is_opaque);

  host_impl.CreateUIResource(uid, bitmap);
  layer->SetUIResourceId(uid);
  layer->SetImageBounds(bitmap_size);
  layer->SetLayout(aperture_rect, border, occlusion, false, false);
  host_impl.active_tree()->SetRootLayerForTesting(std::move(layer));
  UpdateDrawProperties(host_impl.active_tree());

  AppendQuadsData data;
  host_impl.active_tree()->root_layer()->AppendQuads(render_pass.get(), &data);

  // Verify quad rects
  const auto& quads = render_pass->quad_list;
  EXPECT_EQ(expected_quad_size, quads.size());

  Region layer_remaining(visible_layer_rect);
  for (auto iter = quads.cbegin(); iter != quads.cend(); ++iter) {
    gfx::Rect quad_rect = iter->rect;

    EXPECT_TRUE(visible_layer_rect.Contains(quad_rect)) << iter.index();
    EXPECT_TRUE(layer_remaining.Contains(quad_rect)) << iter.index();
    EXPECT_EQ(iter->needs_blending,
              !iter->shared_quad_state->are_contents_opaque);
    layer_remaining.Subtract(Region(quad_rect));
  }

  // Check if the left-over quad is the same size as the mapped aperture quad in
  // layer space.
  EXPECT_EQ(expected_layer_remaining, layer_remaining.bounds());

  // Verify UV rects
  gfx::Rect bitmap_rect(bitmap_size);
  Region tex_remaining(bitmap_rect);
  for (auto* quad : quads) {
    const viz::TextureDrawQuad* tex_quad =
        viz::TextureDrawQuad::MaterialCast(quad);
    gfx::RectF tex_rect =
        gfx::BoundingRect(tex_quad->uv_top_left, tex_quad->uv_bottom_right);
    tex_rect.Scale(bitmap_size.width(), bitmap_size.height());
    tex_remaining.Subtract(Region(ToRoundedIntRect(tex_rect)));
  }

  EXPECT_EQ(expected_tex_remaining, tex_remaining.bounds());
  Region aperture_region(expected_tex_remaining);
  EXPECT_EQ(aperture_region, tex_remaining);

  host_impl.DeleteUIResource(uid);
}

TEST(NinePatchLayerImplTest, VerifyDrawQuads) {
  // Input is a 100x100 bitmap with a 40x50 aperture at x=20, y=30.
  // The bounds of the layer are set to 400x400.
  gfx::Size bitmap_size(100, 100);
  gfx::Size layer_size(400, 500);
  gfx::Rect aperture_rect(20, 30, 40, 50);
  gfx::Rect border(40, 40, 80, 80);
  bool fill_center = false;
  size_t expected_quad_size = 8;
  NinePatchLayerLayoutTest(bitmap_size, aperture_rect, layer_size, border,
                           fill_center, expected_quad_size);

  // The bounds of the layer are set to less than the bitmap size.
  bitmap_size = gfx::Size(100, 100);
  layer_size = gfx::Size(40, 50);
  aperture_rect = gfx::Rect(20, 30, 40, 50);
  border = gfx::Rect(10, 10, 25, 15);
  fill_center = true;
  expected_quad_size = 9;
  NinePatchLayerLayoutTest(bitmap_size, aperture_rect, layer_size, border,
                           fill_center, expected_quad_size);

  // Layer and image sizes are equal.
  bitmap_size = gfx::Size(100, 100);
  layer_size = gfx::Size(100, 100);
  aperture_rect = gfx::Rect(20, 30, 40, 50);
  border = gfx::Rect(20, 30, 40, 50);
  fill_center = true;
  expected_quad_size = 9;
  NinePatchLayerLayoutTest(bitmap_size, aperture_rect, layer_size, border,
                           fill_center, expected_quad_size);
}

TEST(NinePatchLayerImplTest, VerifyDrawQuadsWithOcclusion) {
  // Occlusion removed part of the border and leaves us with 12 patches.
  gfx::Size bitmap_size(100, 100);
  gfx::Rect aperture_rect(30, 30, 40, 40);
  gfx::Size layer_size(400, 400);
  gfx::Rect occlusion(20, 20, 360, 360);
  gfx::Rect border(30, 30, 60, 60);
  size_t expected_quad_size = 12;
  NinePatchLayerLayoutTestWithOcclusion(bitmap_size, aperture_rect, layer_size,
                                        border, occlusion, false,
                                        expected_quad_size);

  bitmap_size = gfx::Size(100, 100);
  aperture_rect = gfx::Rect(20, 30, 60, 40);
  layer_size = gfx::Size(400, 400);
  occlusion = gfx::Rect(10, 10, 380, 380);
  border = gfx::Rect(20, 30, 40, 60);
  expected_quad_size = 12;
  NinePatchLayerLayoutTestWithOcclusion(bitmap_size, aperture_rect, layer_size,
                                        border, occlusion, false,
                                        expected_quad_size);

  // All borders are empty, so nothing should be drawn.
  bitmap_size = gfx::Size(100, 100);
  aperture_rect = gfx::Rect(0, 0, 100, 100);
  layer_size = gfx::Size(400, 400);
  occlusion = gfx::Rect(0, 0, 400, 400);
  border = gfx::Rect(0, 0, 0, 0);
  expected_quad_size = 0;
  NinePatchLayerLayoutTestWithOcclusion(bitmap_size, aperture_rect, layer_size,
                                        border, occlusion, false,
                                        expected_quad_size);

  // Right border is empty, we should have no quads on the right side.
  bitmap_size = gfx::Size(100, 100);
  aperture_rect = gfx::Rect(20, 30, 80, 40);
  layer_size = gfx::Size(400, 400);
  occlusion = gfx::Rect(10, 10, 390, 380);
  border = gfx::Rect(20, 30, 20, 60);
  expected_quad_size = 7;
  NinePatchLayerLayoutTestWithOcclusion(bitmap_size, aperture_rect, layer_size,
                                        border, occlusion, false,
                                        expected_quad_size);
}

TEST(NinePatchLayerImplTest, VerifyDrawQuadsWithEmptyPatches) {
  // The top component of the 9-patch is empty, so there should be no quads for
  // the top three components.
  gfx::Size bitmap_size(100, 100);
  gfx::Size layer_size(100, 100);
  gfx::Rect aperture_rect(10, 0, 80, 90);
  gfx::Rect border(10, 0, 20, 10);
  bool fill_center = false;
  size_t expected_quad_size = 5;
  NinePatchLayerLayoutTest(bitmap_size, aperture_rect, layer_size, border,
                           fill_center, expected_quad_size);

  // The top and left components of the 9-patch are empty, so there should be no
  // quads for the left and top components.
  bitmap_size = gfx::Size(100, 100);
  layer_size = gfx::Size(100, 100);
  aperture_rect = gfx::Rect(0, 0, 90, 90);
  border = gfx::Rect(0, 0, 10, 10);
  fill_center = false;
  expected_quad_size = 3;
  NinePatchLayerLayoutTest(bitmap_size, aperture_rect, layer_size, border,
                           fill_center, expected_quad_size);

  // The aperture is the size of the bitmap and the center doesn't draw.
  bitmap_size = gfx::Size(100, 100);
  layer_size = gfx::Size(100, 100);
  aperture_rect = gfx::Rect(0, 0, 100, 100);
  border = gfx::Rect(0, 0, 0, 0);
  fill_center = false;
  expected_quad_size = 0;
  NinePatchLayerLayoutTest(bitmap_size, aperture_rect, layer_size, border,
                           fill_center, expected_quad_size);

  // The aperture is the size of the bitmap and the center does draw.
  bitmap_size = gfx::Size(100, 100);
  layer_size = gfx::Size(100, 100);
  aperture_rect = gfx::Rect(0, 0, 100, 100);
  border = gfx::Rect(0, 0, 0, 0);
  fill_center = true;
  expected_quad_size = 1;
  NinePatchLayerLayoutTest(bitmap_size, aperture_rect, layer_size, border,
                           fill_center, expected_quad_size);
}

TEST(NinePatchLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTreeImplTestBase impl;

  SkBitmap sk_bitmap;
  sk_bitmap.allocN32Pixels(10, 10);
  sk_bitmap.setImmutable();
  UIResourceId uid = 5;
  UIResourceBitmap bitmap(sk_bitmap);
  impl.host_impl()->CreateUIResource(uid, bitmap);

  NinePatchLayerImpl* nine_patch_layer_impl =
      impl.AddLayer<NinePatchLayerImpl>();
  nine_patch_layer_impl->SetBounds(layer_size);
  nine_patch_layer_impl->SetDrawsContent(true);
  nine_patch_layer_impl->SetUIResourceId(uid);
  nine_patch_layer_impl->SetImageBounds(gfx::Size(10, 10));
  CopyProperties(impl.root_layer(), nine_patch_layer_impl);

  gfx::Rect aperture = gfx::Rect(3, 3, 4, 4);
  gfx::Rect border = gfx::Rect(300, 300, 400, 400);
  nine_patch_layer_impl->SetLayout(aperture, border, gfx::Rect(), true, false);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(nine_patch_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect(layer_size));
    EXPECT_EQ(9u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(nine_patch_layer_impl->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(nine_patch_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(0, 0, 500, 1000);
    impl.AppendQuadsWithOcclusion(nine_patch_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    VerifyQuadsAreOccluded(impl.quad_list(), occluded,
                           &partially_occluded_count);
    // The layer outputs nine quads, three of which are partially occluded, and
    // three fully occluded.
    EXPECT_EQ(6u, impl.quad_list().size());
    EXPECT_EQ(3u, partially_occluded_count);
  }
}

TEST(NinePatchLayerImplTest, OpaqueRect) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTreeImplTestBase impl;

  SkBitmap sk_bitmap_opaque;
  sk_bitmap_opaque.allocN32Pixels(10, 10);
  sk_bitmap_opaque.setImmutable();
  sk_bitmap_opaque.setAlphaType(kOpaque_SkAlphaType);

  UIResourceId uid_opaque = 6;
  UIResourceBitmap bitmap_opaque(sk_bitmap_opaque);
  impl.host_impl()->CreateUIResource(uid_opaque, bitmap_opaque);

  SkBitmap sk_bitmap_alpha;
  sk_bitmap_alpha.allocN32Pixels(10, 10);
  sk_bitmap_alpha.setImmutable();
  sk_bitmap_alpha.setAlphaType(kUnpremul_SkAlphaType);

  UIResourceId uid_alpha = 7;
  UIResourceBitmap bitmap_alpha(sk_bitmap_alpha);

  impl.host_impl()->CreateUIResource(uid_alpha, bitmap_alpha);

  NinePatchLayerImpl* nine_patch_layer_impl =
      impl.AddLayer<NinePatchLayerImpl>();
  nine_patch_layer_impl->SetBounds(layer_size);
  nine_patch_layer_impl->SetDrawsContent(true);
  CopyProperties(impl.root_layer(), nine_patch_layer_impl);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("Use opaque image");

    nine_patch_layer_impl->SetUIResourceId(uid_opaque);
    nine_patch_layer_impl->SetImageBounds(gfx::Size(10, 10));

    gfx::Rect aperture = gfx::Rect(3, 3, 4, 4);
    gfx::Rect border = gfx::Rect(300, 300, 400, 400);
    nine_patch_layer_impl->SetLayout(aperture, border, gfx::Rect(), true,
                                     false);

    impl.AppendQuadsWithOcclusion(nine_patch_layer_impl, gfx::Rect());

    const auto& quad_list = impl.quad_list();
    for (auto it = quad_list.BackToFrontBegin();
         it != quad_list.BackToFrontEnd(); ++it)
      EXPECT_FALSE(it->ShouldDrawWithBlending());
  }

  {
    SCOPED_TRACE("Use tranparent image");

    nine_patch_layer_impl->SetUIResourceId(uid_alpha);

    impl.AppendQuadsWithOcclusion(nine_patch_layer_impl, gfx::Rect());

    const auto& quad_list = impl.quad_list();
    for (auto it = quad_list.BackToFrontBegin();
         it != quad_list.BackToFrontEnd(); ++it)
      EXPECT_TRUE(it->ShouldDrawWithBlending());
  }
}

}  // namespace
}  // namespace cc
