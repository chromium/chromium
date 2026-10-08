// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/hit_test_data_builder.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "cc/base/features.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

// Every emitted child-surface region has these baseline flags.
constexpr uint32_t kChildSurfaceFlags =
    viz::HitTestRegionFlags::kHitTestMouse |
    viz::HitTestRegionFlags::kHitTestTouch |
    viz::HitTestRegionFlags::kHitTestChildSurface;

gfx::MaskFilterInfo CreateTestGradientMaskFilterInfo(
    const gfx::RRectF& rounded_rect) {
  gfx::LinearGradient gradient_mask(45);
  gradient_mask.AddStep(0.0f, 0xff);
  gradient_mask.AddStep(1.0f, 0x00);
  return gfx::MaskFilterInfo(rounded_rect, gradient_mask);
}

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

  SurfaceLayerImpl* AddHitTestableSurfaceLayer(
      const LayerImpl* property_source,
      uint32_t frame_sink_client_id,
      const gfx::Size& bounds = gfx::Size(100, 100)) {
    auto* surface = AddLayerInActiveTree<SurfaceLayerImpl>();
    surface->SetBounds(bounds);
    surface->SetDrawsContent(true);
    surface->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
    surface->SetSurfaceHitTestable(true);
    CopyProperties(property_source, surface);

    viz::LocalSurfaceId local_surface_id(1, base::UnguessableToken::Create());
    viz::SurfaceId surface_id(viz::FrameSinkId(frame_sink_client_id, 1),
                              local_surface_id);
    surface->SetRange(viz::SurfaceRange(std::nullopt, surface_id),
                      std::nullopt);
    return surface;
  }

  LayerImpl* AddHitTestableLayer(
      const LayerImpl* property_source,
      const gfx::Size& bounds,
      const gfx::Vector2dF& offset = gfx::Vector2dF()) {
    LayerImpl* layer = AddLayerInActiveTree<LayerImpl>();
    layer->SetBounds(bounds);
    layer->SetDrawsContent(true);
    layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
    CopyProperties(property_source, layer);
    layer->SetOffsetToTransformParent(offset);
    return layer;
  }

  std::optional<viz::HitTestRegionList> BuildHitTestData() {
    return HitTestDataBuilder(*host_impl()->active_tree()).Build();
  }

  void UpdateDrawPropertiesForHitTestData() {
    UpdateDrawProperties(host_impl()->active_tree());
    draw_property_utils::ComputeEffects(
        &host_impl()->active_tree()->property_trees()->effect_tree_mutable());
  }

  std::optional<viz::HitTestRegionList> BuildHitTestDataAfterUpdate() {
    UpdateDrawPropertiesForHitTestData();
    return BuildHitTestData();
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
  expected_flags = kChildSurfaceFlags;
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
  expected_flags = kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk;
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
  expected_flags = kChildSurfaceFlags;
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
  expected_flags = kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk;
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
  expected_flags = kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child1_transform;
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 100, 100)),
            hit_test_region_list->regions[0].rect);
}

// Even without child surfaces, the list must carry the viewport and latest
// draw transform needed to interpret hit tests in the root coordinate space.
TEST_F(HitTestDataBuilderTest, EmptyRegionListUsesViewportAndDrawTransform) {
  SetupDefaultRootLayer(gfx::Size(400, 300));
  UpdateDrawProperties(host_impl()->active_tree());

  gfx::Transform draw_transform;
  draw_transform.Translate(17, 23);
  draw_transform.Scale(1.5f, 0.75f);
  host_impl()->OnDraw(draw_transform, gfx::Rect(0, 0, 600, 450),
                      /*resourceless_software_draw=*/false,
                      /*skip_draw=*/false);

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  EXPECT_EQ(viz::HitTestRegionFlags::kHitTestMouse |
                viz::HitTestRegionFlags::kHitTestTouch |
                viz::HitTestRegionFlags::kHitTestMine,
            result->flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kNotAsyncHitTest,
            result->async_hit_test_reasons);
  EXPECT_EQ(gfx::Rect(0, 0, 600, 450), result->bounds);
  EXPECT_EQ(draw_transform, result->transform);
  EXPECT_TRUE(result->regions.empty());
}

// Emitting a child region must not change the metadata of the root list.
TEST_F(HitTestDataBuilderTest, NonemptyTreePreservesListMetadata) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  AddHitTestableSurfaceLayer(root, 25);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(viz::HitTestRegionFlags::kHitTestMouse |
                viz::HitTestRegionFlags::kHitTestTouch |
                viz::HitTestRegionFlags::kHitTestMine,
            result->flags);
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300), result->bounds);
}

// Hit-test regions must be ordered front to back.
TEST_F(HitTestDataBuilderTest, EmitsSurfacesFrontToBack) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* first = AddHitTestableSurfaceLayer(root, 30);
  SurfaceLayerImpl* second = AddHitTestableSurfaceLayer(root, 10);
  SurfaceLayerImpl* third = AddHitTestableSurfaceLayer(root, 20);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(3u, result->regions.size());
  EXPECT_EQ(third->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(kChildSurfaceFlags, result->regions[0].flags);
  EXPECT_EQ(second->range().end().frame_sink_id(),
            result->regions[1].frame_sink_id);
  EXPECT_EQ(kChildSurfaceFlags, result->regions[1].flags);
  EXPECT_EQ(first->range().end().frame_sink_id(),
            result->regions[2].frame_sink_id);
  EXPECT_EQ(kChildSurfaceFlags, result->regions[2].flags);
}

// Separate layers embedding the same surface must each emit a region.
TEST_F(HitTestDataBuilderTest, EmitsDuplicateSurfaceIds) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* first = AddHitTestableSurfaceLayer(root, 35);
  SurfaceLayerImpl* second = AddHitTestableSurfaceLayer(root, 36);
  second->SetRange(first->range(), std::nullopt);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(2u, result->regions.size());
  EXPECT_EQ(first->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(first->range().end().frame_sink_id(),
            result->regions[1].frame_sink_id);
}

