// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include "base/numerics/ranges.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

std::pair<gfx::PointF, gfx::PointF> GetVisibleSelectionEndPoints(
    const gfx::RectF& rect,
    const gfx::PointF& top,
    const gfx::PointF& bottom) {
  gfx::PointF start(base::ClampToRange(top.x(), rect.x(), rect.right()),
                    base::ClampToRange(top.y(), rect.y(), rect.bottom()));
  gfx::PointF end =
      start + gfx::Vector2dF(bottom.x() - top.x(), bottom.y() - top.y());
  return {start, end};
}

class LayerTreeImplTest : public LayerTreeImplTestBase, public testing::Test {
 public:
  LayerTreeImplTest() = default;
  explicit LayerTreeImplTest(const LayerTreeSettings& settings)
      : LayerTreeImplTestBase(settings) {}

  void SetUp() override {
    root_layer()->SetBounds(gfx::Size(100, 100));
    UpdateDrawProperties(host_impl().active_tree());
  }

  FakeLayerTreeHostImpl& host_impl() const {
    return *LayerTreeImplTestBase::host_impl();
  }

  const RenderSurfaceList& GetRenderSurfaceList() const {
    return host_impl().active_tree()->GetRenderSurfaceList();
  }

  LayerImpl* HitTestSimpleTree(int top_sorting_context,
                               int left_child_sorting_context,
                               int right_child_sorting_context,
                               float top_depth,
                               float left_child_depth,
                               float right_child_depth) {
    top_ = AddLayer<LayerImpl>();
    left_child_ = AddLayer<LayerImpl>();
    right_child_ = AddLayer<LayerImpl>();

    gfx::Size bounds(100, 100);
    {
      gfx::Transform translate_z;
      translate_z.Translate3d(0, 0, top_depth);
      top_->SetBounds(bounds);
      top_->SetDrawsContent(true);
      top_->SetHitTestable(true);

      CopyProperties(root_layer(), top_);
      auto& transform_node = CreateTransformNode(top_);
      transform_node.local = translate_z;
      transform_node.sorting_context_id = top_sorting_context;
    }
    {
      gfx::Transform translate_z;
      translate_z.Translate3d(0, 0, left_child_depth);
      left_child_->SetBounds(bounds);
      left_child_->SetDrawsContent(true);
      left_child_->SetHitTestable(true);

      CopyProperties(top_, left_child_);
      auto& transform_node = CreateTransformNode(left_child_);
      transform_node.local = translate_z;
      transform_node.sorting_context_id = left_child_sorting_context;
      transform_node.flattens_inherited_transform = false;
    }
    {
      gfx::Transform translate_z;
      translate_z.Translate3d(0, 0, right_child_depth);
      right_child_->SetBounds(bounds);
      right_child_->SetDrawsContent(true);
      right_child_->SetHitTestable(true);

      CopyProperties(top_, right_child_);
      auto& transform_node = CreateTransformNode(right_child_);
      transform_node.local = translate_z;
      transform_node.sorting_context_id = right_child_sorting_context;
    }

    root_layer()->SetBounds(top_->bounds());
    host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(top_->bounds()));

    UpdateDrawProperties(host_impl().active_tree());
    CHECK_EQ(1u, GetRenderSurfaceList().size());

    gfx::PointF test_point = gfx::PointF(1.f, 1.f);
    LayerImpl* result_layer =
        host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);

    CHECK(result_layer);
    return result_layer;
  }

  // These layers are created by HitTestSimpleTree().
  LayerImpl* top_ = nullptr;
  LayerImpl* left_child_ = nullptr;
  LayerImpl* right_child_ = nullptr;
};

TEST_F(LayerTreeImplTest, HitTestingForSingleLayer) {
  gfx::Size bounds(100, 100);
  LayerImpl* root = root_layer();
  root->SetBounds(bounds);
  root->SetDrawsContent(true);
  root->SetHitTestable(true);

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(root_layer())->num_contributors());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::PointF test_point(101.f, 101.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::PointF(1.f, 1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(root, result_layer);

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(root, result_layer);
}

TEST_F(LayerTreeImplTest, UpdateViewportAndHitTest) {
  // Ensures that the viewport rect is correctly updated by the clip tree.
  gfx::Size bounds(100, 100);
  LayerImpl* root = root_layer();
  root->SetBounds(bounds);
  root->SetDrawsContent(true);
  root->SetHitTestable(true);

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());
  EXPECT_EQ(
      gfx::RectF(gfx::SizeF(bounds)),
      host_impl().active_tree()->property_trees()->clip_tree.ViewportClip());
  EXPECT_EQ(gfx::Rect(bounds), root->visible_layer_rect());

  gfx::Size new_bounds(50, 50);
  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(new_bounds));
  gfx::PointF test_point(51.f, 51.f);
  host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(
      gfx::RectF(gfx::SizeF(new_bounds)),
      host_impl().active_tree()->property_trees()->clip_tree.ViewportClip());
  EXPECT_EQ(gfx::Rect(new_bounds), root->visible_layer_rect());
}

