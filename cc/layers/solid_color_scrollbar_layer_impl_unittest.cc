// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_scrollbar_layer_impl.h"

#include <stddef.h>

#include "cc/test/layer_tree_impl_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(SolidColorScrollbarLayerImplTest, Occlusion) {
  gfx::Size layer_size(10, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTreeImplTestBase impl;

  ScrollbarOrientation orientation = ScrollbarOrientation::kVertical;
  int thumb_thickness = layer_size.width();
  int track_start = 0;
  bool is_left_side_vertical_scrollbar = false;

  SolidColorScrollbarLayerImpl* scrollbar_layer_impl =
      impl.AddLayerInActiveTree<SolidColorScrollbarLayerImpl>(
          orientation, thumb_thickness, track_start,
          is_left_side_vertical_scrollbar);
  scrollbar_layer_impl->SetBounds(layer_size);
  scrollbar_layer_impl->SetDrawsContent(true);
  scrollbar_layer_impl->SetCurrentPos(25.f);
  scrollbar_layer_impl->SetClipLayerLength(100.f);
  scrollbar_layer_impl->SetScrollLayerLength(200.f);
  // SolidColorScrollbarLayers construct with opacity = 0.f, so override.
  CopyProperties(impl.root_layer(), scrollbar_layer_impl);
  CreateEffectNode(scrollbar_layer_impl).opacity = 1.f;

  impl.CalcDrawProps(viewport_size);

  gfx::Rect thumb_rect = scrollbar_layer_impl->ComputeThumbQuadRect();
  EXPECT_EQ(gfx::Rect(0, 500 / 4, 10, layer_size.height() / 2).ToString(),
            thumb_rect.ToString());

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(scrollbar_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), thumb_rect);
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(scrollbar_layer_impl->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(scrollbar_layer_impl, occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(0, 0, 5, 1000);
    impl.AppendQuadsWithOcclusion(scrollbar_layer_impl, occluded);

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