// Region geometry is expressed in device pixels, while its transform maps
// those pixels back into the child surface's coordinate space.
TEST_F(HitTestDataBuilderTest, AppliesDeviceScaleAndInverseTransform) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  host_impl()->active_tree()->SetDeviceScaleFactor(2.0f);
  SurfaceLayerImpl* surface =
      AddHitTestableSurfaceLayer(root, 15, gfx::Size(101, 79));
  surface->SetOffsetToTransformParent(gfx::Vector2dF(10, 20));

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 202, 158)), result->regions[0].rect);
  gfx::Transform expected_transform;
  expected_transform.Translate(-20, -40);
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(result->regions[0].transform));
}

// A non-invertible screen transform must not put invalid matrix values in the
// hit-test data.
TEST_F(HitTestDataBuilderTest, NonInvertibleTransformUsesIdentity) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 20);
  CreateTransformNode(surface).local = gfx::Transform::MakeScale(0.0f);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_TRUE(result->regions[0].transform.IsIdentity());
}

// Viz hit testing uses a 2D transform, so perspective must be flattened after
// removing device scale and before inversion.
TEST_F(HitTestDataBuilderTest, FlattensThreeDimensionalTransform) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  host_impl()->active_tree()->SetDeviceScaleFactor(1.25f);
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 22);
  gfx::Transform perspective_transform;
  perspective_transform.ApplyPerspectiveDepth(20);
  perspective_transform.RotateAboutYAxis(60);
  CreateTransformNode(surface).local = perspective_transform;

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_FALSE(surface->ScreenSpaceTransform().IsFlat());
  EXPECT_TRUE(result->regions[0].transform.IsFlat());
  gfx::Transform expected_transform = gfx::Transform::MakeScale(2.0f, 1.0f);
  // Flattening removes 3D rotation but retains its 2D perspective.
  expected_transform.set_rc(3, 0, -0.0692820323);
  EXPECT_TRUE(expected_transform.ApproximatelyEqual(
      result->regions[0].transform, 1e-4f));
}

// An ancestor clip can reduce a surface's visible rect without clipping it in
// its own render target. The child region retains its full local bounds because
// hierarchical hit testing applies the ancestor region's clip separately.
TEST_F(HitTestDataBuilderTest, UnclippedSurfaceUsesBounds) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(40, 30));
  SurfaceLayerImpl* surface =
      AddHitTestableSurfaceLayer(root, 24, gfx::Size(100, 80));
  CreateEffectNode(surface).render_surface_reason = RenderSurfaceReason::kTest;

  UpdateDrawPropertiesForHitTestData();
  ASSERT_FALSE(surface->is_clipped());
  ASSERT_EQ(gfx::Rect(40, 30), surface->visible_layer_rect());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags, result->regions[0].flags);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 100, 80)), result->regions[0].rect);
}

// Same hit-test rect as `UnclippedSurfaceUsesBounds`, but overlap also requires
// async hit testing.
TEST_F(HitTestDataBuilderTest, OverlappedUnclippedSurfaceUsesBounds) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(40, 30));
  SurfaceLayerImpl* surface =
      AddHitTestableSurfaceLayer(root, 195, gfx::Size(100, 80));
  CreateEffectNode(surface).render_surface_reason = RenderSurfaceReason::kTest;
  AddHitTestableLayer(root, gfx::Size(20, 20));

  UpdateDrawPropertiesForHitTestData();
  ASSERT_FALSE(surface->is_clipped());
  ASSERT_EQ(gfx::Rect(40, 30), surface->visible_layer_rect());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 100, 80)), result->regions[0].rect);
}

// Rotation alone does not make an unclipped surface require async hit testing.
TEST_F(HitTestDataBuilderTest, RotatedUnclippedSurfaceUsesBounds) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 26);
  TransformNode& transform = CreateTransformNode(surface);
  transform.post_translation = gfx::Vector2dF(100, 100);
  transform.local.Rotate(30);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags, result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kNotAsyncHitTest,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(surface->bounds())),
            result->regions[0].rect);
}

// A simple rectangular clip can narrow the region without requiring renderer
// hit testing, including when device scaling encloses fractional bounds.
TEST_F(HitTestDataBuilderTest, RectangularClipUsesVisibleRect) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  host_impl()->active_tree()->SetDeviceScaleFactor(1.25f);
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 25);
  constexpr gfx::Rect kVisibleRect(7, 11, 53, 31);
  CreateClipNode(surface).clip = gfx::RectF(kVisibleRect);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags, result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kNotAsyncHitTest,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(8, 13, 67, 40)), result->regions[0].rect);
}

// For a clipped surface with a non-axis-aligned transform, visible_layer_rect
// is an axis-aligned bounding box that can include points outside the true
// clipped region. Viz must ask the renderer to resolve those ambiguous hits.
TEST_F(HitTestDataBuilderTest, RotatedClipRequiresAsyncHitTest) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 30);
  CreateTransformNode(surface).local.Rotate(30);
  PropertyTrees* property_trees = host_impl()->active_tree()->property_trees();
  ClipNode& clip = CreateClipNode(property_trees, root->clip_tree_index(),
                                  root->transform_tree_index());
  clip.clip = gfx::RectF(9, 13, 47, 29);
  surface->SetClipTreeIndex(clip.id);

  UpdateDrawPropertiesForHitTestData();
  ASSERT_TRUE(surface->is_clipped());
  ASSERT_NE(gfx::Rect(surface->bounds()), surface->visible_layer_rect());
  ASSERT_FALSE(surface->ScreenSpaceTransform().Preserves2dAxisAlignment());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kIrregularClip,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(surface->visible_layer_rect())),
            result->regions[0].rect);
}

