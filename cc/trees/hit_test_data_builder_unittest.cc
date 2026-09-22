// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/hit_test_data_builder.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "base/unguessable_token.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

class HitTestDataBuilderTest : public LayerTreeImplTestBase,
                               public testing::Test {
 protected:
  LayerImpl* SetupDefaultRootLayer(const gfx::Size& viewport_size) {
    LayerImpl* root = root_layer();
    root->SetBounds(viewport_size);
    host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));
    GetClipNode(root)->clip = gfx::RectF(gfx::SizeF(viewport_size));
    return root;
  }

  std::optional<viz::HitTestRegionList> BuildHitTestData() {
    return HitTestDataBuilder(*host_impl()->active_tree()).Build();
  }
};

// Test to ensure that hit test data is created correctly from the active layer
// tree.
TEST_F(HitTestDataBuilderTest, BuildHitTestData) {
  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---intermediate_layer (200, 300), 200x200
  // +-----surface_child1 (50, 50), 100x100, Rotate(45)
  // +---surface_child2 (450, 300), 100x100
  // +---overlapping_layer (500, 350), 200x200
  auto* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* intermediate_layer = AddLayerInActiveTree<LayerImpl>();
  auto* surface_child1 = AddLayerInActiveTree<SurfaceLayerImpl>();
  auto* surface_child2 = AddLayerInActiveTree<SurfaceLayerImpl>();
  auto* overlapping_layer = AddLayerInActiveTree<LayerImpl>();

  intermediate_layer->SetBounds(gfx::Size(200, 200));

  surface_child1->SetBounds(gfx::Size(100, 100));
  gfx::Transform rotate;
  rotate.Rotate(45);
  surface_child1->SetDrawsContent(true);
  surface_child1->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child1->SetSurfaceHitTestable(true);

  surface_child2->SetBounds(gfx::Size(100, 100));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child2->SetSurfaceHitTestable(true);

  overlapping_layer->SetBounds(gfx::Size(200, 200));
  overlapping_layer->SetDrawsContent(true);
  overlapping_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetRange(viz::SurfaceRange(std::nullopt, child_surface_id),
                           std::nullopt);
  surface_child2->SetRange(viz::SurfaceRange(std::nullopt, child_surface_id),
                           std::nullopt);

  CopyProperties(root, intermediate_layer);
  intermediate_layer->SetOffsetToTransformParent(gfx::Vector2dF(200, 300));
  CopyProperties(root, surface_child2);
  surface_child2->SetOffsetToTransformParent(gfx::Vector2dF(450, 300));
  CopyProperties(root, overlapping_layer);
  overlapping_layer->SetOffsetToTransformParent(gfx::Vector2dF(500, 350));

  CopyProperties(intermediate_layer, surface_child1);
  auto& surface_child1_transform_node = CreateTransformNode(surface_child1);
  // The post_translation includes offset of intermediate_layer.
  surface_child1_transform_node.post_translation = gfx::Vector2dF(250, 350);
  surface_child1_transform_node.local = rotate;

  UpdateDrawProperties(host_impl()->active_tree());
  draw_property_utils::ComputeEffects(
      &host_impl()->active_tree()->property_trees()->effect_tree_mutable());

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  std::optional<viz::HitTestRegionList> hit_test_region_list =
      BuildHitTestData();
  ASSERT_TRUE(hit_test_region_list);

  // Since surface_child2 draws in front of surface_child1, it should also be in
  // the front of the hit test region list.
  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(2u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[1].frame_sink_id);
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[1].flags);
  gfx::Transform child1_transform;
  child1_transform.Rotate(-45);
  child1_transform.Translate(-250, -350);
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[1].transform));
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 100, 100)),
            hit_test_region_list->regions[1].rect);

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface |
                   viz::HitTestRegionFlags::kHitTestAsk;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child2_transform;
  child2_transform.Translate(-450, -300);
  EXPECT_TRUE(child2_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 100, 100)),
            hit_test_region_list->regions[0].rect);
}