TEST_F(LayerTreeImplTest, HitTestingForSingleLayerAndHud) {
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  root->SetHitTestable(true);

  // Create hud and add it as a child of root.
  auto* hud = AddLayer<HeadsUpDisplayLayerImpl>();
  hud->SetBounds(gfx::Size(200, 200));
  hud->SetDrawsContent(true);
  hud->SetHitTestable(true);

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(hud->bounds()));
  host_impl().active_tree()->set_hud_layer(hud);
  CopyProperties(root, hud);
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(2, GetRenderSurface(root_layer())->num_contributors());

  // Hit testing for a point inside HUD, but outside root should return null
  gfx::PointF test_point(101.f, 101.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer, never the HUD
  // layer.
  test_point = gfx::PointF(1.f, 1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(root, result_layer);

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(root, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForUninvertibleTransform) {
  gfx::Transform uninvertible_transform;
  uninvertible_transform.matrix().set(0, 0, 0.0);
  uninvertible_transform.matrix().set(1, 1, 0.0);
  uninvertible_transform.matrix().set(2, 2, 0.0);
  uninvertible_transform.matrix().set(3, 3, 0.0);
  ASSERT_FALSE(uninvertible_transform.IsInvertible());

  LayerImpl* root = root_layer();

  LayerImpl* layer = AddLayer<LayerImpl>();
  layer->SetBounds(gfx::Size(100, 100));
  layer->SetDrawsContent(true);
  layer->SetHitTestable(true);
  root->SetBounds(layer->bounds());
  CopyProperties(root, layer);
  CreateTransformNode(layer).local = uninvertible_transform;

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());
  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_FALSE(layer->ScreenSpaceTransform().IsInvertible());

  // Hit testing any point should not hit the layer. If the invertible matrix is
  // accidentally ignored and treated like an identity, then the hit testing
  // will incorrectly hit the layer when it shouldn't.
  gfx::PointF test_point(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(10.f, 10.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(10.f, 30.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(50.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(67.f, 48.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForSinglePositionedLayer) {
  // This layer is positioned, and hit testing should correctly know where the
  // layer is located.
  LayerImpl* test_layer = AddLayer<LayerImpl>();
  test_layer->SetBounds(gfx::Size(100, 100));
  test_layer->SetDrawsContent(true);
  test_layer->SetHitTestable(true);
  CopyProperties(root_layer(), test_layer);
  test_layer->SetOffsetToTransformParent(gfx::Vector2dF(50.f, 50.f));

  host_impl().active_tree()->SetDeviceViewportRect(
      gfx::Rect(test_layer->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(test_layer)->num_contributors());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::PointF test_point(49.f, 49.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Even though the layer exists at (101, 101), it should not be visible there
  // since the root render surface would clamp it.
  test_point = gfx::PointF(101.f, 101.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::PointF(51.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(test_layer, result_layer);

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(test_layer, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForSingleRotatedLayer) {
  LayerImpl* root = root_layer();

  gfx::Transform rotation45_degrees_about_center;
  rotation45_degrees_about_center.Translate(50.0, 50.0);
  rotation45_degrees_about_center.RotateAboutZAxis(45.0);
  rotation45_degrees_about_center.Translate(-50.0, -50.0);

  LayerImpl* layer = AddLayer<LayerImpl>();
  layer->SetBounds(gfx::Size(100, 100));
  layer->SetDrawsContent(true);
  layer->SetHitTestable(true);
  root->SetBounds(layer->bounds());
  CopyProperties(root, layer);
  CreateTransformNode(layer).local = rotation45_degrees_about_center;

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(root_layer())->num_contributors());

  // Hit testing for points outside the layer.
  // These corners would have been inside the un-transformed layer, but they
  // should not hit the correctly transformed layer.
  gfx::PointF test_point(99.f, 99.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(1.f, 1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::PointF(1.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(layer, result_layer);

  // Hit testing the corners that would overlap the unclipped layer, but are
  // outside the clipped region.
  test_point = gfx::PointF(50.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingClipNodeDifferentTransformAndTargetIds) {
  // Tests hit testing on a layer whose clip node has different transform and
  // target id.
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(500, 500));

  gfx::Transform translation;
  translation.Translate(100, 100);
  LayerImpl* render_surface = AddLayer<LayerImpl>();
  render_surface->SetBounds(gfx::Size(100, 100));
  CopyProperties(root, render_surface);
  CreateTransformNode(render_surface).local = translation;
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;

  gfx::Transform scale_matrix;
  scale_matrix.Scale(2, 2);
  LayerImpl* scale = AddLayer<LayerImpl>();
  scale->SetBounds(gfx::Size(50, 50));
  CopyProperties(render_surface, scale);
  CreateTransformNode(scale).local = scale_matrix;

  LayerImpl* clip = AddLayer<LayerImpl>();
  clip->SetBounds(gfx::Size(25, 25));
  clip->SetMasksToBounds(true);
  CopyProperties(scale, clip);
  CreateClipNode(clip);

  LayerImpl* test = AddLayer<LayerImpl>();
  test->SetBounds(gfx::Size(100, 100));
  test->SetDrawsContent(true);
  test->SetHitTestable(true);
  CopyProperties(clip, test);

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  gfx::PointF test_point(160.f, 160.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(140.f, 140.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(test, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingSiblings) {
  // This tests hit testing when the test point hits only one of the siblings.
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* child1 = AddLayer<LayerImpl>();
  child1->SetBounds(gfx::Size(25, 25));
  child1->SetMasksToBounds(true);
  child1->SetDrawsContent(true);
  child1->SetHitTestable(true);
  CopyProperties(root, child1);
  CreateClipNode(child1);

  LayerImpl* child2 = AddLayer<LayerImpl>();
  child2->SetBounds(gfx::Size(75, 75));
  child2->SetMasksToBounds(true);
  child2->SetDrawsContent(true);
  child2->SetHitTestable(true);
  CopyProperties(root, child2);
  CreateClipNode(child2);

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  gfx::PointF test_point(50.f, 50.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(child2, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForSinglePerspectiveLayer) {
  LayerImpl* root = root_layer();

  // perspective_projection_about_center * translation_by_z is designed so
  // that the 100 x 100 layer becomes 50 x 50, and remains centered at (50,
  // 50).
  gfx::Transform perspective_projection_about_center;
  perspective_projection_about_center.Translate(50.0, 50.0);
  perspective_projection_about_center.ApplyPerspectiveDepth(1.0);
  perspective_projection_about_center.Translate(-50.0, -50.0);
  gfx::Transform translation_by_z;
  translation_by_z.Translate3d(0.0, 0.0, -1.0);

  LayerImpl* layer = AddLayer<LayerImpl>();
  layer->SetBounds(gfx::Size(100, 100));
  layer->SetDrawsContent(true);
  layer->SetHitTestable(true);
  root->SetBounds(layer->bounds());
  CopyProperties(root, layer);
  CreateTransformNode(layer).local =
      (perspective_projection_about_center * translation_by_z);

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(root_layer())->num_contributors());

  // Hit testing for points outside the layer.
  // These corners would have been inside the un-transformed layer, but they
  // should not hit the correctly transformed layer.
  gfx::PointF test_point(24.f, 24.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(76.f, 76.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::PointF(26.f, 26.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(layer, result_layer);

  test_point = gfx::PointF(74.f, 74.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(layer, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForSimpleClippedLayer) {
  // Test that hit-testing will only work for the visible portion of a layer,
  // and not the entire layer bounds. Here we just test the simple axis-aligned
  // case.
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* clipping_layer = AddLayer<LayerImpl>();
  // this layer is positioned, and hit testing should correctly know where the
  // layer is located.
  clipping_layer->SetBounds(gfx::Size(50, 50));
  clipping_layer->SetMasksToBounds(true);
  CopyProperties(root, clipping_layer);
  clipping_layer->SetOffsetToTransformParent(gfx::Vector2dF(25.f, 25.f));
  CreateClipNode(clipping_layer);

  LayerImpl* child = AddLayer<LayerImpl>();
  child->SetBounds(gfx::Size(300, 300));
  child->SetDrawsContent(true);
  child->SetHitTestable(true);
  CopyProperties(clipping_layer, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(-50.f, -50.f));

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(root_layer())->num_contributors());
  EXPECT_TRUE(child->contributes_to_drawn_render_surface());

  // Hit testing for a point outside the layer should return a null pointer.
  // Despite the child layer being very large, it should be clipped to the root
  // layer's bounds.
  gfx::PointF test_point(24.f, 24.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Even though the layer exists at (101, 101), it should not be visible there
  // since the clipping_layer would clamp it.
  test_point = gfx::PointF(76.f, 76.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the child layer.
  test_point = gfx::PointF(26.f, 26.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child, result_layer);

  test_point = gfx::PointF(74.f, 74.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForMultiClippedRotatedLayer) {
  // This test checks whether hit testing correctly avoids hit testing with
  // multiple ancestors that clip in non axis-aligned ways. To pass this test,
  // the hit testing algorithm needs to recognize that multiple parent layers
  // may clip the layer, and should not actually hit those clipped areas.
  //
  // The child and grand_child layers are both initialized to clip the
  // rotated_leaf. The child layer is rotated about the top-left corner, so that
  // the root + child clips combined create a triangle. The rotated_leaf will
  // only be visible where it overlaps this triangle.
  //
  LayerImpl* root = root_layer();

  root->SetBounds(gfx::Size(100, 100));
  root->SetMasksToBounds(true);
  CreateClipNode(root);

  // Visible rects computed by combinig clips in target space and root space
  // don't match because of rotation transforms. So, we skip
  // verify_visible_rect_calculations.
  LayerImpl* child = AddLayer<LayerImpl>();
  LayerImpl* grand_child = AddLayer<LayerImpl>();
  LayerImpl* rotated_leaf = AddLayer<LayerImpl>();

  child->SetBounds(gfx::Size(80, 80));
  child->SetMasksToBounds(true);
  CopyProperties(root, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(10.f, 10.f));
  CreateClipNode(child);

  gfx::Transform rotation45_degrees_about_corner;
  rotation45_degrees_about_corner.RotateAboutZAxis(45.0);

  // This is positioned with respect to its parent which is already at
  // position (10, 10).
  // The size is to ensure it covers at least sqrt(2) * 100.
  grand_child->SetBounds(gfx::Size(200, 200));
  grand_child->SetMasksToBounds(true);
  CopyProperties(child, grand_child);
  CreateTransformNode(grand_child).local = rotation45_degrees_about_corner;
  CreateClipNode(grand_child);

  // Rotates about the center of the layer
  gfx::Transform rotated_leaf_transform;
  rotated_leaf_transform.Translate(
      -10.0, -10.0);  // cancel out the grand_parent's position
  rotated_leaf_transform.RotateAboutZAxis(
      -45.0);  // cancel out the corner 45-degree rotation of the parent.
  rotated_leaf_transform.Translate(50.0, 50.0);
  rotated_leaf_transform.RotateAboutZAxis(45.0);
  rotated_leaf_transform.Translate(-50.0, -50.0);
  rotated_leaf->SetBounds(gfx::Size(100, 100));
  rotated_leaf->SetDrawsContent(true);
  rotated_leaf->SetHitTestable(true);
  CopyProperties(grand_child, rotated_leaf);
  CreateTransformNode(rotated_leaf).local = rotated_leaf_transform;

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());
  // (11, 89) is close to the the bottom left corner within the clip, but it is
  // not inside the layer.
  gfx::PointF test_point(11.f, 89.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Closer inwards from the bottom left will overlap the layer.
  test_point = gfx::PointF(25.f, 75.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(rotated_leaf, result_layer);

  // (4, 50) is inside the unclipped layer, but that corner of the layer should
  // be clipped away by the grandparent and should not get hit. If hit testing
  // blindly uses visible content rect without considering how parent may clip
  // the layer, then hit testing would accidentally think that the point
  // successfully hits the layer.
  test_point = gfx::PointF(4.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // (11, 50) is inside the layer and within the clipped area.
  test_point = gfx::PointF(11.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(rotated_leaf, result_layer);

  // Around the middle, just to the right and up, would have hit the layer
  // except that that area should be clipped away by the parent.
  test_point = gfx::PointF(51.f, 49.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Around the middle, just to the left and down, should successfully hit the
  // layer.
  test_point = gfx::PointF(49.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(rotated_leaf, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForNonClippingIntermediateLayer) {
  // This test checks that hit testing code does not accidentally clip to layer
  // bounds for a layer that actually does not clip.

  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* intermediate_layer = AddLayer<LayerImpl>();
  intermediate_layer->SetBounds(gfx::Size(50, 50));
  // Sanity check the intermediate layer should not clip.
  ASSERT_FALSE(intermediate_layer->masks_to_bounds());
  CopyProperties(root, intermediate_layer);
  // this layer is positioned, and hit testing should correctly know where the
  // layer is located.
  intermediate_layer->SetOffsetToTransformParent(gfx::Vector2dF(10.f, 10.f));

  // The child of the intermediate_layer is translated so that it does not
  // overlap intermediate_layer at all.  If child is incorrectly clipped, we
  // would not be able to hit it successfully.
  LayerImpl* child = AddLayer<LayerImpl>();
  child->SetBounds(gfx::Size(20, 20));
  child->SetDrawsContent(true);
  child->SetHitTestable(true);
  CopyProperties(intermediate_layer, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(70.f, 70.f));

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(root_layer())->num_contributors());
  EXPECT_TRUE(child->contributes_to_drawn_render_surface());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::PointF test_point(69.f, 69.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(91.f, 91.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the child layer.
  test_point = gfx::PointF(71.f, 71.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child, result_layer);

  test_point = gfx::PointF(89.f, 89.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForMultipleLayers) {
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  root->SetHitTestable(true);

  // child1 and child2 are initialized to overlap between x=50 and x=60.
  // grand_child is set to overlap both child1 and child2 between y=50 and
  // y=60.  The expected stacking order is: (front) child2, (second)
  // grand_child, (third) child1, and (back) the root layer behind all other
  // layers.

  LayerImpl* child1 = AddLayer<LayerImpl>();
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child1->SetHitTestable(true);
  CopyProperties(root, child1);
  child1->SetOffsetToTransformParent(gfx::Vector2dF(10.f, 10.f));

  // Remember that grand_child is positioned with respect to its parent (i.e.
  // child1).  In screen space, the intended position is (10, 50), with size
  // 100 x 50.
  LayerImpl* grand_child1 = AddLayer<LayerImpl>();
  grand_child1->SetBounds(gfx::Size(100, 50));
  grand_child1->SetDrawsContent(true);
  grand_child1->SetHitTestable(true);
  CopyProperties(child1, grand_child1);
  grand_child1->SetOffsetToTransformParent(
      gfx::Vector2dF(0.f, 40.f) + child1->offset_to_transform_parent());

  LayerImpl* child2 = AddLayer<LayerImpl>();
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child2->SetHitTestable(true);
  CopyProperties(root, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(50.f, 10.f));

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(grand_child1);
  ASSERT_EQ(1u, GetRenderSurfaceList().size());

  RenderSurfaceImpl* root_render_surface = GetRenderSurface(root);
  ASSERT_EQ(4, root_render_surface->num_contributors());
  EXPECT_TRUE(root_layer()->contributes_to_drawn_render_surface());
  EXPECT_TRUE(child1->contributes_to_drawn_render_surface());
  EXPECT_TRUE(child2->contributes_to_drawn_render_surface());
  EXPECT_TRUE(grand_child1->contributes_to_drawn_render_surface());

  // Nothing overlaps the root at (1, 1), so hit testing there should find
  // the root layer.
  gfx::PointF test_point = gfx::PointF(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(root, result_layer);

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::PointF(15.f, 15.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child1, result_layer);

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top.
  test_point = gfx::PointF(51.f, 20.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child2, result_layer);

  // At (80, 51), child2 and grand_child1 overlap. child2 is expected to be on
  // top.
  test_point = gfx::PointF(80.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child2, result_layer);

  // At (51, 51), all layers overlap each other. child2 is expected to be on top
  // of all other layers.
  test_point = gfx::PointF(51.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child2, result_layer);

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top.
  test_point = gfx::PointF(20.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(grand_child1, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingSameSortingContextTied) {
  LayerImpl* hit_layer = HitTestSimpleTree(/* sorting_contexts */ 10, 10, 10,
                                           /* depths */ 0, 0, 0);
  // 3 is the last in tree order, and so should be on top.
  EXPECT_EQ(right_child_, hit_layer);
}

TEST_F(LayerTreeImplTest, HitTestingSameSortingContextChildWins) {
  LayerImpl* hit_layer = HitTestSimpleTree(/* sorting_contexts */ 10, 10, 10,
                                           /* depths */ 0, 1, 0);
  EXPECT_EQ(left_child_, hit_layer);
}

TEST_F(LayerTreeImplTest, HitTestingWithoutSortingContext) {
  LayerImpl* hit_layer = HitTestSimpleTree(/* sorting_contexts */ 0, 0, 0,
                                           /* depths */ 0, 1, 0);
  EXPECT_EQ(right_child_, hit_layer);
}

TEST_F(LayerTreeImplTest, HitTestingDistinctSortingContext) {
  LayerImpl* hit_layer = HitTestSimpleTree(/* sorting_contexts */ 10, 11, 12,
                                           /* depths */ 0, 1, 0);
  EXPECT_EQ(right_child_, hit_layer);
}

TEST_F(LayerTreeImplTest, HitTestingSameSortingContextParentWins) {
  LayerImpl* hit_layer = HitTestSimpleTree(/* sorting_contexts */ 10, 10, 10,
                                           /* depths */ 0, -1, -1);
  EXPECT_EQ(top_, hit_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForMultipleLayersAtVaryingDepths) {
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  root->SetHitTestable(true);
  GetTransformNode(root)->flattens_inherited_transform = false;
  GetTransformNode(root)->sorting_context_id = 1;

  // child 1 and child2 are initialized to overlap between x=50 and x=60.
  // grand_child is set to overlap both child1 and child2 between y=50 and
  // y=60.  The expected stacking order is: (front) child2, (second)
  // grand_child, (third) child1, and (back) the root layer behind all other
  // layers.

  LayerImpl* child1 = AddLayer<LayerImpl>();
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child1->SetHitTestable(true);
  CopyProperties(root, child1);
  auto& child1_transform_node = CreateTransformNode(child1);
  child1_transform_node.post_translation = gfx::Vector2dF(10.f, 10.f);
  child1_transform_node.flattens_inherited_transform = false;
  child1_transform_node.sorting_context_id = 1;

  // Remember that grand_child is positioned with respect to its parent (i.e.
  // child1).  In screen space, the intended position is (10, 50), with size
  // 100 x 50.
  LayerImpl* grand_child1 = AddLayer<LayerImpl>();
  grand_child1->SetBounds(gfx::Size(100, 50));
  grand_child1->SetDrawsContent(true);
  grand_child1->SetHitTestable(true);
  CopyProperties(child1, grand_child1);
  auto& grand_child1_transform_node = CreateTransformNode(grand_child1);
  grand_child1_transform_node.post_translation = gfx::Vector2dF(0.f, 40.f);
  grand_child1_transform_node.flattens_inherited_transform = false;

  LayerImpl* child2 = AddLayer<LayerImpl>();
  child2->SetBounds(gfx::Size(50, 50));
  gfx::Transform translate_z;
  translate_z.Translate3d(0, 0, 10.f);
  child2->SetDrawsContent(true);
  child2->SetHitTestable(true);
  CopyProperties(root, child2);
  auto& child2_transform_node = CreateTransformNode(child2);
  child2_transform_node.local = translate_z;
  child2_transform_node.post_translation = gfx::Vector2dF(50.f, 10.f);
  child2_transform_node.flattens_inherited_transform = false;
  child2_transform_node.sorting_context_id = 1;

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(grand_child1);
  ASSERT_EQ(1u, GetRenderSurfaceList().size());

  // Nothing overlaps the root_layer at (1, 1), so hit testing there should find
  // the root layer.
  gfx::PointF test_point = gfx::PointF(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(root, result_layer);

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::PointF(15.f, 15.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child1, result_layer);

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top,
  // as it was transformed to the foreground.
  test_point = gfx::PointF(51.f, 20.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child2, result_layer);

  // At (80, 51), child2 and grand_child1 overlap. child2 is expected to
  // be on top, as it was transformed to the foreground.
  test_point = gfx::PointF(80.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child2, result_layer);

  // At (51, 51), child1, child2 and grand_child1 overlap. child2 is expected to
  // be on top, as it was transformed to the foreground.
  test_point = gfx::PointF(51.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child2, result_layer);

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top, as it descends from child1.
  test_point = gfx::PointF(20.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(grand_child1, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingRespectsClipParents) {
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  root->SetHitTestable(true);

  LayerImpl* child = AddLayer<LayerImpl>();
  child->SetBounds(gfx::Size(1, 1));
  child->SetDrawsContent(true);
  child->SetHitTestable(true);
  CopyProperties(root, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(10.f, 10.f));
  CreateClipNode(child);

  LayerImpl* scroll_child = AddLayer<LayerImpl>();
  scroll_child->SetBounds(gfx::Size(200, 200));
  scroll_child->SetDrawsContent(true);
  scroll_child->SetHitTestable(true);
  CopyProperties(root, scroll_child);
  scroll_child->SetClipTreeIndex(child->clip_tree_index());

  LayerImpl* grand_child = AddLayer<LayerImpl>();
  grand_child->SetBounds(gfx::Size(200, 200));
  grand_child->SetDrawsContent(true);
  grand_child->SetHitTestable(true);
  CopyProperties(scroll_child, grand_child);
  CreateEffectNode(grand_child).render_surface_reason =
      RenderSurfaceReason::kTest;

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  gfx::PointF test_point(12.f, 52.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  // The |test_point| should have been clipped away by |child|, so the only
  // thing that should be hit is |root|.
  EXPECT_EQ(root, result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingForMultipleLayerLists) {
  //
  // The geometry is set up similarly to the previous case, but
  // all layers are forced to be render surfaces now.
  //
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  root->SetHitTestable(true);

  // child 1 and child2 are initialized to overlap between x=50 and x=60.
  // grand_child is set to overlap both child1 and child2 between y=50 and
  // y=60.  The expected stacking order is: (front) child2, (second)
  // grand_child, (third) child1, and (back) the root layer behind all other
  // layers.

  LayerImpl* child1 = AddLayer<LayerImpl>();
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child1->SetHitTestable(true);
  CopyProperties(root, child1);
  CreateTransformNode(child1).post_translation = gfx::Vector2dF(10.f, 10.f);
  CreateEffectNode(child1).render_surface_reason = RenderSurfaceReason::kTest;

  // Remember that grand_child is positioned with respect to its parent (i.e.
  // child1).  In screen space, the intended position is (10, 50), with size
  // 100 x 50.
  LayerImpl* grand_child1 = AddLayer<LayerImpl>();
  grand_child1->SetBounds(gfx::Size(100, 50));
  grand_child1->SetDrawsContent(true);
  grand_child1->SetHitTestable(true);
  CopyProperties(child1, grand_child1);
  CreateTransformNode(grand_child1).post_translation =
      gfx::Vector2dF(0.f, 40.f);
  CreateEffectNode(grand_child1).render_surface_reason =
      RenderSurfaceReason::kTest;

  LayerImpl* child2 = AddLayer<LayerImpl>();
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child2->SetHitTestable(true);
  CopyProperties(root, child2);
  CreateTransformNode(child2).post_translation = gfx::Vector2dF(50.f, 10.f);
  CreateEffectNode(child2).render_surface_reason = RenderSurfaceReason::kTest;

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(grand_child1);
  ASSERT_TRUE(GetRenderSurface(child1));
  ASSERT_TRUE(GetRenderSurface(child2));
  ASSERT_TRUE(GetRenderSurface(grand_child1));
  ASSERT_EQ(4u, GetRenderSurfaceList().size());
  // The root surface has the root layer, and child1's and child2's render
  // surfaces.
  ASSERT_EQ(3, GetRenderSurface(root)->num_contributors());
  // The child1 surface has the child1 layer and grand_child1's render surface.
  ASSERT_EQ(2, GetRenderSurface(child1)->num_contributors());
  ASSERT_EQ(1, GetRenderSurface(child2)->num_contributors());
  ASSERT_EQ(1, GetRenderSurface(grand_child1)->num_contributors());
  EXPECT_TRUE(root_layer()->contributes_to_drawn_render_surface());
  EXPECT_TRUE(child1->contributes_to_drawn_render_surface());
  EXPECT_TRUE(grand_child1->contributes_to_drawn_render_surface());
  EXPECT_TRUE(child2->contributes_to_drawn_render_surface());

  // Nothing overlaps the root at (1, 1), so hit testing there should find
  // the root layer.
  gfx::PointF test_point(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(root, result_layer);

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::PointF(15.f, 15.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child1, result_layer);

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top.
  test_point = gfx::PointF(51.f, 20.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child2, result_layer);

  // At (80, 51), child2 and grand_child1 overlap. child2 is expected to be on
  // top.
  test_point = gfx::PointF(80.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child2, result_layer);

  // At (51, 51), all layers overlap each other. child2 is expected to be on top
  // of all other layers.
  test_point = gfx::PointF(51.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(child2, result_layer);

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top.
  test_point = gfx::PointF(20.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);
  EXPECT_EQ(grand_child1, result_layer);
}

TEST_F(LayerTreeImplTest, HitCheckingTouchHandlerRegionsForSingleLayer) {
  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(10, 10, 50, 50));

  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  root->SetHitTestable(true);

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(root)->num_contributors());

  // Hit checking for any point should return a null pointer for a layer without
  // any touch event handler regions.
  gfx::PointF test_point(11.f, 11.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  root->SetTouchActionRegion(touch_action_region);
  // Hit checking for a point outside the layer should return a null pointer.
  test_point = gfx::PointF(101.f, 101.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::PointF(1.f, 1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::PointF(11.f, 11.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(root, result_layer);

  test_point = gfx::PointF(59.f, 59.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(root, result_layer);
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForUninvertibleTransform) {
  LayerImpl* root = root_layer();

  gfx::Transform uninvertible_transform;
  uninvertible_transform.matrix().set(0, 0, 0.0);
  uninvertible_transform.matrix().set(1, 1, 0.0);
  uninvertible_transform.matrix().set(2, 2, 0.0);
  uninvertible_transform.matrix().set(3, 3, 0.0);
  ASSERT_FALSE(uninvertible_transform.IsInvertible());

  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(10, 10, 50, 50));

  LayerImpl* layer = AddLayer<LayerImpl>();
  layer->SetBounds(gfx::Size(100, 100));
  layer->SetDrawsContent(true);
  layer->SetHitTestable(true);
  layer->SetTouchActionRegion(touch_action_region);
  root->SetBounds(layer->bounds());
  CopyProperties(root, layer);
  CreateTransformNode(layer).local = uninvertible_transform;

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_FALSE(layer->ScreenSpaceTransform().IsInvertible());

  // Hit checking any point should not hit the touch handler region on the
  // layer. If the invertible matrix is accidentally ignored and treated like an
  // identity, then the hit testing will incorrectly hit the layer when it
  // shouldn't.
  gfx::PointF test_point(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(10.f, 10.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(10.f, 30.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(50.f, 50.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(67.f, 48.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(-1.f, -1.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForSinglePositionedLayer) {
  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(10, 10, 50, 50));

  // This layer is positioned, and hit testing should correctly know where the
  // layer is located.
  LayerImpl* test_layer = AddLayer<LayerImpl>();
  test_layer->SetBounds(gfx::Size(100, 100));
  test_layer->SetDrawsContent(true);
  test_layer->SetHitTestable(true);
  test_layer->SetTouchActionRegion(touch_action_region);
  CopyProperties(root_layer(), test_layer);
  test_layer->SetOffsetToTransformParent(gfx::Vector2dF(50.f, 50.f));

  host_impl().active_tree()->SetDeviceViewportRect(
      gfx::Rect(test_layer->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(test_layer)->num_contributors());

  // Hit checking for a point outside the layer should return a null pointer.
  gfx::PointF test_point(49.f, 49.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Even though the layer has a touch handler region containing (101, 101), it
  // should not be visible there since the root render surface would clamp it.
  test_point = gfx::PointF(101.f, 101.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::PointF(51.f, 51.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the test layer.
  test_point = gfx::PointF(61.f, 61.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(test_layer, result_layer);

  test_point = gfx::PointF(99.f, 99.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(test_layer, result_layer);
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForSingleLayerWithDeviceScale) {
  // The layer's device_scale_factor and page_scale_factor should scale the
  // content rect and we should be able to hit the touch handler region by
  // scaling the points accordingly.

  // Set the bounds of the root layer big enough to fit the child when scaled.
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* page_scale_layer = AddLayer<LayerImpl>();
  CopyProperties(root, page_scale_layer);
  CreateTransformNode(page_scale_layer);

  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(10, 10, 30, 30));
  LayerImpl* test_layer = AddLayer<LayerImpl>();
  test_layer->SetBounds(gfx::Size(50, 50));
  test_layer->SetDrawsContent(true);
  test_layer->SetHitTestable(true);
  test_layer->SetTouchActionRegion(touch_action_region);
  CopyProperties(page_scale_layer, test_layer);
  test_layer->SetOffsetToTransformParent(gfx::Vector2dF(25.f, 25.f));

  float device_scale_factor = 3.f;
  float page_scale_factor = 5.f;
  float max_page_scale_factor = 10.f;
  gfx::Size scaled_bounds_for_root = gfx::ScaleToCeiledSize(
      root->bounds(), device_scale_factor * page_scale_factor);
  host_impl().active_tree()->SetDeviceViewportRect(
      gfx::Rect(scaled_bounds_for_root));

  host_impl().active_tree()->SetDeviceScaleFactor(device_scale_factor);
  LayerTreeImpl::ViewportPropertyIds viewport_property_ids;
  viewport_property_ids.page_scale_transform =
      page_scale_layer->transform_tree_index();
  host_impl().active_tree()->SetViewportPropertyIds(viewport_property_ids);
  host_impl().active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, page_scale_factor, max_page_scale_factor);
  host_impl().active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  // The visible content rect for test_layer is actually 100x100, even though
  // its layout size is 50x50, positioned at 25x25.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(root)->num_contributors());

  // Check whether the child layer fits into the root after scaled.
  EXPECT_EQ(gfx::Rect(test_layer->bounds()), test_layer->visible_layer_rect());

  // Hit checking for a point outside the layer should return a null pointer
  // (the root layer does not have a touch event handler, so it will not be
  // tested either).
  gfx::PointF test_point(76.f, 76.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::PointF(26.f, 26.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(34.f, 34.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(65.f, 65.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(74.f, 74.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::PointF(35.f, 35.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(test_layer, result_layer);

  test_point = gfx::PointF(64.f, 64.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(test_layer, result_layer);

  // Check update of page scale factor on the active tree when page scale layer
  // is also the root layer.
  page_scale_factor *= 1.5f;
  host_impl().active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  EXPECT_EQ(page_scale_layer->transform_tree_index(),
            host_impl().active_tree()->PageScaleTransformNode()->id);

  test_point = gfx::PointF(35.f, 35.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(test_layer, result_layer);

  test_point = gfx::PointF(64.f, 64.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(test_layer, result_layer);
}

TEST_F(LayerTreeImplTest, HitCheckingTouchHandlerRegionsForSimpleClippedLayer) {
  // Test that hit-checking will only work for the visible portion of a layer,
  // and not the entire layer bounds. Here we just test the simple axis-aligned
  // case.
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* clipping_layer = AddLayer<LayerImpl>();
  // this layer is positioned, and hit testing should correctly know where
  // the layer is located.
  clipping_layer->SetBounds(gfx::Size(50, 50));
  clipping_layer->SetMasksToBounds(true);
  clipping_layer->SetOffsetToTransformParent(gfx::Vector2dF(25.f, 25.f));
  CopyProperties(root, clipping_layer);
  CreateClipNode(clipping_layer);

  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(10, 10, 50, 50));

  LayerImpl* child = AddLayer<LayerImpl>();
  child->SetBounds(gfx::Size(300, 300));
  child->SetDrawsContent(true);
  child->SetHitTestable(true);
  child->SetTouchActionRegion(touch_action_region);
  CopyProperties(clipping_layer, child);
  child->SetOffsetToTransformParent(
      gfx::Vector2dF(-50.f, -50.f) +
      clipping_layer->offset_to_transform_parent());

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(root)->num_contributors());
  EXPECT_TRUE(child->contributes_to_drawn_render_surface());

  // Hit checking for a point outside the layer should return a null pointer.
  // Despite the child layer being very large, it should be clipped to the root
  // layer's bounds.
  gfx::PointF test_point(24.f, 24.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::PointF(35.f, 35.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  test_point = gfx::PointF(74.f, 74.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::PointF(25.f, 25.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(child, result_layer);

  test_point = gfx::PointF(34.f, 34.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(child, result_layer);
}

TEST_F(LayerTreeImplTest,
       HitCheckingTouchHandlerRegionsForClippedLayerWithDeviceScale) {
  // The layer's device_scale_factor and page_scale_factor should scale the
  // content rect and we should be able to hit the touch handler region by
  // scaling the points accordingly.

  // Set the bounds of the root layer big enough to fit the child when scaled.
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* surface = AddLayer<LayerImpl>();
  surface->SetBounds(gfx::Size(100, 100));
  CopyProperties(root, surface);
  CreateEffectNode(surface).render_surface_reason = RenderSurfaceReason::kTest;

  LayerImpl* clipping_layer = AddLayer<LayerImpl>();
  // This layer is positioned, and hit testing should correctly know where
  // the layer is located.
  clipping_layer->SetBounds(gfx::Size(50, 50));
  clipping_layer->SetMasksToBounds(true);
  CopyProperties(surface, clipping_layer);
  clipping_layer->SetOffsetToTransformParent(gfx::Vector2dF(25.f, 20.f));
  CreateClipNode(clipping_layer);

  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(0, 0, 300, 300));

  LayerImpl* child = AddLayer<LayerImpl>();
  child->SetBounds(gfx::Size(300, 300));
  child->SetDrawsContent(true);
  child->SetHitTestable(true);
  child->SetTouchActionRegion(touch_action_region);
  CopyProperties(clipping_layer, child);
  child->SetOffsetToTransformParent(
      gfx::Vector2dF(-50.f, -50.f) +
      clipping_layer->offset_to_transform_parent());

  float device_scale_factor = 3.f;
  float page_scale_factor = 1.f;
  float max_page_scale_factor = 1.f;
  gfx::Size scaled_bounds_for_root = gfx::ScaleToCeiledSize(
      root->bounds(), device_scale_factor * page_scale_factor);
  host_impl().active_tree()->SetDeviceViewportRect(
      gfx::Rect(scaled_bounds_for_root));

  host_impl().active_tree()->SetDeviceScaleFactor(device_scale_factor);
  host_impl().active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, page_scale_factor, max_page_scale_factor);
  host_impl().active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(2u, GetRenderSurfaceList().size());

  // Hit checking for a point outside the layer should return a null pointer.
  // Despite the child layer being very large, it should be clipped to the root
  // layer's bounds.
  gfx::PointF test_point(24.f, 24.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the child layer.
  test_point = gfx::PointF(25.f, 25.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(child, result_layer);
}

TEST_F(LayerTreeImplTest, HitCheckingTouchHandlerOverlappingRegions) {
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* touch_layer = AddLayer<LayerImpl>();
  // this layer is positioned, and hit testing should correctly know where
  // the layer is located.
  touch_layer->SetBounds(gfx::Size(50, 50));
  touch_layer->SetDrawsContent(true);
  touch_layer->SetHitTestable(true);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(0, 0, 50, 50));
  touch_layer->SetTouchActionRegion(touch_action_region);
  CopyProperties(root, touch_layer);

  LayerImpl* notouch_layer = AddLayer<LayerImpl>();
  // this layer is positioned, and hit testing should correctly know where
  // the layer is located.
  notouch_layer->SetBounds(gfx::Size(50, 50));
  notouch_layer->SetDrawsContent(true);
  notouch_layer->SetHitTestable(true);
  CopyProperties(root, notouch_layer);
  notouch_layer->SetOffsetToTransformParent(gfx::Vector2dF(0, 25));

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(2, GetRenderSurface(root)->num_contributors());
  EXPECT_TRUE(touch_layer->contributes_to_drawn_render_surface());
  EXPECT_TRUE(notouch_layer->contributes_to_drawn_render_surface());

  gfx::PointF test_point(35.f, 35.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);

  // We should have passed through the no-touch layer and found the layer
  // behind it.
  EXPECT_TRUE(result_layer);

  notouch_layer->SetContentsOpaque(true);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);

  // Even with an opaque layer in the middle, we should still find the layer
  // with
  // the touch handler behind it (since we can't assume that opaque layers are
  // opaque to hit testing).
  EXPECT_TRUE(result_layer);

  test_point = gfx::PointF(35.f, 15.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_EQ(touch_layer, result_layer);

  test_point = gfx::PointF(35.f, 65.f);
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeImplTest, HitTestingTouchHandlerRegionsForLayerThatIsNotDrawn) {
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  root->SetHitTestable(true);

  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(10, 10, 30, 30));
  LayerImpl* test_layer = AddLayer<LayerImpl>();
  test_layer->SetBounds(gfx::Size(50, 50));
  test_layer->SetDrawsContent(false);
  test_layer->SetHitTestable(false);
  test_layer->SetTouchActionRegion(touch_action_region);
  CopyProperties(root, test_layer);

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // As test_layer doesn't draw content, it shouldn't contribute content to the
  // root surface.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  EXPECT_FALSE(test_layer->contributes_to_drawn_render_surface());

  // Hit testing for a point outside the test layer should return null pointer.
  // We also implicitly check that the updated screen space transform of a layer
  // that is not in drawn render surface layer list (test_layer) is used during
  // hit testing (becuase the point is inside test_layer with respect to the old
  // screen space transform).
  gfx::PointF test_point(24.f, 24.f);
  test_layer->SetOffsetToTransformParent(gfx::Vector2dF(25.f, 25.f));
  gfx::Transform expected_screen_space_transform;
  expected_screen_space_transform.Translate(25.f, 25.f);

  UpdateDrawProperties(host_impl().active_tree());
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  EXPECT_FALSE(result_layer);
  EXPECT_FALSE(test_layer->contributes_to_drawn_render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_screen_space_transform,
      draw_property_utils::ScreenSpaceTransform(
          test_layer,
          host_impl().active_tree()->property_trees()->transform_tree));

  // We change the position of the test layer such that the test point is now
  // inside the test_layer.
  test_layer->SetOffsetToTransformParent(gfx::Vector2dF(10.f, 10.f));
  test_layer->NoteLayerPropertyChanged();
  expected_screen_space_transform.MakeIdentity();
  expected_screen_space_transform.Translate(10.f, 10.f);

  UpdateDrawProperties(host_impl().active_tree());
  result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point);
  ASSERT_TRUE(result_layer);
  ASSERT_EQ(test_layer, result_layer);
  EXPECT_FALSE(result_layer->contributes_to_drawn_render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_screen_space_transform,
      draw_property_utils::ScreenSpaceTransform(
          test_layer,
          host_impl().active_tree()->property_trees()->transform_tree));
}

TEST_F(LayerTreeImplTest, SelectionBoundsForSingleLayer) {
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurface(root)->num_contributors());

  LayerSelection input;

  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_start = gfx::Point(10, 10);
  input.start.edge_end = gfx::Point(10, 20);
  input.start.layer_id = root->id();

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_start = gfx::Point(50, 10);
  input.end.edge_end = gfx::Point(50, 30);
  input.end.layer_id = root->id();

  viz::Selection<gfx::SelectionBound> output;

  // Empty input bounds should produce empty output bounds.
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(gfx::SelectionBound(), output.start);
  EXPECT_EQ(gfx::SelectionBound(), output.end);

  // Selection bounds should produce distinct left and right bounds.
  host_impl().active_tree()->RegisterSelection(input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(input.start.type, output.start.type());
  EXPECT_EQ(gfx::PointF(input.start.edge_end), output.start.edge_end());
  EXPECT_EQ(gfx::PointF(input.start.edge_start), output.start.edge_start());
  EXPECT_EQ(gfx::PointF(input.start.edge_end), output.start.visible_edge_end());
  EXPECT_EQ(gfx::PointF(input.start.edge_start),
            output.start.visible_edge_start());
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(input.end.type, output.end.type());
  EXPECT_EQ(gfx::PointF(input.end.edge_end), output.end.edge_end());
  EXPECT_EQ(gfx::PointF(input.end.edge_start), output.end.edge_start());
  EXPECT_EQ(gfx::PointF(input.end.edge_end), output.end.visible_edge_end());
  EXPECT_EQ(gfx::PointF(input.end.edge_start), output.end.visible_edge_start());
  EXPECT_TRUE(output.end.visible());

  // Selection bounds should produce distinct left and right bounds for the
  // vertical text.
  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_start = gfx::Point(20, 10);
  input.start.edge_end = gfx::Point(10, 10);
  input.start.layer_id = root->id();

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_start = gfx::Point(30, 20);
  input.end.edge_end = gfx::Point(50, 20);
  input.end.layer_id = root->id();

  host_impl().active_tree()->RegisterSelection(input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(input.start.type, output.start.type());
  EXPECT_EQ(gfx::PointF(input.start.edge_end), output.start.edge_end());
  EXPECT_EQ(gfx::PointF(input.start.edge_start), output.start.edge_start());
  EXPECT_EQ(gfx::PointF(input.start.edge_end), output.start.visible_edge_end());
  EXPECT_EQ(gfx::PointF(input.start.edge_start),
            output.start.visible_edge_start());
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(input.end.type, output.end.type());
  EXPECT_EQ(gfx::PointF(input.end.edge_end), output.end.edge_end());
  EXPECT_EQ(gfx::PointF(input.end.edge_start), output.end.edge_start());
  EXPECT_EQ(gfx::PointF(input.end.edge_end), output.end.visible_edge_end());
  EXPECT_EQ(gfx::PointF(input.end.edge_start), output.end.visible_edge_start());
  EXPECT_TRUE(output.end.visible());

  // Insertion bounds should produce identical left and right bounds.
  LayerSelection insertion_input;
  insertion_input.start.type = gfx::SelectionBound::CENTER;
  insertion_input.start.edge_start = gfx::Point(15, 10);
  insertion_input.start.edge_end = gfx::Point(15, 30);
  insertion_input.start.layer_id = root->id();
  insertion_input.end = insertion_input.start;
  host_impl().active_tree()->RegisterSelection(insertion_input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(insertion_input.start.type, output.start.type());
  EXPECT_EQ(gfx::PointF(insertion_input.start.edge_end),
            output.start.edge_end());
  EXPECT_EQ(gfx::PointF(insertion_input.start.edge_start),
            output.start.edge_start());
  EXPECT_EQ(gfx::PointF(insertion_input.start.edge_end),
            output.start.visible_edge_end());
  EXPECT_EQ(gfx::PointF(insertion_input.start.edge_start),
            output.start.visible_edge_start());
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(output.start, output.end);
}

TEST_F(LayerTreeImplTest, SelectionBoundsForPartialOccludedLayers) {
  LayerImpl* root = root_layer();
  root->SetDrawsContent(true);
  root->SetBounds(gfx::Size(100, 100));

  gfx::Vector2dF clipping_offset(10, 10);

  LayerImpl* clipping_layer = AddLayer<LayerImpl>();
  // The clipping layer should occlude the right selection bound.
  clipping_layer->SetBounds(gfx::Size(50, 50));
  clipping_layer->SetMasksToBounds(true);
  CopyProperties(root, clipping_layer);
  clipping_layer->SetOffsetToTransformParent(clipping_offset);
  CreateClipNode(clipping_layer);

  LayerImpl* clipped_layer = AddLayer<LayerImpl>();
  clipped_layer->SetBounds(gfx::Size(100, 100));
  clipped_layer->SetDrawsContent(true);
  CopyProperties(clipping_layer, clipped_layer);
  clipped_layer->SetOffsetToTransformParent(
      clipping_layer->offset_to_transform_parent());

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());

  LayerSelection input;
  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_start = gfx::Point(25, 10);
  input.start.edge_end = gfx::Point(25, 30);
  input.start.layer_id = clipped_layer->id();

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_start = gfx::Point(75, 10);
  input.end.edge_end = gfx::Point(75, 30);
  input.end.layer_id = clipped_layer->id();
  host_impl().active_tree()->RegisterSelection(input);

  // The right bound should be occluded by the clip layer.
  viz::Selection<gfx::SelectionBound> output;
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(input.start.type, output.start.type());
  auto expected_output_edge_start = gfx::PointF(input.start.edge_start);
  auto expected_output_edge_end = gfx::PointF(input.start.edge_end);
  expected_output_edge_start.Offset(clipping_offset.x(), clipping_offset.y());
  expected_output_edge_end.Offset(clipping_offset.x(), clipping_offset.y());
  EXPECT_EQ(expected_output_edge_start, output.start.edge_start());
  EXPECT_EQ(expected_output_edge_end, output.start.edge_end());
  EXPECT_EQ(expected_output_edge_start, output.start.visible_edge_start());
  EXPECT_EQ(expected_output_edge_end, output.start.visible_edge_end());
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(input.end.type, output.end.type());
  expected_output_edge_start = gfx::PointF(input.end.edge_start);
  expected_output_edge_end = gfx::PointF(input.end.edge_end);
  expected_output_edge_end.Offset(clipping_offset.x(), clipping_offset.y());
  expected_output_edge_start.Offset(clipping_offset.x(), clipping_offset.y());
  EXPECT_EQ(expected_output_edge_start, output.end.edge_start());
  EXPECT_EQ(expected_output_edge_end, output.end.edge_end());

  gfx::RectF visible_layer_rect(clipped_layer->visible_layer_rect());
  gfx::PointF expected_output_visible_edge_start;
  gfx::PointF expected_output_visible_edge_end;
  std::tie(expected_output_visible_edge_start,
           expected_output_visible_edge_end) =
      GetVisibleSelectionEndPoints(visible_layer_rect,
                                   gfx::PointF(input.end.edge_start),
                                   gfx::PointF(input.end.edge_end));
  expected_output_visible_edge_start.Offset(clipping_offset.x(),
                                            clipping_offset.y());
  expected_output_visible_edge_end.Offset(clipping_offset.x(),
                                          clipping_offset.y());

  EXPECT_EQ(expected_output_visible_edge_start,
            output.end.visible_edge_start());
  EXPECT_EQ(expected_output_visible_edge_end, output.end.visible_edge_end());
  EXPECT_FALSE(output.end.visible());

  // The right bound should be occluded by the clip layer for the vertical text.
  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_start = gfx::Point(25, 10);
  input.start.edge_end = gfx::Point(15, 10);
  input.start.layer_id = clipped_layer->id();

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_start = gfx::Point(75, 30);
  input.end.edge_end = gfx::Point(85, 30);
  input.end.layer_id = clipped_layer->id();
  host_impl().active_tree()->RegisterSelection(input);

  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(input.start.type, output.start.type());
  expected_output_edge_start = gfx::PointF(input.start.edge_start);
  expected_output_edge_end = gfx::PointF(input.start.edge_end);
  expected_output_edge_start.Offset(clipping_offset.x(), clipping_offset.y());
  expected_output_edge_end.Offset(clipping_offset.x(), clipping_offset.y());
  EXPECT_EQ(expected_output_edge_start, output.start.edge_start());
  EXPECT_EQ(expected_output_edge_end, output.start.edge_end());
  EXPECT_EQ(expected_output_edge_start, output.start.visible_edge_start());
  EXPECT_EQ(expected_output_edge_end, output.start.visible_edge_end());
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(input.end.type, output.end.type());
  expected_output_edge_start = gfx::PointF(input.end.edge_start);
  expected_output_edge_end = gfx::PointF(input.end.edge_end);
  expected_output_edge_end.Offset(clipping_offset.x(), clipping_offset.y());
  expected_output_edge_start.Offset(clipping_offset.x(), clipping_offset.y());
  EXPECT_EQ(expected_output_edge_start, output.end.edge_start());
  EXPECT_EQ(expected_output_edge_end, output.end.edge_end());

  std::tie(expected_output_visible_edge_start,
           expected_output_visible_edge_end) =
      GetVisibleSelectionEndPoints(visible_layer_rect,
                                   gfx::PointF(input.end.edge_start),
                                   gfx::PointF(input.end.edge_end));
  expected_output_visible_edge_start.Offset(clipping_offset.x(),
                                            clipping_offset.y());
  expected_output_visible_edge_end.Offset(clipping_offset.x(),
                                          clipping_offset.y());

  EXPECT_EQ(expected_output_visible_edge_start,
            output.end.visible_edge_start());
  EXPECT_EQ(expected_output_visible_edge_end, output.end.visible_edge_end());
  EXPECT_FALSE(output.end.visible());

  // Handles outside the viewport bounds should be marked invisible.
  input.start.edge_start = gfx::Point(-25, 0);
  input.start.edge_end = gfx::Point(-25, 20);
  host_impl().active_tree()->RegisterSelection(input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_FALSE(output.start.visible());

  input.start.edge_start = gfx::Point(0, -25);
  input.start.edge_end = gfx::Point(0, -5);
  host_impl().active_tree()->RegisterSelection(input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_FALSE(output.start.visible());

  // If the handle end is partially visible, the handle is marked visible.
  input.start.edge_start = gfx::Point(0, -20);
  input.start.edge_end = gfx::Point(0, 1);
  host_impl().active_tree()->RegisterSelection(input);
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_TRUE(output.start.visible());
}

TEST_F(LayerTreeImplTest, SelectionBoundsForScaledLayers) {
  LayerImpl* root = root_layer();
  root->SetDrawsContent(true);
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* page_scale_layer = AddLayer<LayerImpl>();
  page_scale_layer->SetBounds(gfx::Size(50, 50));
  CopyProperties(root, page_scale_layer);
  CreateTransformNode(page_scale_layer);

  gfx::Vector2dF sub_layer_offset(10, 0);
  LayerImpl* sub_layer = AddLayer<LayerImpl>();
  sub_layer->SetBounds(gfx::Size(50, 50));
  sub_layer->SetDrawsContent(true);
  CopyProperties(page_scale_layer, sub_layer);
  sub_layer->SetOffsetToTransformParent(sub_layer_offset);

  UpdateDrawProperties(host_impl().active_tree());

  float device_scale_factor = 3.f;
  float page_scale_factor = 5.f;
  gfx::Size scaled_bounds_for_root = gfx::ScaleToCeiledSize(
      root->bounds(), device_scale_factor * page_scale_factor);

  LayerTreeImpl::ViewportPropertyIds viewport_property_ids;
  viewport_property_ids.page_scale_transform =
      page_scale_layer->transform_tree_index();
  host_impl().active_tree()->SetViewportPropertyIds(viewport_property_ids);
  host_impl().active_tree()->SetDeviceViewportRect(
      gfx::Rect(scaled_bounds_for_root));
  host_impl().active_tree()->SetDeviceScaleFactor(device_scale_factor);
  host_impl().active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  host_impl().active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, page_scale_factor, page_scale_factor);
  host_impl().active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  UpdateDrawProperties(host_impl().active_tree());

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());

  LayerSelection input;
  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_start = gfx::Point(10, 10);
  input.start.edge_end = gfx::Point(10, 30);
  input.start.layer_id = page_scale_layer->id();

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_start = gfx::Point(0, 0);
  input.end.edge_end = gfx::Point(0, 20);
  input.end.layer_id = sub_layer->id();
  host_impl().active_tree()->RegisterSelection(input);

  // The viewport bounds should be properly scaled by the page scale, but should
  // remain in DIP coordinates.
  viz::Selection<gfx::SelectionBound> output;
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(input.start.type, output.start.type());
  auto expected_output_edge_start = gfx::PointF(input.start.edge_start);
  auto expected_output_edge_end = gfx::PointF(input.start.edge_end);
  expected_output_edge_start.Scale(page_scale_factor);
  expected_output_edge_end.Scale(page_scale_factor);
  EXPECT_EQ(expected_output_edge_start, output.start.edge_start());
  EXPECT_EQ(expected_output_edge_end, output.start.edge_end());
  EXPECT_EQ(expected_output_edge_start, output.start.visible_edge_start());
  EXPECT_EQ(expected_output_edge_end, output.start.visible_edge_end());
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(input.end.type, output.end.type());

  expected_output_edge_start = gfx::PointF(input.end.edge_start);
  expected_output_edge_end = gfx::PointF(input.end.edge_end);
  expected_output_edge_start.Offset(sub_layer_offset.x(), sub_layer_offset.y());
  expected_output_edge_end.Offset(sub_layer_offset.x(), sub_layer_offset.y());
  expected_output_edge_start.Scale(page_scale_factor);
  expected_output_edge_end.Scale(page_scale_factor);
  EXPECT_EQ(expected_output_edge_start, output.end.edge_start());
  EXPECT_EQ(expected_output_edge_end, output.end.edge_end());
  EXPECT_EQ(expected_output_edge_start, output.end.visible_edge_start());
  EXPECT_EQ(expected_output_edge_end, output.end.visible_edge_end());
  EXPECT_TRUE(output.end.visible());
}

TEST_F(LayerTreeImplTest, SelectionBoundsForDSFEnabled) {
  LayerImpl* root = root_layer();
  root->SetDrawsContent(true);
  root->SetBounds(gfx::Size(100, 100));
  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));

  gfx::Vector2dF sub_layer_offset(10, 0);
  LayerImpl* sub_layer = AddLayer<LayerImpl>();
  sub_layer->SetBounds(gfx::Size(50, 50));
  sub_layer->SetDrawsContent(true);
  CopyProperties(root, sub_layer);
  sub_layer->SetOffsetToTransformParent(sub_layer_offset);

  UpdateDrawProperties(host_impl().active_tree());

  float device_scale_factor = 3.f;
  float painted_device_scale_factor = 5.f;
  host_impl().active_tree()->SetDeviceScaleFactor(device_scale_factor);
  host_impl().active_tree()->set_painted_device_scale_factor(
      painted_device_scale_factor);

  LayerSelection input;
  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_start = gfx::Point(10, 10);
  input.start.edge_end = gfx::Point(10, 30);
  input.start.layer_id = root->id();

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_start = gfx::Point(0, 0);
  input.end.edge_end = gfx::Point(0, 20);
  input.end.layer_id = sub_layer->id();
  host_impl().active_tree()->RegisterSelection(input);

  // The viewport bounds should be properly scaled by the page scale, but should
  // remain in DIP coordinates.
  viz::Selection<gfx::SelectionBound> output;
  host_impl().active_tree()->GetViewportSelection(&output);
  EXPECT_EQ(input.start.type, output.start.type());
  auto expected_output_edge_start = gfx::PointF(input.start.edge_start);
  auto expected_output_edge_end = gfx::PointF(input.start.edge_end);
  expected_output_edge_start.Scale(
      1.f / (device_scale_factor * painted_device_scale_factor));
  expected_output_edge_end.Scale(
      1.f / (device_scale_factor * painted_device_scale_factor));
  EXPECT_EQ(expected_output_edge_start, output.start.edge_start());
  EXPECT_EQ(expected_output_edge_end, output.start.edge_end());
  EXPECT_EQ(expected_output_edge_start, output.start.visible_edge_start());
  EXPECT_EQ(expected_output_edge_end, output.start.visible_edge_end());
  EXPECT_TRUE(output.start.visible());
  EXPECT_EQ(input.end.type, output.end.type());

  expected_output_edge_start = gfx::PointF(input.end.edge_start);
  expected_output_edge_end = gfx::PointF(input.end.edge_end);
  expected_output_edge_start.Offset(sub_layer_offset.x(), sub_layer_offset.y());
  expected_output_edge_end.Offset(sub_layer_offset.x(), sub_layer_offset.y());
  expected_output_edge_start.Scale(
      1.f / (device_scale_factor * painted_device_scale_factor));
  expected_output_edge_end.Scale(
      1.f / (device_scale_factor * painted_device_scale_factor));
  EXPECT_EQ(expected_output_edge_start, output.end.edge_start());
  EXPECT_EQ(expected_output_edge_end, output.end.edge_end());
  EXPECT_EQ(expected_output_edge_start, output.end.visible_edge_start());
  EXPECT_EQ(expected_output_edge_end, output.end.visible_edge_end());
  EXPECT_TRUE(output.end.visible());
}

TEST_F(LayerTreeImplTest, SelectionBoundsWithLargeTransforms) {
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  gfx::Transform large_transform;
  large_transform.Scale(SkDoubleToMScalar(1e37), SkDoubleToMScalar(1e37));
  large_transform.RotateAboutYAxis(30);

  LayerImpl* child = AddLayer<LayerImpl>();
  child->SetBounds(gfx::Size(100, 100));
  CopyProperties(root, child);
  CreateTransformNode(child).local = large_transform;

  LayerImpl* grand_child = AddLayer<LayerImpl>();
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);
  CopyProperties(child, grand_child);
  CreateTransformNode(grand_child).local = large_transform;

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());

  LayerSelection input;

  input.start.type = gfx::SelectionBound::LEFT;
  input.start.edge_start = gfx::Point(10, 10);
  input.start.edge_end = gfx::Point(10, 20);
  input.start.layer_id = grand_child->id();

  input.end.type = gfx::SelectionBound::RIGHT;
  input.end.edge_start = gfx::Point(50, 10);
  input.end.edge_end = gfx::Point(50, 30);
  input.end.layer_id = grand_child->id();

  host_impl().active_tree()->RegisterSelection(input);

  viz::Selection<gfx::SelectionBound> output;
  host_impl().active_tree()->GetViewportSelection(&output);

  // edge_end and edge_start aren't allowed to have NaNs, so the selection
  // should be empty.
  EXPECT_EQ(gfx::SelectionBound(), output.start);
  EXPECT_EQ(gfx::SelectionBound(), output.end);
}

TEST_F(LayerTreeImplTest, NumLayersTestOne) {
  // Root is created by the test harness.
  EXPECT_EQ(1u, host_impl().active_tree()->NumLayers());
  EXPECT_TRUE(root_layer());
  // Create another layer, should increment.
  AddLayer<LayerImpl>();
  EXPECT_EQ(2u, host_impl().active_tree()->NumLayers());
}

TEST_F(LayerTreeImplTest, NumLayersSmallTree) {
  EXPECT_EQ(1u, host_impl().active_tree()->NumLayers());
  AddLayer<LayerImpl>();
  AddLayer<LayerImpl>();
  AddLayer<LayerImpl>();
  EXPECT_EQ(4u, host_impl().active_tree()->NumLayers());
}

TEST_F(LayerTreeImplTest, DeviceScaleFactorNeedsDrawPropertiesUpdate) {
  host_impl().active_tree()->UpdateDrawProperties();
  EXPECT_FALSE(host_impl().active_tree()->needs_update_draw_properties());
  host_impl().active_tree()->SetDeviceScaleFactor(1.f);
  EXPECT_FALSE(host_impl().active_tree()->needs_update_draw_properties());
  host_impl().active_tree()->SetDeviceScaleFactor(2.f);
  EXPECT_TRUE(host_impl().active_tree()->needs_update_draw_properties());
}

TEST_F(LayerTreeImplTest, RasterColorSpaceDoesNotNeedDrawPropertiesUpdate) {
  host_impl().active_tree()->SetRasterColorSpace(
      1, gfx::ColorSpace::CreateXYZD50());
  host_impl().active_tree()->UpdateDrawProperties();
  EXPECT_FALSE(host_impl().active_tree()->needs_update_draw_properties());
  host_impl().active_tree()->SetRasterColorSpace(1,
                                                 gfx::ColorSpace::CreateSRGB());
  EXPECT_FALSE(host_impl().active_tree()->needs_update_draw_properties());
}

TEST_F(LayerTreeImplTest, HitTestingCorrectLayerWheelListener) {
  host_impl().active_tree()->set_event_listener_properties(
      EventListenerClass::kMouseWheel, EventListenerProperties::kBlocking);

  LayerImpl* root = root_layer();
  LayerImpl* top = AddLayer<LayerImpl>();
  LayerImpl* left_child = AddLayer<LayerImpl>();
  LayerImpl* right_child = AddLayer<LayerImpl>();

  {
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, 10);
    top->SetBounds(gfx::Size(100, 100));
    top->SetDrawsContent(true);
    top->SetHitTestable(true);
    CopyProperties(root, top);
    CreateTransformNode(top).local = translate_z;
  }
  {
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, 10);
    left_child->SetBounds(gfx::Size(100, 100));
    left_child->SetDrawsContent(true);
    left_child->SetHitTestable(true);
    CopyProperties(top, left_child);
    CreateTransformNode(left_child).local = translate_z;
  }
  {
    gfx::Transform translate_z;
    translate_z.Translate3d(0, 0, 10);
    right_child->SetBounds(gfx::Size(100, 100));
    CopyProperties(top, right_child);
    CreateTransformNode(right_child).local = translate_z;
  }

  host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  UpdateDrawProperties(host_impl().active_tree());
  CHECK_EQ(1u, GetRenderSurfaceList().size());

  gfx::PointF test_point = gfx::PointF(1.f, 1.f);
  LayerImpl* result_layer =
      host_impl().active_tree()->FindLayerThatIsHitByPoint(test_point);

  EXPECT_EQ(left_child, result_layer);
}

namespace {

class PersistentSwapPromise
    : public SwapPromise,
      public base::SupportsWeakPtr<PersistentSwapPromise> {
 public:
  PersistentSwapPromise() = default;
  ~PersistentSwapPromise() override = default;

  void DidActivate() override {}
  MOCK_METHOD1(WillSwap, void(viz::CompositorFrameMetadata* metadata));
  MOCK_METHOD0(DidSwap, void());

  DidNotSwapAction DidNotSwap(DidNotSwapReason reason) override {
    return DidNotSwapAction::KEEP_ACTIVE;
  }

  void OnCommit() override {}
  int64_t TraceId() const override { return 0; }
};

class NotPersistentSwapPromise
    : public SwapPromise,
      public base::SupportsWeakPtr<NotPersistentSwapPromise> {
 public:
  NotPersistentSwapPromise() = default;
  ~NotPersistentSwapPromise() override = default;

  void DidActivate() override {}
  void WillSwap(viz::CompositorFrameMetadata* metadata) override {}
  void DidSwap() override {}

  DidNotSwapAction DidNotSwap(DidNotSwapReason reason) override {
    return DidNotSwapAction::BREAK_PROMISE;
  }

  void OnCommit() override {}
  int64_t TraceId() const override { return 0; }
};

}  // namespace

TEST_F(LayerTreeImplTest, PersistentSwapPromisesAreKeptAlive) {
  const size_t promises_count = 2;

  std::vector<base::WeakPtr<PersistentSwapPromise>> persistent_promises;
  std::vector<std::unique_ptr<PersistentSwapPromise>>
      persistent_promises_to_pass;
  for (size_t i = 0; i < promises_count; ++i) {
    persistent_promises_to_pass.push_back(
        std::make_unique<PersistentSwapPromise>());
  }

  for (auto& promise : persistent_promises_to_pass) {
    persistent_promises.push_back(promise->AsWeakPtr());
    host_impl().active_tree()->QueueSwapPromise(std::move(promise));
  }

  std::vector<std::unique_ptr<SwapPromise>> promises;
  host_impl().active_tree()->PassSwapPromises(std::move(promises));
  host_impl().active_tree()->BreakSwapPromises(
      SwapPromise::DidNotSwapReason::SWAP_FAILS);

  ASSERT_EQ(promises_count, persistent_promises.size());
  for (size_t i = 0; i < persistent_promises.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "While checking case #" << i);
    ASSERT_TRUE(persistent_promises[i]);
    EXPECT_CALL(*persistent_promises[i], WillSwap(testing::_));
  }
  host_impl().active_tree()->FinishSwapPromises(nullptr);
}

TEST_F(LayerTreeImplTest, NotPersistentSwapPromisesAreDroppedWhenSwapFails) {
  const size_t promises_count = 2;

  std::vector<base::WeakPtr<NotPersistentSwapPromise>> not_persistent_promises;
  std::vector<std::unique_ptr<NotPersistentSwapPromise>>
      not_persistent_promises_to_pass;
  for (size_t i = 0; i < promises_count; ++i) {
    not_persistent_promises_to_pass.push_back(
        std::make_unique<NotPersistentSwapPromise>());
  }

  for (auto& promise : not_persistent_promises_to_pass) {
    not_persistent_promises.push_back(promise->AsWeakPtr());
    host_impl().active_tree()->QueueSwapPromise(std::move(promise));
  }
  std::vector<std::unique_ptr<SwapPromise>> promises;
  host_impl().active_tree()->PassSwapPromises(std::move(promises));

  ASSERT_EQ(promises_count, not_persistent_promises.size());
  for (size_t i = 0; i < not_persistent_promises.size(); ++i) {
    EXPECT_FALSE(not_persistent_promises[i]) << "While checking case #" << i;
  }

  // Finally, check that not persistent promise doesn't survive
  // |LayerTreeImpl::BreakSwapPromises|.
  {
    std::unique_ptr<NotPersistentSwapPromise> promise(
        new NotPersistentSwapPromise());
    auto weak_promise = promise->AsWeakPtr();
    host_impl().active_tree()->QueueSwapPromise(std::move(promise));
    host_impl().active_tree()->BreakSwapPromises(
        SwapPromise::DidNotSwapReason::SWAP_FAILS);
    EXPECT_FALSE(weak_promise);
  }
}

TEST_F(LayerTreeImplTest, TrackPictureLayersWithPaintWorklets) {
  host_impl().CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl().pending_tree();

  // Initially there are no layers in the set.
  EXPECT_EQ(pending_tree->picture_layers_with_paint_worklets().size(), 0u);

  auto* root = EnsureRootLayerInPendingTree();
  root->SetBounds(gfx::Size(100, 100));

  // Add three layers; two with PaintWorklets and one without.
  auto* child1 = AddLayerInPendingTree<PictureLayerImpl>();
  child1->SetBounds(gfx::Size(100, 100));
  auto* child2 = AddLayerInPendingTree<PictureLayerImpl>();
  child2->SetBounds(gfx::Size(100, 100));
  auto* child3 = AddLayerInPendingTree<PictureLayerImpl>();
  child3->SetBounds(gfx::Size(100, 100));

  CopyProperties(root, child1);
  CopyProperties(root, child2);
  CopyProperties(root, child3);

  Region empty_invalidation;
  scoped_refptr<RasterSource> raster_source1(
      FakeRasterSource::CreateFilledWithPaintWorklet(child1->bounds()));
  child1->UpdateRasterSource(raster_source1, &empty_invalidation, nullptr,
                             nullptr);
  scoped_refptr<RasterSource> raster_source3(
      FakeRasterSource::CreateFilledWithPaintWorklet(child3->bounds()));
  child3->UpdateRasterSource(raster_source3, &empty_invalidation, nullptr,
                             nullptr);

  // The set should correctly track which layers are in it.
  const base::flat_set<PictureLayerImpl*>& layers =
      pending_tree->picture_layers_with_paint_worklets();
  EXPECT_EQ(layers.size(), 2u);
  EXPECT_TRUE(layers.contains(child1));
  EXPECT_TRUE(layers.contains(child3));

  // Test explicitly removing a layer from the set.
  scoped_refptr<RasterSource> empty_raster_source(
      FakeRasterSource::CreateFilled(child1->bounds()));
  child1->UpdateRasterSource(empty_raster_source, &empty_invalidation, nullptr,
                             nullptr);
  EXPECT_EQ(layers.size(), 1u);
  EXPECT_FALSE(layers.contains(child1));

  pending_tree->DetachLayers();
  EXPECT_EQ(layers.size(), 0u);
}

namespace {
class CommitToPendingTreeLayerTreeImplTestSettings : public LayerListSettings {
 public:
  CommitToPendingTreeLayerTreeImplTestSettings() {
    commit_to_active_tree = false;
  }
};

class CommitToPendingTreeLayerTreeImplTest : public LayerTreeImplTest {
 public:
  CommitToPendingTreeLayerTreeImplTest()
      : LayerTreeImplTest(CommitToPendingTreeLayerTreeImplTestSettings()) {}
};
}  // namespace

TEST_F(CommitToPendingTreeLayerTreeImplTest,
       ElementIdToAnimationMapsTrackOnlyOnSyncTree) {
  ASSERT_FALSE(host_impl().CommitToActiveTree());

  // When we have a pending tree (e.g. commit_to_active_tree is false), the
  // various ElementId to animation maps should not track anything for the
  // active tree (as they are only used on the sync tree).
  LayerTreeImpl* active_tree = host_impl().active_tree();
  UpdateDrawProperties(active_tree);
  LayerImpl* active_root = active_tree->root_layer();

  auto& active_opacity_map =
      active_tree->element_id_to_opacity_animations_for_testing();
  ASSERT_EQ(active_opacity_map.size(), 0u);
  active_tree->SetOpacityMutated(active_root->element_id(), 0.5f);
  EXPECT_EQ(active_opacity_map.size(), 0u);

  auto& active_transform_map =
      active_tree->element_id_to_transform_animations_for_testing();
  ASSERT_EQ(active_transform_map.size(), 0u);
  active_tree->SetTransformMutated(active_root->element_id(), gfx::Transform());
  EXPECT_EQ(active_transform_map.size(), 0u);

  auto& active_filter_map =
      active_tree->element_id_to_filter_animations_for_testing();
  ASSERT_EQ(active_filter_map.size(), 0u);
  active_tree->SetFilterMutated(active_root->element_id(), FilterOperations());
  EXPECT_EQ(active_filter_map.size(), 0u);

  // The pending/recycle tree however should track them. Here we need two nodes
  // (the root and a child) as we will be adding entries for both the pending
  // and recycle tree cases.
  host_impl().CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl().pending_tree();
  LayerImpl* pending_root = EnsureRootLayerInPendingTree();
  pending_root->SetBounds(gfx::Size(1, 1));
  LayerImpl* child = AddLayerInPendingTree<LayerImpl>();
  pending_tree->SetElementIdsForTesting();

  // A scale transform forces a TransformNode.
  gfx::Transform scale3d;
  scale3d.Scale3d(1, 1, 0.5);
  CopyProperties(pending_root, child);
  CreateTransformNode(child).local = scale3d;
  // A non-one opacity forces an EffectNode.
  CreateEffectNode(child).opacity = 0.9f;

  UpdateDrawProperties(pending_tree);

  auto& pending_opacity_map =
      pending_tree->element_id_to_opacity_animations_for_testing();
  ASSERT_EQ(pending_opacity_map.size(), 0u);
  pending_tree->SetOpacityMutated(pending_root->element_id(), 0.5f);
  EXPECT_EQ(pending_opacity_map.size(), 1u);

  auto& pending_transform_map =
      pending_tree->element_id_to_transform_animations_for_testing();
  ASSERT_EQ(pending_transform_map.size(), 0u);
  pending_tree->SetTransformMutated(pending_root->element_id(),
                                    gfx::Transform());
  EXPECT_EQ(pending_transform_map.size(), 1u);

  auto& pending_filter_map =
      pending_tree->element_id_to_filter_animations_for_testing();
  ASSERT_EQ(pending_filter_map.size(), 0u);
  pending_tree->SetFilterMutated(pending_root->element_id(),
                                 FilterOperations());
  EXPECT_EQ(pending_filter_map.size(), 1u);

  // Finally, check the recycle tree - this should still track them.
  host_impl().ActivateSyncTree();
  LayerTreeImpl* recycle_tree = host_impl().recycle_tree();
  ASSERT_TRUE(recycle_tree);

  auto& recycle_opacity_map =
      recycle_tree->element_id_to_opacity_animations_for_testing();
  ASSERT_EQ(recycle_opacity_map.size(), 1u);
  recycle_tree->SetOpacityMutated(child->element_id(), 0.5f);
  EXPECT_EQ(recycle_opacity_map.size(), 2u);

  auto& recycle_transform_map =
      recycle_tree->element_id_to_transform_animations_for_testing();
  ASSERT_EQ(recycle_transform_map.size(), 1u);
  recycle_tree->SetTransformMutated(child->element_id(), gfx::Transform());
  EXPECT_EQ(recycle_transform_map.size(), 2u);

  auto& recycle_filter_map =
      recycle_tree->element_id_to_filter_animations_for_testing();
  ASSERT_EQ(recycle_filter_map.size(), 1u);
  recycle_tree->SetFilterMutated(child->element_id(), FilterOperations());
  EXPECT_EQ(recycle_filter_map.size(), 2u);
}

TEST_F(LayerTreeImplTest, ElementIdToAnimationMapsTrackOnlyOnSyncTree) {
  ASSERT_TRUE(host_impl().CommitToActiveTree());

  // When we are commiting directly to the active tree, the various ElementId to
  // animation maps should track on the active tree (as it is the sync tree, and
  // they are used on the sync tree).
  LayerTreeImpl* active_tree = host_impl().active_tree();
  UpdateDrawProperties(active_tree);
  LayerImpl* root = active_tree->root_layer();

  auto& opacity_map =
      active_tree->element_id_to_opacity_animations_for_testing();
  ASSERT_EQ(opacity_map.size(), 0u);
  active_tree->SetOpacityMutated(root->element_id(), 0.5f);
  EXPECT_EQ(opacity_map.size(), 1u);

  auto& transform_map =
      active_tree->element_id_to_transform_animations_for_testing();
  ASSERT_EQ(transform_map.size(), 0u);
  active_tree->SetTransformMutated(root->element_id(), gfx::Transform());
  EXPECT_EQ(transform_map.size(), 1u);

  auto& filter_map = active_tree->element_id_to_filter_animations_for_testing();
  ASSERT_EQ(filter_map.size(), 0u);
  active_tree->SetFilterMutated(root->element_id(), FilterOperations());
  EXPECT_EQ(filter_map.size(), 1u);
}

class LayerTreeImplOcclusionSettings : public LayerListSettings {
 public:
  explicit LayerTreeImplOcclusionSettings(bool enabled) {
    enable_occlusion = enabled;
    minimum_occlusion_tracking_size = gfx::Size(1, 1);
  }
};

class LayerTreeImplOcclusionTest : public LayerTreeImplTest {
 public:
  explicit LayerTreeImplOcclusionTest(bool enable_occlusion)
      : LayerTreeImplTest(LayerTreeImplOcclusionSettings(enable_occlusion)),
        enable_occlusion_(enable_occlusion) {}

  void TestOcclusion() {
    LayerImpl* root = root_layer();
    root->SetBounds(gfx::Size(100, 100));

    // Create a 50x50 layer in the center of our root bounds.
    LayerImpl* bottom_layer = AddLayer<LayerImpl>();
    bottom_layer->SetBounds(gfx::Size(50, 50));
    bottom_layer->SetDrawsContent(true);
    bottom_layer->SetContentsOpaque(true);
    CopyProperties(root, bottom_layer);
    bottom_layer->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));

    // Create a full-bounds 100x100 layer which occludes the 50x50 layer.
    LayerImpl* occluding_layer = AddLayer<LayerImpl>();
    occluding_layer->SetBounds(gfx::Size(100, 100));
    occluding_layer->SetDrawsContent(true);
    occluding_layer->SetContentsOpaque(true);
    CopyProperties(root, occluding_layer);

    host_impl().active_tree()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
    UpdateDrawProperties(host_impl().active_tree());

    LayerTreeImpl* active_tree = host_impl().active_tree();
    if (enable_occlusion_) {
      // With occlusion on, the root is fully occluded, as is the bottom layer.
      EXPECT_TRUE(active_tree->UnoccludedScreenSpaceRegion().IsEmpty());
      EXPECT_TRUE(bottom_layer->draw_properties()
                      .occlusion_in_content_space.HasOcclusion());
    } else {
      // With occlusion off, the full root should be unoccluded and the bottom
      // layer should have no occlusion.
      EXPECT_TRUE(active_tree->UnoccludedScreenSpaceRegion().Contains(
          gfx::Rect(root->bounds())));
      EXPECT_FALSE(bottom_layer->draw_properties()
                       .occlusion_in_content_space.HasOcclusion());
    }
  }

 private:
  bool enable_occlusion_;
};

class LayerTreeImplOcclusionDisabledTest : public LayerTreeImplOcclusionTest {
 public:
  LayerTreeImplOcclusionDisabledTest() : LayerTreeImplOcclusionTest(false) {}
};

class LayerTreeImplOcclusionEnabledTest : public LayerTreeImplOcclusionTest {
 public:
  LayerTreeImplOcclusionEnabledTest() : LayerTreeImplOcclusionTest(true) {}
};

TEST_F(LayerTreeImplOcclusionDisabledTest, OcclusionDisabled) {
  TestOcclusion();
}

TEST_F(LayerTreeImplOcclusionEnabledTest, OcclusionEnabled) {
  TestOcclusion();
}

}  // namespace
}  // namespace cc