// An effect transform can make a clip non-axis-aligned even when the surface's
// screen transform is axis aligned. Its visible rect then only bounds the true
// clipped region, so Viz must ask the renderer to resolve ambiguous hits.
TEST_F(HitTestDataBuilderTest, NonRectangularEffectClipRequiresAsyncHitTest) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 32);
  PropertyTrees* property_trees = host_impl()->active_tree()->property_trees();
  TransformNode& effect_transform =
      CreateTransformNode(property_trees, root->transform_tree_index());
  effect_transform.local.Rotate(30);
  ClipNode& effect_clip = CreateClipNode(
      property_trees, root->clip_tree_index(), effect_transform.id);
  effect_clip.clip = gfx::RectF(9, 13, 47, 29);
  surface->SetClipTreeIndex(effect_clip.id);
  CreateEffectNode(surface).transform_id = effect_transform.id;

  UpdateDrawPropertiesForHitTestData();
  ASSERT_TRUE(surface->is_clipped());
  ASSERT_NE(gfx::Rect(surface->bounds()), surface->visible_layer_rect());
  ASSERT_TRUE(surface->ScreenSpaceTransform().Preserves2dAxisAlignment());
  ASSERT_FALSE(property_trees->effect_tree().ClippedHitTestRegionIsRectangle(
      surface->effect_tree_index()));

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kIrregularClip,
            result->regions[0].async_hit_test_reasons);
}

// A surface that is both overlapped and non-axis-aligned clipped must report
// both async hit-test reasons.
TEST_F(HitTestDataBuilderTest, CombinesOverlapAndIrregularClipReasons) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 35);

  // The root-space clip becomes non-axis-aligned in the rotated surface's
  // local space, requiring renderer hit testing.
  CreateTransformNode(surface).local.Rotate(30);
  PropertyTrees* property_trees = host_impl()->active_tree()->property_trees();
  ClipNode& clip = CreateClipNode(property_trees, root->clip_tree_index(),
                                  root->transform_tree_index());
  clip.clip = gfx::RectF(9, 13, 47, 29);
  surface->SetClipTreeIndex(clip.id);

  // This later layer is visited first in reverse traversal and contributes
  // geometry that overlaps the surface.
  AddHitTestableLayer(root, gfx::Size(20, 20));

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  // A single async request carries both independently established reasons.
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion |
                viz::AsyncHitTestReasons::kIrregularClip,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(surface->visible_layer_rect())),
            result->regions[0].rect);
}

// A SurfaceRange with different start and end frame sink IDs is valid. Verify
// that the builder emits one child-surface hit-test region and gives it the
// end's frame sink ID rather than the start's.
TEST_F(HitTestDataBuilderTest, UsesEndFrameSinkIdForCrossSinkSurfaceRange) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 43);
  const base::UnguessableToken token = base::UnguessableToken::Create();
  const viz::SurfaceId start(viz::FrameSinkId(103, 1),
                             viz::LocalSurfaceId(1, 1, token));
  const viz::SurfaceId end(viz::FrameSinkId(104, 1),
                           viz::LocalSurfaceId(1, 1, token));
  surface->SetRange(viz::SurfaceRange(start, end), std::nullopt);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(end.frame_sink_id(), result->regions[0].frame_sink_id);
  EXPECT_EQ(kChildSurfaceFlags, result->regions[0].flags);
}

// An invalid range prevents a hit-testable surface from emitting a region.
TEST_F(HitTestDataBuilderTest, InvalidSurfaceRangeDoesNotEmitRegion) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 44);
  surface->SetRange(viz::SurfaceRange(), std::nullopt);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->regions.empty());
}

// A drawing surface with an invalid range cannot emit a child-surface region,
// but its geometry can still overlap a valid surface behind it. Rejecting the
// region must not discard the surface from overlap tracking.
TEST_F(HitTestDataBuilderTest,
       InvalidSurfaceRangeTracksOverlapWithoutEmitting) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* target = AddHitTestableSurfaceLayer(root, 45);
  SurfaceLayerImpl* blocker = AddHitTestableSurfaceLayer(root, 46);
  blocker->SetRange(viz::SurfaceRange(), std::nullopt);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(target->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[0].async_hit_test_reasons);
}

// A surface that neither draws nor accepts hit tests must be excluded from
// both emitted regions and overlap bookkeeping.
TEST_F(HitTestDataBuilderTest, IneligibleSurfaceDoesNotEmitOrTrackOverlap) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* target =
      AddHitTestableSurfaceLayer(root, 50, gfx::Size(20, 20));
  SurfaceLayerImpl* ineligible =
      AddHitTestableSurfaceLayer(root, 51, gfx::Size(20, 20));
  ineligible->SetDrawsContent(false);
  ineligible->SetHitTestOpaqueness(HitTestOpaqueness::kTransparent);
  ineligible->SetSurfaceHitTestable(false);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(target->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(kChildSurfaceFlags, result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kNotAsyncHitTest,
            result->regions[0].async_hit_test_reasons);
}

// A drawing surface omitted from hit-test data still contributes to overlap
// for surfaces behind it.
TEST_F(HitTestDataBuilderTest, OmittedDrawingSurfaceTracksOverlap) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* target =
      AddHitTestableSurfaceLayer(root, 55, gfx::Size(20, 20));
  SurfaceLayerImpl* blocker =
      AddHitTestableSurfaceLayer(root, 56, gfx::Size(20, 20));
  blocker->SetHitTestOpaqueness(HitTestOpaqueness::kTransparent);
  blocker->SetSurfaceHitTestable(false);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(target->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[0].async_hit_test_reasons);
}

// A pointer-events:none child surface remains in Viz hit-test data so its
// subtree can be represented, but the surface itself must be ignored.
TEST_F(HitTestDataBuilderTest, EmittedPointerEventsNoneSurfaceIsIgnored) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 60);
  surface->SetHasPointerEventsNone(true);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestIgnore,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kNotAsyncHitTest,
            result->regions[0].async_hit_test_reasons);
}