TEST_F(HitTestDataBuilderTest, PointerEvents) {
  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---surface_child1 (0, 0), 100x100
  // +---overlapping_surface_child2 (50, 50), 100x100, pointer-events: none,
  // does not generate hit test region
  auto* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* surface_child1 = AddLayerInActiveTree<SurfaceLayerImpl>();
  auto* surface_child2 = AddLayerInActiveTree<SurfaceLayerImpl>();

  surface_child1->SetBounds(gfx::Size(100, 100));
  surface_child1->SetDrawsContent(true);
  surface_child1->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child1->SetSurfaceHitTestable(true);
  surface_child1->SetHasPointerEventsNone(false);
  CopyProperties(root, surface_child1);

  surface_child2->SetBounds(gfx::Size(100, 100));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child2->SetSurfaceHitTestable(false);
  surface_child2->SetHasPointerEventsNone(true);
  CopyProperties(root, surface_child2);
  surface_child2->SetOffsetToTransformParent(gfx::Vector2dF(50, 50));

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetRange(viz::SurfaceRange(std::nullopt, child_surface_id),
                           std::nullopt);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  UpdateDrawProperties(host_impl()->active_tree());
  std::optional<viz::HitTestRegionList> hit_test_region_list =
      BuildHitTestData();
  ASSERT_TRUE(hit_test_region_list);

  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  // Since |surface_child2| is not |surface_hit_testable|, it does not
  // contribute to a hit test region. Although it overlaps |surface_child1|, it
  // does not make |surface_child1| kHitTestAsk because it has pointer-events
  // none property.
  EXPECT_EQ(1u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child1_transform;
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 100, 100)),
            hit_test_region_list->regions[0].rect);
}

TEST_F(HitTestDataBuilderTest, ComplexPage) {
  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---surface_child (0, 0), 100x100
  // +---100x non overlapping layers (110, 110), 1x1
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* surface_child = AddLayerInActiveTree<SurfaceLayerImpl>();

  surface_child->SetBounds(gfx::Size(100, 100));
  surface_child->SetDrawsContent(true);
  surface_child->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child->SetSurfaceHitTestable(true);
  surface_child->SetHasPointerEventsNone(false);

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child->SetRange(viz::SurfaceRange(std::nullopt, child_surface_id),
                          std::nullopt);

  CopyProperties(root, surface_child);

  // Create 101 non overlapping layers.
  for (size_t i = 0; i <= 100; ++i) {
    LayerImpl* layer = AddLayerInActiveTree<LayerImpl>();
    layer->SetBounds(gfx::Size(1, 1));
    layer->SetDrawsContent(true);
    layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
    CopyProperties(root, layer);
  }

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  UpdateDrawProperties(host_impl()->active_tree());
  std::optional<viz::HitTestRegionList> hit_test_region_list =
      BuildHitTestData();
  ASSERT_TRUE(hit_test_region_list);

  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  // Since the layer count is greater than 100, in order to save time, we do not
  // check whether each layer overlaps the surface layer, instead, we are being
  // conservative and make the surface layer slow hit testing.
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface |
                   viz::HitTestRegionFlags::kHitTestAsk;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child1_transform;
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 100, 100)),
            hit_test_region_list->regions[0].rect);
}

TEST_F(HitTestDataBuilderTest, InvalidFrameSinkId) {
  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---surface_child1 (0, 0), 100x100
  // +---surface_child2 (0, 0), 50x50, frame_sink_id = (0, 0)
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* surface_child1 = AddLayerInActiveTree<SurfaceLayerImpl>();

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(1024, 768));

  surface_child1->SetBounds(gfx::Size(100, 100));
  surface_child1->SetDrawsContent(true);
  surface_child1->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child1->SetSurfaceHitTestable(true);
  surface_child1->SetHasPointerEventsNone(false);
  CopyProperties(root, surface_child1);

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetRange(viz::SurfaceRange(std::nullopt, child_surface_id),
                           std::nullopt);

  auto* surface_child2 = AddLayerInActiveTree<SurfaceLayerImpl>();

  surface_child2->SetBounds(gfx::Size(50, 50));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child2->SetSurfaceHitTestable(true);
  surface_child2->SetHasPointerEventsNone(false);
  CopyProperties(root, surface_child2);

  surface_child2->SetRange(viz::SurfaceRange(std::nullopt, viz::SurfaceId()),
                           std::nullopt);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  UpdateDrawProperties(host_impl()->active_tree());
  std::optional<viz::HitTestRegionList> hit_test_region_list =
      BuildHitTestData();
  ASSERT_TRUE(hit_test_region_list);

  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  // We do not populate hit test region for a surface layer with invalid frame
  // sink id to avoid deserialization failure. Instead we make the overlapping
  // hit test region kHitTestAsk.
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface |
                   viz::HitTestRegionFlags::kHitTestAsk;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child1_transform;
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 100, 100)),
            hit_test_region_list->regions[0].rect);
}

}  // namespace
}  // namespace cc
