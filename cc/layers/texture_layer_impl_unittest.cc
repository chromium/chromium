// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer_impl.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void IgnoreCallback(const gpu::SyncToken& sync_token, bool lost) {}

TEST(TextureLayerImplTest, VisibleOpaqueRegion) {
  const gfx::Size layer_bounds(100, 100);
  const gfx::Rect layer_rect(layer_bounds);
  const Region layer_region(layer_rect);

  LayerTreeImplTestBase impl;

  auto resource = viz::TransferableResource::MakeGpu(
      gpu::Mailbox::Generate(), GL_TEXTURE_2D,
      gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                     gpu::CommandBufferId::FromUnsafeValue(0x234), 0x456),
      layer_bounds, viz::SinglePlaneFormat::kRGBA_8888,
      false /* is_overlay_candidate */);

  TextureLayerImpl* layer = impl.AddLayerInActiveTree<TextureLayerImpl>();
  layer->SetBounds(layer_bounds);
  layer->draw_properties().visible_layer_rect = layer_rect;
  layer->SetBlendBackgroundColor(true);
  layer->SetTransferableResource(resource, base::BindOnce(&IgnoreCallback));
  CopyProperties(impl.root_layer(), layer);

  // Verify initial conditions.
  EXPECT_FALSE(layer->contents_opaque());
  EXPECT_EQ(SkColors::kTransparent, layer->background_color());
  EXPECT_EQ(Region().ToString(), layer->VisibleOpaqueRegion().ToString());

  // Opaque background.
  layer->SetBackgroundColor(SkColors::kWhite);
  EXPECT_EQ(layer_region.ToString(), layer->VisibleOpaqueRegion().ToString());

  // Transparent background.
  layer->SetBackgroundColor({1.0f, 1.0f, 1.0f, 0.5f});
  EXPECT_EQ(Region().ToString(), layer->VisibleOpaqueRegion().ToString());
}

TEST(TextureLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTreeImplTestBase impl;

  auto resource = viz::TransferableResource::MakeGpu(
      gpu::Mailbox::Generate(), GL_TEXTURE_2D,
      gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                     gpu::CommandBufferId::FromUnsafeValue(0x234), 0x456),
      layer_size, viz::SinglePlaneFormat::kRGBA_8888,
      false /* is_overlay_candidate */);

  TextureLayerImpl* texture_layer_impl =
      impl.AddLayerInActiveTree<TextureLayerImpl>();
  texture_layer_impl->SetBounds(layer_size);
  texture_layer_impl->SetDrawsContent(true);
  texture_layer_impl->SetTransferableResource(resource,
                                              base::BindOnce(&IgnoreCallback));
  CopyProperties(impl.root_layer(), texture_layer_impl);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(texture_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(texture_layer_impl->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(texture_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendQuadsWithOcclusion(texture_layer_impl, occluded);

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