// An omitted pointer-events:none surface has no represented subtree, so
// retaining it as an overlap contributor would create a false async request.
TEST_F(HitTestDataBuilderTest,
       OmittedPointerEventsNoneSurfaceDoesNotTrackOverlap) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* target = AddHitTestableSurfaceLayer(root, 62);
  SurfaceLayerImpl* ignored = AddHitTestableSurfaceLayer(root, 63);
  ignored->SetSurfaceHitTestable(false);
  ignored->SetHasPointerEventsNone(true);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(target->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(kChildSurfaceFlags, result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kNotAsyncHitTest,
            result->regions[0].async_hit_test_reasons);
}

// Pointer-events:none also prevents an omitted invalid-range surface from
// contributing to overlap tracking.
TEST_F(HitTestDataBuilderTest,
       InvalidRangePointerEventsNoneSurfaceDoesNotTrackOverlap) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* target = AddHitTestableSurfaceLayer(root, 64);
  SurfaceLayerImpl* ignored = AddHitTestableSurfaceLayer(root, 65);
  ignored->SetHasPointerEventsNone(true);
  ignored->SetRange(viz::SurfaceRange(), std::nullopt);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(target->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(kChildSurfaceFlags, result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kNotAsyncHitTest,
            result->regions[0].async_hit_test_reasons);
}

// An emitted pointer-events:none surface is ignored during hit testing. If it
// is also overlapped, its region must include the async overlap flags.
TEST_F(HitTestDataBuilderTest, PointerEventsNoneCombinesWithOverlap) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 64);
  surface->SetHasPointerEventsNone(true);
  AddHitTestableLayer(root, gfx::Size(20, 20));

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestIgnore |
                viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[0].async_hit_test_reasons);
}

// Non-surface layers must be hit-testable before contributing to overlap
// tracking. The transparent layer is not hit-testable even though it
// geometrically overlaps a surface. It must be ignored. The hit-testable layer
// overlaps the other and must mark it.
TEST_F(HitTestDataBuilderTest, OnlyHitTestableNonSurfaceLayersCauseOverlap) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* unaffected =
      AddHitTestableSurfaceLayer(root, 65, gfx::Size(20, 20));
  SurfaceLayerImpl* overlapped =
      AddHitTestableSurfaceLayer(root, 66, gfx::Size(20, 20));
  overlapped->SetOffsetToTransformParent(gfx::Vector2dF(100, 0));

  LayerImpl* transparent = AddHitTestableLayer(root, gfx::Size(20, 20));
  transparent->SetHitTestOpaqueness(HitTestOpaqueness::kTransparent);
  AddHitTestableLayer(root, gfx::Size(20, 20), gfx::Vector2dF(100, 0));

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(2u, result->regions.size());
  EXPECT_EQ(overlapped->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(unaffected->range().end().frame_sink_id(),
            result->regions[1].frame_sink_id);
  EXPECT_EQ(viz::AsyncHitTestReasons::kNotAsyncHitTest,
            result->regions[1].async_hit_test_reasons);
}

// Overlap contributors must be accumulated using their transformed bounds.
// The blocker's scale expands its bounds from [0, 10) to [0, 20), causing it
// to overlap the target at [15, 20); its untransformed bounds would not.
TEST_F(HitTestDataBuilderTest, UsesTransformedBoundsForOverlapContributors) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* target =
      AddHitTestableSurfaceLayer(root, 70, gfx::Size(5, 5));
  target->SetOffsetToTransformParent(gfx::Vector2dF(15, 0));
  LayerImpl* blocker = AddHitTestableLayer(root, gfx::Size(10, 10));
  CreateTransformNode(blocker).local.Scale(2.0f);

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(target->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[0].async_hit_test_reasons);
}

// Surfaces must be checked for overlap using their transformed bounds. The
// target's scale expands its bounds from [0, 10) to [0, 20), causing it to
// overlap the blocker at [15, 20); its untransformed bounds would not.
TEST_F(HitTestDataBuilderTest, UsesTransformedBoundsForOverlapTargets) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* target =
      AddHitTestableSurfaceLayer(root, 72, gfx::Size(10, 10));
  CreateTransformNode(target).local.Scale(2.0f);
  AddHitTestableLayer(root, gfx::Size(5, 5), gfx::Vector2dF(15, 0));

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[0].async_hit_test_reasons);
}

// Overlap between layers in different render targets must be computed in
// screen space, their common coordinate space. Here the target maps to
// [15, 20) in the root render target. The blocker's target-space bounds are
// [0, 10) in its separate render surface and would appear disjoint, while its
// screen-space bounds are [10, 20) and overlap the target.
TEST_F(HitTestDataBuilderTest,
       UsesScreenTransformForOverlapAcrossRenderSurface) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* target =
      AddHitTestableSurfaceLayer(root, 73, gfx::Size(5, 5));
  target->SetOffsetToTransformParent(gfx::Vector2dF(15, 0));
  LayerImpl* blocker = AddHitTestableLayer(root, gfx::Size(10, 10));
  CreateTransformNode(blocker).post_translation = gfx::Vector2dF(10, 0);
  CreateEffectNode(blocker).render_surface_reason = RenderSurfaceReason::kTest;

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[0].async_hit_test_reasons);
}

// Disjoint overlap regions must stay disjoint in screen space. The second
// contributor is at [10, 20) in its render target but [20, 30) on screen;
// using its target-space bounds or collapsing the regions into a bounding box
// would falsely mark the surface in the [10, 20) gap as overlapped.
TEST_F(HitTestDataBuilderTest, PreservesExactOverlapRegionAcrossTargets) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* first_target =
      AddHitTestableSurfaceLayer(root, 75, gfx::Size(10, 10));
  SurfaceLayerImpl* target_in_hole =
      AddHitTestableSurfaceLayer(root, 76, gfx::Size(10, 10));
  target_in_hole->SetOffsetToTransformParent(gfx::Vector2dF(10, 0));
  LayerImpl* render_surface = AddHitTestableLayer(root, gfx::Size());
  CreateTransformNode(render_surface).post_translation = gfx::Vector2dF(10, 0);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  SurfaceLayerImpl* non_emitted_surface =
      AddHitTestableSurfaceLayer(render_surface, 77, gfx::Size(10, 10));
  non_emitted_surface->SetSurfaceHitTestable(false);
  non_emitted_surface->SetOffsetToTransformParent(gfx::Vector2dF(10, 0));
  AddHitTestableLayer(root, gfx::Size(10, 10));

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(2u, result->regions.size());
  EXPECT_EQ(target_in_hole->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(viz::AsyncHitTestReasons::kNotAsyncHitTest,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(first_target->range().end().frame_sink_id(),
            result->regions[1].frame_sink_id);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[1].async_hit_test_reasons);
}

// The skipped surface's invalid range prevents region emission, but it still
// contributes to overlap tracking. Its 100x100 bounds scale to 200x200 and
// intersect the target at x=150, while its clipped 10x10 visible rect would
// not.
TEST_F(HitTestDataBuilderTest,
       SkippedSurfaceUsesTransformedFullBoundsForOverlap) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* target =
      AddHitTestableSurfaceLayer(root, 80, gfx::Size(10, 10));
  target->SetOffsetToTransformParent(gfx::Vector2dF(150, 0));
  SurfaceLayerImpl* skipped_surface = AddHitTestableSurfaceLayer(root, 81);
  skipped_surface->SetRange(viz::SurfaceRange(), std::nullopt);
  CreateTransformNode(skipped_surface).local.Scale(2.0f);
  CreateClipNode(skipped_surface).clip = gfx::RectF(10, 10);

  UpdateDrawPropertiesForHitTestData();
  ASSERT_TRUE(skipped_surface->is_clipped());
  ASSERT_EQ(gfx::Rect(10, 10), skipped_surface->visible_layer_rect());
  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(target->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[0].async_hit_test_reasons);
}

// A valid child surface is still represented when its geometry is empty.
TEST_F(HitTestDataBuilderTest, EmitsSurfaceWithEmptyBounds) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 85, gfx::Size());

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(surface->range().end().frame_sink_id(),
            result->regions[0].frame_sink_id);
  EXPECT_EQ(kChildSurfaceFlags, result->regions[0].flags);
  EXPECT_EQ(gfx::RRectF(), result->regions[0].rect);
  EXPECT_TRUE(result->regions[0].transform.IsIdentity());
}

struct OverlapThresholdTestCase {
  const char* name;
  size_t hit_testable_layer_count;
  size_t surface_count;
  bool empty_hit_testable_bounds;
  bool expect_assumed_overlap;
};

class HitTestDataBuilderThresholdTest
    : public HitTestDataBuilderTest,
      public testing::WithParamInterface<OverlapThresholdTestCase> {};

// These cases verify both sides of the complexity fallback while ensuring only
// hit-testable non-surface layers contribute to the count.
TEST_P(HitTestDataBuilderThresholdTest,
       AssumesOverlapOnlyAfterNonSurfaceLayerThreshold) {
  const OverlapThresholdTestCase& test_case = GetParam();
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* target = AddHitTestableSurfaceLayer(root, 90);

  for (size_t i = 0; i < test_case.surface_count; ++i) {
    SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(
        root, 100 + static_cast<uint32_t>(i), gfx::Size(10, 10));
    surface->SetOffsetToTransformParent(gfx::Vector2dF(300, 0));
  }
  for (size_t i = 0; i < test_case.hit_testable_layer_count; ++i) {
    AddHitTestableLayer(
        root,
        test_case.empty_hit_testable_bounds ? gfx::Size() : gfx::Size(1, 1),
        gfx::Vector2dF(250, 250));
  }

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u + test_case.surface_count, result->regions.size());
  const viz::HitTestRegion& target_region =
      result->regions[test_case.surface_count];
  EXPECT_EQ(target->range().end().frame_sink_id(), target_region.frame_sink_id);
  EXPECT_EQ(test_case.expect_assumed_overlap
                ? viz::AsyncHitTestReasons::kOverlappedRegion
                : viz::AsyncHitTestReasons::kNotAsyncHitTest,
            target_region.async_hit_test_reasons);
}

std::string OverlapThresholdTestName(
    const testing::TestParamInfo<OverlapThresholdTestCase>& info) {
  return info.param.name;
}

INSTANTIATE_TEST_SUITE_P(
    All,
    HitTestDataBuilderThresholdTest,
    testing::Values(
        OverlapThresholdTestCase{"AtThreshold", 100, 0, false, false},
        OverlapThresholdTestCase{"AboveThreshold", 101, 0, false, true},
        OverlapThresholdTestCase{"SurfacesDoNotCount", 80, 25, false, false},
        OverlapThresholdTestCase{"EmptyLayersCount", 101, 0, true, true}),
    OverlapThresholdTestName);

class HitTestDataBuilderRoundedCornersTest
    : public HitTestDataBuilderTest,
      public testing::WithParamInterface<bool> {
 protected:
  HitTestDataBuilderRoundedCornersTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kVizHitTestRoundedCorners, GetParam());
  }

  void ExpectHitTestRegionForRoundedCorners(const viz::HitTestRegion& region,
                                            const gfx::RRectF& rounded_rect) {
    // TODO(crbug.com/40572334): When rounded-corner hit testing is supported,
    // the expectations will need updates to reflect viz handling the query.
    // This includes verifying the full RRectF and not just the rect portion.
    EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
              region.flags);
    EXPECT_EQ(viz::AsyncHitTestReasons::kIrregularClip,
              region.async_hit_test_reasons);
    EXPECT_EQ(gfx::RRectF(rounded_rect.rect()), region.rect);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Rounded corners can get represented either as a fast-path rounded clip in
// mask_filter_info or as a child kDstIn mask layer when the fast path rejects
// the rounded clip (see `PropertyTreeManager::ShaderBasedRRect()` and
// `EmitClipMaskLayer()`). As we don't run Blink's fast-path selection in these
// tests, we construct the resulting effect-tree representations directly. This
// test verifies that rounded corners represented as a fast-path rounded clip in
// mask_filter_info get the appropriate hit-testing treatment.
TEST_P(HitTestDataBuilderRoundedCornersTest,
       RoundedCornerMaskFilterInfoRequiresAsyncHitTest) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 189);
  const gfx::RRectF rounded_mask(0.0f, 0.0f, 100.0f, 100.0f, 10.0f);
  EffectNode& effect = CreateEffectNode(surface);
  effect.mask_filter_info = gfx::MaskFilterInfo(rounded_mask);
  effect.is_fast_rounded_corner = true;
  const int effect_id = effect.id;

  UpdateDrawPropertiesForHitTestData();
  const EffectNode& updated_effect =
      host_impl()->active_tree()->property_trees()->effect_tree().Node(
          effect_id);
  ASSERT_TRUE(updated_effect.mask_filter_info.HasRoundedCorners());
  ASSERT_FALSE(updated_effect.mask_filter_info.HasGradientMask());
  ASSERT_FALSE(updated_effect.has_masking_child);
  ASSERT_FALSE(surface->is_clipped());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  ExpectHitTestRegionForRoundedCorners(result->regions[0], rounded_mask);
}

// Verify distinct X/Y radii at every corner to ensure they are correctly and
// independently handled.
TEST_P(HitTestDataBuilderRoundedCornersTest, RoundedMaskWithDeviceScaleFactor) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  constexpr float kDeviceScaleFactor = 2.0f;
  host_impl()->active_tree()->SetDeviceScaleFactor(kDeviceScaleFactor);
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 206);
  const gfx::RRectF rounded_mask(0.0f, 0.0f, 100.0f, 100.0f, 1.0f, 2.0f, 3.0f,
                                 4.0f, 5.0f, 6.0f, 7.0f, 8.0f);
  CreateEffectNode(surface).mask_filter_info =
      gfx::MaskFilterInfo(rounded_mask);

  UpdateDrawPropertiesForHitTestData();
  ASSERT_EQ(rounded_mask,
            GetEffectNode(surface)->mask_filter_info.rounded_corner_bounds());
  ASSERT_FALSE(surface->is_clipped());
  ASSERT_EQ(gfx::Rect(surface->bounds()), surface->visible_layer_rect());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  gfx::RRectF expected_scaled_rounded_mask = rounded_mask;
  expected_scaled_rounded_mask.Scale(kDeviceScaleFactor);
  ExpectHitTestRegionForRoundedCorners(result->regions[0],
                                       expected_scaled_rounded_mask);
}

// A layer transform is handled separately from device scale factor. The mask
// and geometry remain in the surface's local coordinate space.
TEST_P(HitTestDataBuilderRoundedCornersTest,
       RoundedMaskWithScaledTransformUsesLocalGeometry) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 207);
  CreateTransformNode(surface).local.Scale(2.0f, 1.5f);
  const gfx::RRectF rounded_mask(0.0f, 0.0f, 100.0f, 100.0f, 10.0f);
  CreateEffectNode(surface).mask_filter_info =
      gfx::MaskFilterInfo(rounded_mask);

  UpdateDrawPropertiesForHitTestData();
  ASSERT_EQ(rounded_mask,
            GetEffectNode(surface)->mask_filter_info.rounded_corner_bounds());
  ASSERT_EQ(surface->transform_tree_index(),
            GetEffectNode(surface)->transform_id);
  ASSERT_FALSE(surface->is_clipped());
  ASSERT_EQ(gfx::Rect(surface->bounds()), surface->visible_layer_rect());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  ExpectHitTestRegionForRoundedCorners(result->regions[0], rounded_mask);
}

// The builder must query rounded corners using the surface's transform ID,
// not the mask's, so a transform-space mismatch still requires fallback.
TEST_P(HitTestDataBuilderRoundedCornersTest,
       RoundedMaskInDifferentTransformSpaceRequiresAsyncHitTest) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 210);
  const gfx::RRectF rounded_mask(0.0f, 0.0f, 100.0f, 100.0f, 10.0f);

  // Keep the mask in the original transform space, then give the surface
  // a distinct identity transform node.
  CreateEffectNode(surface).mask_filter_info =
      gfx::MaskFilterInfo(rounded_mask);
  CreateTransformNode(surface);

  UpdateDrawPropertiesForHitTestData();
  ASSERT_NE(surface->transform_tree_index(),
            GetEffectNode(surface)->transform_id);
  ASSERT_EQ(rounded_mask,
            GetEffectNode(surface)->mask_filter_info.rounded_corner_bounds());
  ASSERT_FALSE(surface->is_clipped());
  ASSERT_EQ(gfx::Rect(surface->bounds()), surface->visible_layer_rect());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kIrregularClip,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(surface->bounds())),
            result->regions[0].rect);
}

// Verify that a rotated, unclipped surface that shares its transform space with
// a rounded mask requires an async hit test if viz rounded corner hit testing
// is disabled, or a synchronous hit test if viz support is enabled. Note that
// it's important to keep the surface inside the viewport so clipping doesn't
// independently require an async hit test.
TEST_P(HitTestDataBuilderRoundedCornersTest, RoundedMaskWithRotatedTransform) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 211);
  TransformNode& transform = CreateTransformNode(surface);
  transform.post_translation = gfx::Vector2dF(100, 100);
  transform.local.Rotate(45);
  const gfx::RRectF rounded_mask(0.0f, 0.0f, 100.0f, 100.0f, 10.0f);
  CreateEffectNode(surface).mask_filter_info =
      gfx::MaskFilterInfo(rounded_mask);

  UpdateDrawPropertiesForHitTestData();
  ASSERT_EQ(rounded_mask,
            GetEffectNode(surface)->mask_filter_info.rounded_corner_bounds());
  ASSERT_EQ(surface->transform_tree_index(),
            GetEffectNode(surface)->transform_id);
  ASSERT_FALSE(surface->ScreenSpaceTransform().Preserves2dAxisAlignment());
  ASSERT_FALSE(surface->is_clipped());
  ASSERT_EQ(gfx::Rect(surface->bounds()), surface->visible_layer_rect());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  ExpectHitTestRegionForRoundedCorners(result->regions[0], rounded_mask);
}

// The builder must add the surface's offset to its local bounds before passing
// them to EffectTree::GetRoundedCornersForHitTest, which expects bounds in
// transform-node space. The generated HitTestRegion::rect must remain local:
//   Surface-local bounds:         (0, 0, 200, 200)
//   Surface offset:               (0, 133)
//   Mask in transform-node space: (0, 133, 200, 200)
// The mask and surface cover the same area but their rectangle origins differ
// because they are expressed in different coordinate spaces.
TEST_P(HitTestDataBuilderRoundedCornersTest,
       RoundedMaskWithOffsetToTransformParent) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 400));
  LayerImpl* rounded_ancestor = AddLayerInActiveTree<LayerImpl>();
  rounded_ancestor->SetBounds(gfx::Size(200, 200));
  CopyProperties(root, rounded_ancestor);
  const gfx::RRectF rounded_mask(0.0f, 133.0f, 200.0f, 200.0f, 12.0f);
  CreateEffectNode(rounded_ancestor).mask_filter_info =
      gfx::MaskFilterInfo(rounded_mask);
  SurfaceLayerImpl* surface =
      AddHitTestableSurfaceLayer(rounded_ancestor, 212, gfx::Size(200, 200));
  surface->SetOffsetToTransformParent(gfx::Vector2dF(0.0f, 133.0f));

  UpdateDrawPropertiesForHitTestData();
  ASSERT_EQ(rounded_ancestor->effect_tree_index(),
            surface->effect_tree_index());
  ASSERT_EQ(rounded_mask,
            GetEffectNode(surface)->mask_filter_info.rounded_corner_bounds());
  ASSERT_EQ(surface->transform_tree_index(),
            GetEffectNode(surface)->transform_id);
  // To verify the behavior of `GetRoundedCornersForHitTest` when viz rounded
  // corner support is enabled, the offset must be non-zero and the mask and
  // bounds rectangles must differ.
  ASSERT_EQ(gfx::Vector2dF(0.0f, 133.0f),
            surface->offset_to_transform_parent());
  ASSERT_NE(rounded_mask.rect(), gfx::RectF(surface->bounds()));
  ASSERT_FALSE(surface->is_clipped());
  ASSERT_EQ(gfx::Rect(surface->bounds()), surface->visible_layer_rect());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  // With the feature enabled, applying the offset when comparing bounds lets
  // this coincident mask use rounded, synchronous hit testing. Omitting the
  // offset would incorrectly force async fallback. With the feature disabled,
  // async fallback is expected.
  ExpectHitTestRegionForRoundedCorners(
      result->regions[0], gfx::RRectF(gfx::RectF(surface->bounds()), 12.0f));
}

// When the fast path rejects a rounded clip, `EmitClipMaskLayer()` converts it
// into a child kDstIn mask layer. Blink also uses kDstIn mask layers for clip
// paths. EffectTree records the child as has_masking_child without describing
// its mask shape, so this region needs async hit testing.
TEST_P(HitTestDataBuilderRoundedCornersTest,
       RoundedCornerDstInMaskingChildRequiresAsyncHitTest) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 190);
  PropertyTrees* property_trees = host_impl()->active_tree()->property_trees();
  EffectNode& mask_isolation = CreateEffectNode(surface);
  mask_isolation.render_surface_reason = RenderSurfaceReason::kRoundedCorner;
  const int mask_isolation_id = mask_isolation.id;
  ASSERT_TRUE(mask_isolation.mask_filter_info.IsEmpty());

  EffectNode& mask_effect =
      CreateEffectNode(property_trees, mask_isolation_id,
                       mask_isolation.transform_id, mask_isolation.clip_id);
  mask_effect.blend_mode = SkBlendMode::kDstIn;

  UpdateDrawPropertiesForHitTestData();
  ASSERT_TRUE(
      property_trees->effect_tree().Node(mask_isolation_id).has_masking_child);
  ASSERT_FALSE(surface->is_clipped());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kIrregularClip,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(surface->bounds())),
            result->regions[0].rect);
}

// Masked geometry must select the visible rect before converting it to device
// pixels. Here the 32x24 visible rect becomes 40x30 instead of using the
// surface's 100x80 bounds.
TEST_P(HitTestDataBuilderRoundedCornersTest,
       MaskedUnclippedSurfaceUsesScaledVisibleRect) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(40, 30));
  host_impl()->active_tree()->SetDeviceScaleFactor(1.25f);
  SurfaceLayerImpl* surface =
      AddHitTestableSurfaceLayer(root, 197, gfx::Size(100, 80));
  EffectNode& effect = CreateEffectNode(surface);
  effect.render_surface_reason = RenderSurfaceReason::kTest;
  effect.mask_filter_info = CreateTestGradientMaskFilterInfo(
      gfx::RRectF(0.0f, 0.0f, 100.0f, 80.0f, 10.0f));

  UpdateDrawPropertiesForHitTestData();
  ASSERT_FALSE(surface->is_clipped());
  ASSERT_EQ(gfx::Rect(32, 24), surface->visible_layer_rect());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kIrregularClip,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 40, 30)), result->regions[0].rect);
}

// Rectangular clips are normally synchronous, but must not clear the
// asynchronous hit testing required by the gradient mask.
TEST_P(HitTestDataBuilderRoundedCornersTest,
       RectangularClipWithMaskRequiresAsyncHitTest) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 198);
  constexpr gfx::Rect kVisibleRect(7, 11, 53, 31);
  CreateClipNode(surface).clip = gfx::RectF(kVisibleRect);
  CreateEffectNode(surface).mask_filter_info = CreateTestGradientMaskFilterInfo(
      gfx::RRectF(0.0f, 0.0f, 100.0f, 100.0f, 10.0f));

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kIrregularClip,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(kVisibleRect)), result->regions[0].rect);
}

// Gradient alpha cannot be represented by rounded-corner hit-test geometry.
TEST_P(HitTestDataBuilderRoundedCornersTest, GradientMaskRequiresAsyncHitTest) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 200);
  CreateEffectNode(surface).mask_filter_info = CreateTestGradientMaskFilterInfo(
      gfx::RRectF(0.0f, 0.0f, 100.0f, 100.0f, 10.0f));

  std::optional<viz::HitTestRegionList> result = BuildHitTestDataAfterUpdate();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kIrregularClip,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(surface->bounds())),
            result->regions[0].rect);
}

// An overlapped surface needs kHitTestAsk, but without a mask or a clip in its
// render target, HitTestRegion::rect must retain its full local bounds rather
// than the smaller, offset visible rect. This test guards against extending the
// rounded-mask fallback's visible-rect selection to every async region.
TEST_P(HitTestDataBuilderRoundedCornersTest,
       OverlappedUnclippedSurfaceUsesBounds) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(53, 31));
  SurfaceLayerImpl* surface =
      AddHitTestableSurfaceLayer(root, 195, gfx::Size(100, 80));
  CreateTransformNode(surface).local.Translate(-7.0f, -11.0f);
  CreateEffectNode(surface).render_surface_reason = RenderSurfaceReason::kTest;
  AddHitTestableLayer(root, gfx::Size(20, 20));

  UpdateDrawPropertiesForHitTestData();
  ASSERT_FALSE(surface->is_clipped());
  ASSERT_EQ(gfx::Rect(7, 11, 53, 31), surface->visible_layer_rect());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(0, 0, 100, 80)), result->regions[0].rect);
}

// Overlap must retain kHitTestAsk even when the rounded mask is representable.
// Unlike `OverlappedUnclippedSurfaceUsesBounds`, this masked surface must use
// its visible rect, including its nonzero origin, for HitTestRegion::rect. The
// builder must discard any returned radii so rounded geometry cannot reject a
// point before async hit testing resolves it.
TEST_P(HitTestDataBuilderRoundedCornersTest,
       OverlappedRoundedMaskUsesVisibleRectWithoutCorners) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(53, 31));
  SurfaceLayerImpl* surface =
      AddHitTestableSurfaceLayer(root, 185, gfx::Size(100, 80));
  CreateTransformNode(surface).local.Translate(-7.0f, -11.0f);
  EffectNode& effect = CreateEffectNode(surface);
  effect.render_surface_reason = RenderSurfaceReason::kTest;
  effect.mask_filter_info =
      gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(surface->bounds()), 10.0f));
  AddHitTestableLayer(root, gfx::Size(20, 20));

  UpdateDrawPropertiesForHitTestData();
  ASSERT_FALSE(surface->is_clipped());
  constexpr gfx::Rect kVisibleRect(7, 11, 53, 31);
  ASSERT_EQ(kVisibleRect, surface->visible_layer_rect());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  // TODO(crbug.com/40572334): When rounded-corner hit testing is supported,
  // the expectations will need updates to reflect viz ability to handle the
  // rounded corner portion of the query.
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kOverlappedRegion |
                viz::AsyncHitTestReasons::kIrregularClip,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(kVisibleRect)), result->regions[0].rect);
}

// The rectangular clip cuts through the rounded mask's bottom corners. This
// exposes a limitation in rounded-rectangle representation, as provided to viz,
// where a rounded corner is defined by the rect corners + the radii for that
// corner. If we change the rect's corners, then we move the rounded corner
// center which changes the overall clipping geometry. This test verifies that
// this scenario requires async testing with the current implementation.
TEST_P(HitTestDataBuilderRoundedCornersTest,
       ClippedRoundedMaskWithMismatchedBoundsRequiresAsyncHitTest) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(400, 300));
  SurfaceLayerImpl* surface = AddHitTestableSurfaceLayer(root, 189);
  constexpr gfx::Rect kVisibleRect(0, 0, 100, 95);
  CreateClipNode(surface).clip = gfx::RectF(kVisibleRect);
  CreateEffectNode(surface).mask_filter_info =
      gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(surface->bounds()), 10.0f));

  UpdateDrawPropertiesForHitTestData();
  ASSERT_TRUE(surface->is_clipped());

  std::optional<viz::HitTestRegionList> result = BuildHitTestData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->regions.size());
  EXPECT_EQ(kChildSurfaceFlags | viz::HitTestRegionFlags::kHitTestAsk,
            result->regions[0].flags);
  EXPECT_EQ(viz::AsyncHitTestReasons::kIrregularClip,
            result->regions[0].async_hit_test_reasons);
  EXPECT_EQ(gfx::RRectF(gfx::RectF(kVisibleRect)), result->regions[0].rect);
}

std::string RoundedCornersFeatureTestName(
    const testing::TestParamInfo<bool>& info) {
  return info.param ? "Enabled" : "Disabled";
}

INSTANTIATE_TEST_SUITE_P(All,
                         HitTestDataBuilderRoundedCornersTest,
                         testing::Bool(),
                         RoundedCornersFeatureTestName);

}  // namespace
}  // namespace cc
