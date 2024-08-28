// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/effect_tree_layer_list_iterator.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_operations.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {
namespace {

bool LayerSubtreeHasCopyRequest(Layer* layer) {
  return GetEffectNode(layer)->subtree_has_copy_request;
}

class DrawPropertiesTestBase : public LayerTreeImplTestBase {
 public:
  explicit DrawPropertiesTestBase(const LayerTreeSettings& settings =
                                      CommitToPendingTreeLayerListSettings())
      : LayerTreeImplTestBase(settings) {}

  static void SetScrollOffsetDelta(LayerImpl* layer_impl,
                                   const gfx::Vector2dF& delta) {
    if (layer_impl->layer_tree_impl()
            ->property_trees()
            ->scroll_tree_mutable()
            .SetScrollOffsetDeltaForTesting(layer_impl->element_id(), delta))
      layer_impl->layer_tree_impl()->DidUpdateScrollOffset(
          layer_impl->element_id(), /*pushed_from_main_or_pending_tree=*/false);
  }

  static float MaximumAnimationToScreenScale(LayerImpl* layer_impl) {
    return layer_impl->layer_tree_impl()
        ->property_trees()
        ->MaximumAnimationToScreenScale(layer_impl->transform_tree_index());
  }
  static bool AnimationAffectedByInvalidScale(LayerImpl* layer_impl) {
    return layer_impl->layer_tree_impl()
        ->property_trees()
        ->AnimationAffectedByInvalidScale(layer_impl->transform_tree_index());
  }

  void UpdateMainDrawProperties(float device_scale_factor = 1.0f) {
    SetDeviceScaleAndUpdateViewportRect(host(), device_scale_factor);
    UpdateDrawProperties(host(), &update_layer_list_);
  }

  LayerImpl* ImplOf(const scoped_refptr<Layer>& layer) {
    return layer ? host_impl()->active_tree()->LayerById(layer->id()) : nullptr;
  }
  LayerImpl* PendingImplOf(const scoped_refptr<Layer>& layer) {
    return layer ? host_impl()->pending_tree()->LayerById(layer->id())
                 : nullptr;
  }
  RenderSurfaceImpl* GetRenderSurfaceImpl(const scoped_refptr<Layer>& layer) {
    return GetRenderSurface(ImplOf(layer));
  }

  // Updates main thread draw properties, commits main thread tree to
  // impl-side pending tree, and updates pending tree draw properties.
  void Commit(float device_scale_factor = 1.0f) {
    UpdateMainDrawProperties(device_scale_factor);
    if (!host_impl()->pending_tree())
      host_impl()->CreatePendingTree();
    host()->CommitToPendingTree();
    // TODO(crbug.com/40617417) This call should be handled by
    // FakeLayerTreeHost instead of manually pushing the properties from the
    // layer tree host to the pending tree.
    host_impl()->pending_tree()->PullLayerTreePropertiesFrom(
        *host()->GetPendingCommitState());

    UpdateDrawProperties(host_impl()->pending_tree());
  }

  // Calls Commit(), then activates the pending tree, and updates active tree
  // draw properties.
  void CommitAndActivate(float device_scale_factor = 1.0f) {
    Commit(device_scale_factor);
    host_impl()->ActivateSyncTree();
    DCHECK_EQ(device_scale_factor,
              host_impl()->active_tree()->device_scale_factor());
    UpdateActiveTreeDrawProperties(device_scale_factor);
  }

  bool UpdateLayerListContains(int id) const {
    for (const auto& layer : update_layer_list_) {
      if (layer->id() == id)
        return true;
    }
    return false;
  }

  const LayerList& update_layer_list() const { return update_layer_list_; }

  const RenderSurfaceList& GetRenderSurfaceList() {
    return host_impl()->active_tree()->GetRenderSurfaceList();
  }

  void SetDeviceTransform(const gfx::Transform& device_transform) {
    host_impl()->OnDraw(device_transform, host_impl()->external_viewport(),
                        false, false);
  }

 private:
  LayerList update_layer_list_;
};

class DrawPropertiesTest : public DrawPropertiesTestBase,
                           public testing::Test {};

class DrawPropertiesTestWithLayerTree : public DrawPropertiesTestBase,
                                        public testing::Test {
 public:
  DrawPropertiesTestWithLayerTree()
      : DrawPropertiesTestBase(CommitToPendingTreeLayerTreeSettings()) {}
};

class DrawPropertiesDrawRectsTest : public DrawPropertiesTest {
 public:
  DrawPropertiesDrawRectsTest() : DrawPropertiesTest() {}

  void SetUp() override {
    LayerImpl* root = root_layer();
    root->SetDrawsContent(true);
    root->SetBounds(gfx::Size(500, 500));
  }

  LayerImpl* TestVisibleRectAndDrawableContentRect(
      const gfx::Rect& target_rect,
      const gfx::Transform& layer_transform,
      const gfx::Rect& layer_rect) {
    LayerImpl* root = root_layer();
    LayerImpl* target = AddLayerInActiveTree<LayerImpl>();
    LayerImpl* drawing_layer = AddLayerInActiveTree<LayerImpl>();

    target->SetDrawsContent(true);
    drawing_layer->SetDrawsContent(true);

    target->SetBounds(target_rect.size());
    drawing_layer->SetBounds(layer_rect.size());

    CopyProperties(root, target);
    CreateTransformNode(target).post_translation =
        gfx::PointF(target_rect.origin()).OffsetFromOrigin();
    CreateEffectNode(target).render_surface_reason = RenderSurfaceReason::kTest;
    CreateClipNode(target);
    CopyProperties(target, drawing_layer);
    auto& drawing_layer_transform_node = CreateTransformNode(drawing_layer);
    drawing_layer_transform_node.local = layer_transform;
    drawing_layer_transform_node.post_translation =
        gfx::PointF(layer_rect.origin()).OffsetFromOrigin();
    drawing_layer_transform_node.flattens_inherited_transform = false;

    UpdateActiveTreeDrawProperties();

    return drawing_layer;
  }
};

// Sanity check: For layers positioned at zero, with zero size,
// and with identity transforms, then the draw transform,
// screen space transform, and the hierarchy passed on to children
// layers should also be identity transforms.
TEST_F(DrawPropertiesTest, TransformsForNoOpLayer) {
  LayerImpl* parent = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();

  parent->SetBounds(gfx::Size(100, 100));

  CopyProperties(parent, child);
  CopyProperties(child, grand_child);

  UpdateActiveTreeDrawProperties();

  EXPECT_TRANSFORM_EQ(gfx::Transform(), child->DrawTransform());
  EXPECT_TRANSFORM_EQ(gfx::Transform(), child->ScreenSpaceTransform());
  EXPECT_TRANSFORM_EQ(gfx::Transform(), grand_child->DrawTransform());
  EXPECT_TRANSFORM_EQ(gfx::Transform(), grand_child->ScreenSpaceTransform());
}

TEST_F(DrawPropertiesTest, TransformsForSingleLayer) {
  LayerImpl* root = root_layer();
  LayerImpl* layer = AddLayerInActiveTree<LayerImpl>();

  TransformTree& transform_tree =
      host_impl()->active_tree()->property_trees()->transform_tree_mutable();
  EffectTree& effect_tree =
      host_impl()->active_tree()->property_trees()->effect_tree_mutable();

  root->SetBounds(gfx::Size(1, 2));
  CopyProperties(root, layer);

  // Case 1: Setting the bounds of the layer should not affect either the draw
  // transform or the screenspace transform.
  layer->SetBounds(gfx::Size(10, 12));
  UpdateActiveTreeDrawProperties();
  EXPECT_TRANSFORM_EQ(
      gfx::Transform(),
      draw_property_utils::DrawTransform(layer, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      gfx::Transform(),
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));

  // Case 2: The anchor point by itself (without a layer transform) should have
  // no effect on the transforms.
  CreateTransformNode(layer).origin = gfx::Point3F(2.5f, 3.0f, 0.f);
  layer->SetBounds(gfx::Size(10, 12));
  UpdateActiveTreeDrawProperties();
  EXPECT_TRANSFORM_EQ(
      gfx::Transform(),
      draw_property_utils::DrawTransform(layer, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      gfx::Transform(),
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));

  // Case 3: A change in actual position affects both the draw transform and
  // screen space transform.
  gfx::Transform position_transform;
  position_transform.Translate(0.f, 1.2f);
  SetPostTranslation(layer, gfx::Vector2dF(0.f, 1.2f));
  UpdateActiveTreeDrawProperties();
  EXPECT_TRANSFORM_EQ(
      position_transform,
      draw_property_utils::DrawTransform(layer, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      position_transform,
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));

  // Case 4: In the correct sequence of transforms, the layer transform should
  // pre-multiply the translation-to-center. This is easily tested by using a
  // scale transform, because scale and translation are not commutative.
  gfx::Transform layer_transform;
  layer_transform.Scale3d(2.0, 2.0, 1.0);
  SetTransform(layer, layer_transform);
  SetTransformOrigin(layer, gfx::Point3F());
  SetPostTranslation(layer, gfx::Vector2dF());
  UpdateActiveTreeDrawProperties();
  EXPECT_TRANSFORM_EQ(layer_transform, draw_property_utils::DrawTransform(
                                           layer, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      layer_transform,
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));

  // Case 5: The layer transform should occur with respect to the anchor point.
  gfx::Transform translation_to_anchor;
  translation_to_anchor.Translate(5.0, 0.0);
  gfx::Transform expected_result = translation_to_anchor * layer_transform *
                                   translation_to_anchor.GetCheckedInverse();
  SetTransformOrigin(layer, gfx::Point3F(5.f, 0.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_TRANSFORM_EQ(expected_result, draw_property_utils::DrawTransform(
                                           layer, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      expected_result,
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));

  // Case 6: Verify that position pre-multiplies the layer transform.  The
  // current implementation of CalculateDrawProperties does this implicitly, but
  // it is still worth testing to detect accidental regressions.
  expected_result = position_transform * translation_to_anchor *
                    layer_transform * translation_to_anchor.GetCheckedInverse();
  SetPostTranslation(layer, gfx::Vector2dF(0.f, 1.2f));
  UpdateActiveTreeDrawProperties();
  EXPECT_TRANSFORM_EQ(expected_result, draw_property_utils::DrawTransform(
                                           layer, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      expected_result,
      draw_property_utils::ScreenSpaceTransform(layer, transform_tree));
}

TEST_F(DrawPropertiesTest, TransformsAboutScrollOffset) {
  const gfx::PointF kScrollOffset(50, 100);
  const gfx::Vector2dF kScrollDelta(2.34f, 5.67f);
  const gfx::Vector2d kMaxScrollOffset(200, 200);
  const gfx::PointF kScrollLayerPosition(-kScrollOffset.x(),
                                         -kScrollOffset.y());
  float page_scale = 0.888f;
  const float kDeviceScale = 1.666f;

  LayerImpl* sublayer = AddLayerInActiveTree<LayerImpl>();
  sublayer->SetDrawsContent(true);
  sublayer->SetBounds(gfx::Size(500, 500));

  LayerImpl* scroll_layer = AddLayerInActiveTree<LayerImpl>();
  scroll_layer->SetBounds(sublayer->bounds());
  scroll_layer->SetElementId(LayerIdToElementIdForTesting(scroll_layer->id()));

  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(3, 4));
  SetupViewport(root, gfx::Size(3, 4), gfx::Size(500, 500));

  CopyProperties(OuterViewportScrollLayer(), scroll_layer);
  CreateTransformNode(scroll_layer);
  CreateScrollNode(
      scroll_layer,
      gfx::Size(scroll_layer->bounds().width() - kMaxScrollOffset.x(),
                scroll_layer->bounds().height() - kMaxScrollOffset.y()));
  CopyProperties(scroll_layer, sublayer);

  auto& scroll_tree = GetPropertyTrees(scroll_layer)->scroll_tree_mutable();
  scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                               kScrollOffset);
  SetScrollOffsetDelta(scroll_layer, kScrollDelta);
  host_impl()->active_tree()->SetPageScaleOnActiveTree(page_scale);
  UpdateActiveTreeDrawProperties(kDeviceScale);

  gfx::Transform expected_transform;
  gfx::PointF sub_layer_screen_position = kScrollLayerPosition - kScrollDelta;
  expected_transform.Translate(
      std::round(sub_layer_screen_position.x() * page_scale * kDeviceScale),
      std::round(sub_layer_screen_position.y() * page_scale * kDeviceScale));
  expected_transform.Scale(page_scale * kDeviceScale,
                           page_scale * kDeviceScale);
  EXPECT_TRANSFORM_EQ(expected_transform, sublayer->DrawTransform());
  EXPECT_TRANSFORM_EQ(expected_transform, sublayer->ScreenSpaceTransform());

  gfx::Transform arbitrary_translate;
  const float kTranslateX = 10.6f;
  const float kTranslateY = 20.6f;
  arbitrary_translate.Translate(kTranslateX, kTranslateY);
  SetTransform(scroll_layer, arbitrary_translate);
  UpdateActiveTreeDrawProperties(kDeviceScale);
  expected_transform.MakeIdentity();
  expected_transform.Translate(
      std::round(kTranslateX * page_scale * kDeviceScale +
                 sub_layer_screen_position.x() * page_scale * kDeviceScale),
      std::round(kTranslateY * page_scale * kDeviceScale +
                 sub_layer_screen_position.y() * page_scale * kDeviceScale));
  expected_transform.Scale(page_scale * kDeviceScale,
                           page_scale * kDeviceScale);
  EXPECT_TRANSFORM_EQ(expected_transform, sublayer->DrawTransform());

  // Test that page scale is updated even when we don't rebuild property trees.
  page_scale = 1.888f;

  host_impl()->active_tree()->SetPageScaleOnActiveTree(page_scale);
  EXPECT_FALSE(host_impl()->active_tree()->property_trees()->needs_rebuild());
  UpdateActiveTreeDrawProperties(kDeviceScale);

  expected_transform.MakeIdentity();
  expected_transform.Translate(
      std::round(kTranslateX * page_scale * kDeviceScale +
                 sub_layer_screen_position.x() * page_scale * kDeviceScale),
      std::round(kTranslateY * page_scale * kDeviceScale +
                 sub_layer_screen_position.y() * page_scale * kDeviceScale));
  expected_transform.Scale(page_scale * kDeviceScale,
                           page_scale * kDeviceScale);
  EXPECT_TRANSFORM_EQ(expected_transform, sublayer->DrawTransform());
}

TEST_F(DrawPropertiesTest, TransformsForSimpleHierarchy) {
  LayerImpl* root = root_layer();
  LayerImpl* parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();

  // One-time setup of root layer
  root->SetBounds(gfx::Size(1, 2));

  TransformTree& transform_tree =
      host_impl()->active_tree()->property_trees()->transform_tree_mutable();
  EffectTree& effect_tree =
      host_impl()->active_tree()->property_trees()->effect_tree_mutable();

  // Case 1: parent's anchor point should not affect child or grand_child.
  parent->SetBounds(gfx::Size(10, 12));
  child->SetBounds(gfx::Size(16, 18));
  grand_child->SetBounds(gfx::Size(76, 78));

  CopyProperties(root, parent);
  CreateTransformNode(parent).origin = gfx::Point3F(2.5f, 3.0f, 0.f);
  CopyProperties(parent, child);
  CopyProperties(child, grand_child);

  UpdateActiveTreeDrawProperties();

  EXPECT_TRANSFORM_EQ(
      gfx::Transform(),
      draw_property_utils::DrawTransform(child, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      gfx::Transform(),
      draw_property_utils::ScreenSpaceTransform(child, transform_tree));
  EXPECT_TRANSFORM_EQ(gfx::Transform(),
                      draw_property_utils::DrawTransform(
                          grand_child, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      gfx::Transform(),
      draw_property_utils::ScreenSpaceTransform(grand_child, transform_tree));

  // Case 2: parent's position affects child and grand_child.
  gfx::Transform parent_position_transform;
  parent_position_transform.Translate(0.f, 1.2f);
  SetPostTranslation(parent, gfx::Vector2dF(0.f, 1.2f));
  UpdateActiveTreeDrawProperties();
  EXPECT_TRANSFORM_EQ(
      parent_position_transform,
      draw_property_utils::DrawTransform(child, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      parent_position_transform,
      draw_property_utils::ScreenSpaceTransform(child, transform_tree));
  EXPECT_TRANSFORM_EQ(parent_position_transform,
                      draw_property_utils::DrawTransform(
                          grand_child, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      parent_position_transform,
      draw_property_utils::ScreenSpaceTransform(grand_child, transform_tree));

  // Case 3: parent's local transform affects child and grandchild
  gfx::Transform parent_layer_transform;
  parent_layer_transform.Scale3d(2.0, 2.0, 1.0);
  gfx::Transform parent_translation_to_anchor;
  parent_translation_to_anchor.Translate(2.5, 3.0);
  gfx::Transform parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      parent_translation_to_anchor.GetCheckedInverse();
  SetTransform(parent, parent_layer_transform);
  SetPostTranslation(parent, gfx::Vector2dF());
  UpdateActiveTreeDrawProperties();
  EXPECT_TRANSFORM_EQ(
      parent_composite_transform,
      draw_property_utils::DrawTransform(child, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      parent_composite_transform,
      draw_property_utils::ScreenSpaceTransform(child, transform_tree));
  EXPECT_TRANSFORM_EQ(parent_composite_transform,
                      draw_property_utils::DrawTransform(
                          grand_child, transform_tree, effect_tree));
  EXPECT_TRANSFORM_EQ(
      parent_composite_transform,
      draw_property_utils::ScreenSpaceTransform(grand_child, transform_tree));
}

TEST_F(DrawPropertiesTest, TransformsForSingleRenderSurface) {
  LayerImpl* root = root_layer();
  LayerImpl* parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform parent_layer_transform;
  parent_layer_transform.Scale3d(1.f, 0.9f, 1.f);
  gfx::Transform parent_translation_to_anchor;
  parent_translation_to_anchor.Translate(25.0, 30.0);

  gfx::Transform parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      parent_translation_to_anchor.GetCheckedInverse();
  gfx::Vector2dF parent_composite_scale =
      gfx::ComputeTransform2dScaleComponents(parent_composite_transform, 1.f);
  gfx::Transform surface_sublayer_transform;
  surface_sublayer_transform.Scale(parent_composite_scale.x(),
                                   parent_composite_scale.y());
  gfx::Transform surface_sublayer_composite_transform =
      parent_composite_transform *
      surface_sublayer_transform.GetCheckedInverse();

  root->SetBounds(gfx::Size(1, 2));
  parent->SetBounds(gfx::Size(100, 120));
  child->SetBounds(gfx::Size(16, 18));
  grand_child->SetBounds(gfx::Size(8, 10));
  grand_child->SetDrawsContent(true);

  CopyProperties(root, parent);
  auto& parent_transform_node = CreateTransformNode(parent);
  parent_transform_node.origin = gfx::Point3F(2.5f, 30.f, 0.f);
  parent_transform_node.local = parent_layer_transform;
  CopyProperties(parent, child);
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(child, grand_child);

  UpdateActiveTreeDrawProperties();

  // Render surface should have been created now.
  ASSERT_TRUE(GetRenderSurface(child));
  ASSERT_EQ(GetRenderSurface(child), child->render_target());

  // The child layer's draw transform should refer to its new render surface.
  // The screen-space transform, however, should still refer to the root.
  EXPECT_TRANSFORM_EQ(surface_sublayer_transform, child->DrawTransform());
  EXPECT_TRANSFORM_EQ(parent_composite_transform,
                      child->ScreenSpaceTransform());

  // Because the grand_child is the only drawable content, the child's render
  // surface will tighten its bounds to the grand_child.  The scale at which the
  // surface's subtree is drawn must be removed from the composite transform.
  EXPECT_TRANSFORM_EQ(surface_sublayer_composite_transform,
                      child->render_target()->draw_transform());

  // The screen space is the same as the target since the child surface draws
  // into the root.
  EXPECT_TRANSFORM_EQ(surface_sublayer_composite_transform,
                      child->render_target()->screen_space_transform());
}

TEST_F(DrawPropertiesTest, TransformsForRenderSurfaceHierarchy) {
  // This test creates a more complex tree and verifies it all at once. This
  // covers the following cases:
  //   - layers that are described w.r.t. a render surface: should have draw
  //   transforms described w.r.t. that surface
  //   - A render surface described w.r.t. an ancestor render surface: should
  //   have a draw transform described w.r.t. that ancestor surface
  //   - Sanity check on recursion: verify transforms of layers described w.r.t.
  //   a render surface that is described w.r.t. an ancestor render surface.
  //   - verifying that each layer has a reference to the correct render surface
  //   and render target values.

  LayerImpl* root = root_layer();
  LayerImpl* parent = AddLayerInActiveTree<LayerImpl>();
  parent->SetDrawsContent(true);
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  render_surface1->SetDrawsContent(true);
  LayerImpl* render_surface2 = AddLayerInActiveTree<LayerImpl>();
  render_surface2->SetDrawsContent(true);
  LayerImpl* child_of_root = AddLayerInActiveTree<LayerImpl>();
  child_of_root->SetDrawsContent(true);
  LayerImpl* child_of_rs1 = AddLayerInActiveTree<LayerImpl>();
  child_of_rs1->SetDrawsContent(true);
  LayerImpl* child_of_rs2 = AddLayerInActiveTree<LayerImpl>();
  child_of_rs2->SetDrawsContent(true);
  LayerImpl* grand_child_of_root = AddLayerInActiveTree<LayerImpl>();
  grand_child_of_root->SetDrawsContent(true);
  LayerImpl* grand_child_of_rs1 = AddLayerInActiveTree<LayerImpl>();
  grand_child_of_rs1->SetDrawsContent(true);
  LayerImpl* grand_child_of_rs2 = AddLayerInActiveTree<LayerImpl>();
  grand_child_of_rs2->SetDrawsContent(true);

  // All layers in the tree are initialized with an anchor at .25 and a size of
  // (10,10).  Matrix "A" is the composite layer transform used in all layers.
  gfx::Transform translation_to_anchor;
  translation_to_anchor.Translate(2.5, 0.0);
  gfx::Transform layer_transform;
  layer_transform.Translate(1.0, 1.0);

  gfx::Transform A = translation_to_anchor * layer_transform *
                     translation_to_anchor.GetCheckedInverse();

  gfx::Vector2dF surface1_parent_transform_scale =
      gfx::ComputeTransform2dScaleComponents(A, 1.f);
  gfx::Transform surface1_sublayer_transform;
  surface1_sublayer_transform.Scale(surface1_parent_transform_scale.x(),
                                    surface1_parent_transform_scale.y());

  // SS1 = transform given to the subtree of render_surface1
  gfx::Transform SS1 = surface1_sublayer_transform;
  // S1 = transform to move from render_surface1 pixels to the layer space of
  // the owning layer
  gfx::Transform S1 = surface1_sublayer_transform.GetCheckedInverse();

  gfx::Vector2dF surface2_parent_transform_scale =
      gfx::ComputeTransform2dScaleComponents(SS1 * A, 1.f);
  gfx::Transform surface2_sublayer_transform;
  surface2_sublayer_transform.Scale(surface2_parent_transform_scale.x(),
                                    surface2_parent_transform_scale.y());

  // SS2 = transform given to the subtree of render_surface2
  gfx::Transform SS2 = surface2_sublayer_transform;
  // S2 = transform to move from render_surface2 pixels to the layer space of
  // the owning layer
  gfx::Transform S2 = surface2_sublayer_transform.GetCheckedInverse();

  root->SetBounds(gfx::Size(1, 2));
  parent->SetBounds(gfx::Size(10, 10));
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface2->SetBounds(gfx::Size(10, 10));
  child_of_root->SetBounds(gfx::Size(10, 10));
  child_of_rs1->SetBounds(gfx::Size(10, 10));
  child_of_rs2->SetBounds(gfx::Size(10, 10));
  grand_child_of_root->SetBounds(gfx::Size(10, 10));
  grand_child_of_rs1->SetBounds(gfx::Size(10, 10));
  grand_child_of_rs2->SetBounds(gfx::Size(10, 10));

  CopyProperties(root, parent);
  auto& parent_transform_node = CreateTransformNode(parent);
  parent_transform_node.origin = gfx::Point3F(2.5f, 0.f, 0.f);
  parent_transform_node.local = layer_transform;
  CopyProperties(parent, child_of_root);
  auto& child_of_root_transform_node = CreateTransformNode(child_of_root);
  child_of_root_transform_node.origin = gfx::Point3F(2.5f, 0.f, 0.f);
  child_of_root_transform_node.local = layer_transform;
  CopyProperties(child_of_root, grand_child_of_root);
  auto& grand_child_of_root_transform_node =
      CreateTransformNode(grand_child_of_root);
  grand_child_of_root_transform_node.origin = gfx::Point3F(2.5f, 0.f, 0.f);
  grand_child_of_root_transform_node.local = layer_transform;
  CopyProperties(parent, render_surface1);
  auto& render_surface1_transform_node = CreateTransformNode(render_surface1);
  render_surface1_transform_node.origin = gfx::Point3F(2.5f, 0.f, 0.f);
  render_surface1_transform_node.local = layer_transform;
  auto& render_surface1_effect_node = CreateEffectNode(render_surface1);
  render_surface1_effect_node.render_surface_reason =
      RenderSurfaceReason::kTest;
  render_surface1_effect_node.opacity = 0.5f;
  CopyProperties(render_surface1, child_of_rs1);
  auto& child_of_rs1_transform_node = CreateTransformNode(child_of_rs1);
  child_of_rs1_transform_node.origin = gfx::Point3F(2.5f, 0.f, 0.f);
  child_of_rs1_transform_node.local = layer_transform;
  CopyProperties(child_of_rs1, grand_child_of_rs1);
  auto& grand_child_of_rs1_transform_node =
      CreateTransformNode(grand_child_of_rs1);
  grand_child_of_rs1_transform_node.origin = gfx::Point3F(2.5f, 0.f, 0.f);
  grand_child_of_rs1_transform_node.local = layer_transform;
  CopyProperties(render_surface1, render_surface2);
  auto& render_surface2_transform_node = CreateTransformNode(render_surface2);
  render_surface2_transform_node.origin = gfx::Point3F(2.5f, 0.f, 0.f);
  render_surface2_transform_node.local = layer_transform;
  auto& render_surface2_effect_node = CreateEffectNode(render_surface2);
  render_surface2_effect_node.render_surface_reason =
      RenderSurfaceReason::kTest;
  render_surface2_effect_node.opacity = 0.33f;
  CopyProperties(render_surface2, child_of_rs2);
  auto& child_of_rs2_transform_node = CreateTransformNode(child_of_rs2);
  child_of_rs2_transform_node.origin = gfx::Point3F(2.5f, 0.f, 0.f);
  child_of_rs2_transform_node.local = layer_transform;
  CopyProperties(child_of_rs2, grand_child_of_rs2);
  auto& grand_child_of_rs2_transform_node =
      CreateTransformNode(grand_child_of_rs2);
  grand_child_of_rs2_transform_node.origin = gfx::Point3F(2.5f, 0.f, 0.f);
  grand_child_of_rs2_transform_node.local = layer_transform;

  UpdateActiveTreeDrawProperties();

  // Only layers that are associated with render surfaces should have an actual
  // RenderSurface() value.
  ASSERT_TRUE(GetRenderSurface(root));
  ASSERT_EQ(GetRenderSurface(child_of_root), GetRenderSurface(root));
  ASSERT_EQ(GetRenderSurface(grand_child_of_root), GetRenderSurface(root));

  ASSERT_NE(GetRenderSurface(render_surface1), GetRenderSurface(root));
  ASSERT_EQ(GetRenderSurface(child_of_rs1), GetRenderSurface(render_surface1));
  ASSERT_EQ(GetRenderSurface(grand_child_of_rs1),
            GetRenderSurface(render_surface1));

  ASSERT_NE(GetRenderSurface(render_surface2), GetRenderSurface(root));
  ASSERT_NE(GetRenderSurface(render_surface2),
            GetRenderSurface(render_surface1));
  ASSERT_EQ(GetRenderSurface(child_of_rs2), GetRenderSurface(render_surface2));
  ASSERT_EQ(GetRenderSurface(grand_child_of_rs2),
            GetRenderSurface(render_surface2));

  // Verify all render target accessors
  EXPECT_EQ(GetRenderSurface(root), parent->render_target());
  EXPECT_EQ(GetRenderSurface(root), child_of_root->render_target());
  EXPECT_EQ(GetRenderSurface(root), grand_child_of_root->render_target());

  EXPECT_EQ(GetRenderSurface(render_surface1),
            render_surface1->render_target());
  EXPECT_EQ(GetRenderSurface(render_surface1), child_of_rs1->render_target());
  EXPECT_EQ(GetRenderSurface(render_surface1),
            grand_child_of_rs1->render_target());

  EXPECT_EQ(GetRenderSurface(render_surface2),
            render_surface2->render_target());
  EXPECT_EQ(GetRenderSurface(render_surface2), child_of_rs2->render_target());
  EXPECT_EQ(GetRenderSurface(render_surface2),
            grand_child_of_rs2->render_target());

  // Verify layer draw transforms note that draw transforms are described with
  // respect to the nearest ancestor render surface but screen space transforms
  // are described with respect to the root.
  EXPECT_TRANSFORM_EQ(A, parent->DrawTransform());
  EXPECT_TRANSFORM_EQ(A * A, child_of_root->DrawTransform());
  EXPECT_TRANSFORM_EQ(A * A * A, grand_child_of_root->DrawTransform());

  EXPECT_TRANSFORM_EQ(SS1, render_surface1->DrawTransform());
  EXPECT_TRANSFORM_EQ(SS1 * A, child_of_rs1->DrawTransform());
  EXPECT_TRANSFORM_EQ(SS1 * A * A, grand_child_of_rs1->DrawTransform());

  EXPECT_TRANSFORM_EQ(SS2, render_surface2->DrawTransform());
  EXPECT_TRANSFORM_EQ(SS2 * A, child_of_rs2->DrawTransform());
  EXPECT_TRANSFORM_EQ(SS2 * A * A, grand_child_of_rs2->DrawTransform());

  // Verify layer screen-space transforms
  //
  EXPECT_TRANSFORM_EQ(A, parent->ScreenSpaceTransform());
  EXPECT_TRANSFORM_EQ(A * A, child_of_root->ScreenSpaceTransform());
  EXPECT_TRANSFORM_EQ(A * A * A, grand_child_of_root->ScreenSpaceTransform());

  EXPECT_TRANSFORM_EQ(A * A, render_surface1->ScreenSpaceTransform());
  EXPECT_TRANSFORM_EQ(A * A * A, child_of_rs1->ScreenSpaceTransform());
  EXPECT_TRANSFORM_EQ(A * A * A * A,
                      grand_child_of_rs1->ScreenSpaceTransform());

  EXPECT_TRANSFORM_EQ(A * A * A, render_surface2->ScreenSpaceTransform());
  EXPECT_TRANSFORM_EQ(A * A * A * A, child_of_rs2->ScreenSpaceTransform());
  EXPECT_TRANSFORM_EQ(A * A * A * A * A,
                      grand_child_of_rs2->ScreenSpaceTransform());

  // Verify render surface transforms.
  //
  // Draw transform of render surface 1 is described with respect to root.
  EXPECT_TRANSFORM_EQ(A * A * S1,
                      GetRenderSurface(render_surface1)->draw_transform());
  EXPECT_TRANSFORM_EQ(
      A * A * S1, GetRenderSurface(render_surface1)->screen_space_transform());
  // Draw transform of render surface 2 is described with respect to render
  // surface 1.
  EXPECT_TRANSFORM_EQ(SS1 * A * S2,
                      GetRenderSurface(render_surface2)->draw_transform());
  EXPECT_TRANSFORM_EQ(
      A * A * A * S2,
      GetRenderSurface(render_surface2)->screen_space_transform());

  // Sanity check. If these fail there is probably a bug in the test itself.  It
  // is expected that we correctly set up transforms so that the y-component of
  // the screen-space transform encodes the "depth" of the layer in the tree.
  EXPECT_FLOAT_EQ(1.0, parent->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(2.0, child_of_root->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(3.0, grand_child_of_root->ScreenSpaceTransform().rc(1, 3));

  EXPECT_FLOAT_EQ(2.0, render_surface1->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(3.0, child_of_rs1->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(4.0, grand_child_of_rs1->ScreenSpaceTransform().rc(1, 3));

  EXPECT_FLOAT_EQ(3.0, render_surface2->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(4.0, child_of_rs2->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(5.0, grand_child_of_rs2->ScreenSpaceTransform().rc(1, 3));
}

TEST_F(DrawPropertiesTest, LayerFullyContainedWithinClipInTargetSpace) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform child_transform;
  child_transform.Translate(50.0, 50.0);
  child_transform.RotateAboutZAxis(30.0);

  gfx::Transform grand_child_transform;
  grand_child_transform.RotateAboutYAxis(90.0);

  root->SetBounds(gfx::Size(200, 200));
  child->SetBounds(gfx::Size(10, 10));
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);

  CopyProperties(root, child);
  CreateTransformNode(child).local = child_transform;
  CopyProperties(child, grand_child);
  auto& grand_child_transform_node = CreateTransformNode(grand_child);
  grand_child_transform_node.flattens_inherited_transform = false;
  grand_child_transform_node.local = grand_child_transform;

  UpdateActiveTreeDrawProperties();

  // Mapping grand_child's bounds to screen space produces an empty rect
  // because it is turned sideways.
  EXPECT_EQ(gfx::Rect(), grand_child->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, TransformsForDegenerateIntermediateLayer) {
  // A layer that is empty in one axis, but not the other, was accidentally
  // skipping a necessary translation.  Without that translation, the coordinate
  // space of the layer's draw transform is incorrect.
  //
  // Normally this isn't a problem, because the layer wouldn't be drawn anyway,
  // but if that layer becomes a render surface, then its draw transform is
  // implicitly inherited by the rest of the subtree, which then is positioned
  // incorrectly as a result.

  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();
  grand_child->SetDrawsContent(true);

  root->SetBounds(gfx::Size(100, 100));
  // The child height is zero, but has non-zero width that should be accounted
  // for while computing draw transforms.
  child->SetBounds(gfx::Size(10, 0));
  grand_child->SetBounds(gfx::Size(10, 10));

  CopyProperties(root, child);
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(child, grand_child);

  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(child));
  // This is the real test, the rest are sanity checks.
  EXPECT_TRANSFORM_EQ(gfx::Transform(),
                      GetRenderSurface(child)->draw_transform());
  EXPECT_TRANSFORM_EQ(gfx::Transform(), child->DrawTransform());
  EXPECT_TRANSFORM_EQ(gfx::Transform(), grand_child->DrawTransform());
}

TEST_F(DrawPropertiesTest, RenderSurfaceWithSublayerScale) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform translate;
  translate.Translate3d(5, 5, 5);

  root->SetBounds(gfx::Size(100, 100));
  render_surface->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(100, 100));
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);

  CopyProperties(root, render_surface);
  CreateTransformNode(render_surface).local = translate;
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface, child);
  CreateTransformNode(child).local = translate;
  CopyProperties(child, grand_child);
  CreateTransformNode(grand_child).local = translate;

  // render_surface will have a sublayer scale because of device scale factor.
  float device_scale_factor = 2.0f;
  UpdateActiveTreeDrawProperties(device_scale_factor);

  // Between grand_child and render_surface, we translate by (10, 10) and scale
  // by a factor of 2.
  gfx::Vector2dF expected_translation(20.0f, 20.0f);
  EXPECT_EQ(grand_child->DrawTransform().To2dTranslation(),
            expected_translation);
}

TEST_F(DrawPropertiesTest, TransformAboveRootLayer) {
  // Transformations applied at the root of the tree should be forwarded
  // to child layers instead of applied to the root RenderSurface.
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetDrawsContent(true);
  root->SetBounds(gfx::Size(100, 100));
  child->SetDrawsContent(true);
  child->SetBounds(gfx::Size(100, 100));

  CopyProperties(root, child);
  CreateClipNode(child);

  float device_scale_factor = 1.0f;
  gfx::Transform translate;
  translate.Translate(50, 50);
  {
    SetDeviceTransform(translate);
    UpdateActiveTreeDrawProperties(device_scale_factor);
    EXPECT_TRANSFORM_EQ(translate,
                        root->draw_properties().target_space_transform);
    EXPECT_TRANSFORM_EQ(translate,
                        child->draw_properties().target_space_transform);
    EXPECT_TRANSFORM_EQ(gfx::Transform(),
                        GetRenderSurface(root)->draw_transform());
    EXPECT_TRANSFORM_EQ(translate, child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(50, 50, 100, 100), child->clip_rect());
  }

  gfx::Transform scale;
  scale.Scale(2, 2);
  {
    SetDeviceTransform(scale);
    UpdateActiveTreeDrawProperties(device_scale_factor);
    EXPECT_TRANSFORM_EQ(scale, root->draw_properties().target_space_transform);
    EXPECT_TRANSFORM_EQ(scale, child->draw_properties().target_space_transform);
    EXPECT_TRANSFORM_EQ(gfx::Transform(),
                        GetRenderSurface(root)->draw_transform());
    EXPECT_TRANSFORM_EQ(scale, child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(0, 0, 200, 200), child->clip_rect());
  }

  gfx::Transform rotate;
  rotate.Rotate(2);
  {
    SetDeviceTransform(rotate);
    UpdateActiveTreeDrawProperties(device_scale_factor);
    EXPECT_TRANSFORM_EQ(rotate, root->draw_properties().target_space_transform);
    EXPECT_TRANSFORM_EQ(rotate,
                        child->draw_properties().target_space_transform);
    EXPECT_TRANSFORM_EQ(gfx::Transform(),
                        GetRenderSurface(root)->draw_transform());
    EXPECT_TRANSFORM_EQ(rotate, child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(-4, 0, 104, 104), child->clip_rect());
  }

  gfx::Transform composite;
  composite.PostConcat(translate);
  composite.PostConcat(scale);
  composite.PostConcat(rotate);
  {
    SetDeviceTransform(composite);
    UpdateActiveTreeDrawProperties(device_scale_factor);
    EXPECT_TRANSFORM_EQ(composite,
                        root->draw_properties().target_space_transform);
    EXPECT_TRANSFORM_EQ(composite,
                        child->draw_properties().target_space_transform);
    EXPECT_TRANSFORM_EQ(gfx::Transform(),
                        GetRenderSurface(root)->draw_transform());
    EXPECT_TRANSFORM_EQ(composite, child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(89, 103, 208, 208), child->clip_rect());
  }

  // Verify it composes correctly with device scale.
  device_scale_factor = 1.5f;

  {
    SetDeviceTransform(translate);
    UpdateActiveTreeDrawProperties(device_scale_factor);
    gfx::Transform device_scaled_translate = translate;
    device_scaled_translate.Scale(device_scale_factor, device_scale_factor);
    EXPECT_TRANSFORM_EQ(device_scaled_translate,
                        root->draw_properties().target_space_transform);
    EXPECT_TRANSFORM_EQ(device_scaled_translate,
                        child->draw_properties().target_space_transform);
    EXPECT_TRANSFORM_EQ(gfx::Transform(),
                        GetRenderSurface(root)->draw_transform());
    EXPECT_TRANSFORM_EQ(device_scaled_translate, child->ScreenSpaceTransform());
    EXPECT_EQ(gfx::Rect(50, 50, 150, 150), child->clip_rect());
  }
}

TEST_F(DrawPropertiesTest, DrawableContentRectForReferenceFilter) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(25, 25));
  child->SetDrawsContent(true);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(
      sk_make_sp<OffsetPaintFilter>(50, 50, nullptr)));

  CopyProperties(root, child);
  auto& child_effect_node = CreateEffectNode(child);
  child_effect_node.render_surface_reason = RenderSurfaceReason::kTest;
  child_effect_node.filters = filters;
  auto& child_clip_node = CreateClipNode(child);
  child_clip_node.pixel_moving_filter_id = child_effect_node.id;

  UpdateActiveTreeDrawProperties();

  // The render surface's size should be unaffected by the offset image filter;
  // it need only have a drawable content rect large enough to contain the
  // contents (at the new offset).
  ASSERT_TRUE(GetRenderSurface(child));
  EXPECT_EQ(gfx::RectF(50, 50, 25, 25),
            GetRenderSurface(child)->DrawableContentRect());
}

TEST_F(DrawPropertiesTest, DrawableContentRectForReferenceFilterHighDpi) {
  const float device_scale_factor = 2.0f;

  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(25, 25));
  child->SetDrawsContent(true);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(
      sk_make_sp<OffsetPaintFilter>(50, 50, nullptr)));

  CopyProperties(root, child);
  auto& child_effect_node = CreateEffectNode(child);
  child_effect_node.render_surface_reason = RenderSurfaceReason::kTest;
  child_effect_node.filters = filters;
  auto& child_clip_node = CreateClipNode(child);
  child_clip_node.pixel_moving_filter_id = child_effect_node.id;

  UpdateActiveTreeDrawProperties(device_scale_factor);

  // The render surface's size should be unaffected by the offset image filter;
  // it need only have a drawable content rect large enough to contain the
  // contents (at the new offset). All coordinates should be scaled by 2,
  // corresponding to the device scale factor.
  ASSERT_TRUE(GetRenderSurface(child));
  EXPECT_EQ(gfx::RectF(100, 100, 50, 50),
            GetRenderSurface(child)->DrawableContentRect());
}

TEST_F(DrawPropertiesTest, VisibleLayerRectForBlurFilterUnderClip) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(300, 300));
  child->SetDrawsContent(true);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(10));

  CreateClipNode(root);
  CopyProperties(root, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(-100, -100));
  auto& filter_node = CreateEffectNode(child);
  filter_node.render_surface_reason = RenderSurfaceReason::kFilter;
  filter_node.filters = filters;
  auto& clip_node = CreateClipNode(child);
  clip_node.pixel_moving_filter_id = filter_node.id;

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(70, 70, 160, 160), child->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, VisibleLayerRectForReferenceFilterUnderClip) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(300, 300));
  child->SetDrawsContent(true);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(
      sk_make_sp<OffsetPaintFilter>(50, 50, nullptr)));

  CreateClipNode(root);
  CopyProperties(root, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(-100, -100));
  auto& filter_node = CreateEffectNode(child);
  filter_node.render_surface_reason = RenderSurfaceReason::kFilter;
  filter_node.filters = filters;
  auto& clip_node = CreateClipNode(child);
  clip_node.pixel_moving_filter_id = filter_node.id;

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(100, 100, 150, 150), child->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, RenderSurfaceForBlendMode) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(10, 10));
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  CopyProperties(root, child);
  auto& child_effect_node = CreateEffectNode(child);
  child_effect_node.render_surface_reason = RenderSurfaceReason::kTest;
  child_effect_node.blend_mode = SkBlendMode::kMultiply;
  child_effect_node.opacity = 0.5f;

  UpdateActiveTreeDrawProperties();

  // Since the child layer has a blend mode other than normal, it should get
  // its own render surface.
  ASSERT_TRUE(GetRenderSurface(child));
  EXPECT_EQ(1.0f, child->draw_opacity());
  EXPECT_EQ(0.5f, GetRenderSurface(child)->draw_opacity());
  EXPECT_EQ(SkBlendMode::kMultiply, GetRenderSurface(child)->BlendMode());
}

TEST_F(DrawPropertiesTest, RenderSurfaceDrawOpacity) {
  LayerImpl* root = root_layer();
  LayerImpl* surface1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* not_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* surface2 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(10, 10));
  surface1->SetBounds(gfx::Size(10, 10));
  surface1->SetDrawsContent(true);
  not_surface->SetBounds(gfx::Size(10, 10));
  surface2->SetBounds(gfx::Size(10, 10));
  surface2->SetDrawsContent(true);

  CopyProperties(root, surface1);
  auto& surface1_effect_node = CreateEffectNode(surface1);
  surface1_effect_node.render_surface_reason = RenderSurfaceReason::kTest;
  surface1_effect_node.opacity = 0.5f;
  CopyProperties(surface1, not_surface);
  CreateEffectNode(not_surface).opacity = 0.5f;
  CopyProperties(not_surface, surface2);
  auto& surface2_effect_node = CreateEffectNode(surface2);
  surface2_effect_node.render_surface_reason = RenderSurfaceReason::kTest;
  surface2_effect_node.opacity = 0.5f;

  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(surface1));
  ASSERT_EQ(GetRenderSurface(not_surface), GetRenderSurface(surface1));
  ASSERT_TRUE(GetRenderSurface(surface2));
  EXPECT_EQ(0.5f, GetRenderSurface(surface1)->draw_opacity());
  // surface2's draw opacity should include the opacity of not-surface and
  // itself, but not the opacity of surface1.
  EXPECT_EQ(0.25f, GetRenderSurface(surface2)->draw_opacity());
}

TEST_F(DrawPropertiesTest, ClipRectCullsRenderSurfaces) {
  // The entire subtree of layers that are outside the clip rect should be
  // culled away, and should not affect the GetRenderSurfaceList.
  //
  // The test tree is set up as follows:
  //  - all layers except the leaf_nodes are forced to be a new render surface
  //  that have something to draw.
  //  - parent is a large container layer.
  //  - child has MasksToBounds=true to cause clipping.
  //  - grand_child is positioned outside of the child's bounds
  //  - great_grand_child is also kept outside child's bounds.
  //
  // In this configuration, grand_child and great_grand_child are completely
  // outside the clip rect, and they should never get scheduled on the list of
  // render surfaces.

  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* great_grand_child = AddLayerInActiveTree<LayerImpl>();

  // leaf_node1 ensures that root and child are kept on the
  // GetRenderSurfaceList, even though grand_child and great_grand_child should
  // be clipped.
  LayerImpl* leaf_node1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* leaf_node2 = AddLayerInActiveTree<LayerImpl>();

  SetElementIdsForTesting();

  root->SetBounds(gfx::Size(500, 500));
  child->SetBounds(gfx::Size(20, 20));
  grand_child->SetBounds(gfx::Size(10, 10));
  great_grand_child->SetBounds(gfx::Size(10, 10));
  leaf_node1->SetBounds(gfx::Size(500, 500));
  leaf_node1->SetDrawsContent(true);
  leaf_node1->SetBounds(gfx::Size(20, 20));
  leaf_node2->SetDrawsContent(true);

  CopyProperties(root, child);
  CreateClipNode(child);
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(child, leaf_node1);
  CopyProperties(child, grand_child);
  grand_child->SetOffsetToTransformParent(gfx::Vector2dF(45.f, 45.f));
  CopyProperties(grand_child, great_grand_child);
  great_grand_child->SetOffsetToTransformParent(
      grand_child->offset_to_transform_parent());
  CopyProperties(great_grand_child, leaf_node2);
  leaf_node2->SetOffsetToTransformParent(
      great_grand_child->offset_to_transform_parent());

  UpdateActiveTreeDrawProperties();

  ASSERT_EQ(2U, GetRenderSurfaceList().size());
  EXPECT_EQ(root->element_id(), GetRenderSurfaceList().at(0)->id());
  EXPECT_EQ(child->element_id(), GetRenderSurfaceList().at(1)->id());
}

TEST_F(DrawPropertiesTest, ClipRectCullsSurfaceWithoutVisibleContent) {
  // When a render surface has a clip rect, it is used to clip the content rect
  // of the surface.

  // The test tree is set up as follows:
  //  - root is a container layer that masksToBounds=true to cause clipping.
  //  - child is a render surface, which has a clip rect set to the bounds of
  //  the root.
  //  - grand_child is a render surface, and the only visible content in child.
  //  It is positioned outside of the clip rect from root.

  // In this configuration, grand_child should be outside the clipped
  // content rect of the child, making grand_child not appear in the
  // GetRenderSurfaceList.

  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* leaf_node = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(20, 20));
  grand_child->SetBounds(gfx::Size(10, 10));
  leaf_node->SetBounds(gfx::Size(10, 10));
  leaf_node->SetDrawsContent(true);

  CreateClipNode(root);
  CopyProperties(root, child);
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(child, grand_child);
  grand_child->SetOffsetToTransformParent(gfx::Vector2dF(200.f, 200.f));
  CreateEffectNode(grand_child).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(grand_child, leaf_node);
  leaf_node->SetOffsetToTransformParent(
      grand_child->offset_to_transform_parent());

  UpdateActiveTreeDrawProperties();

  // We should cull child and grand_child from the GetRenderSurfaceList.
  ASSERT_EQ(1U, GetRenderSurfaceList().size());
  EXPECT_EQ(root->element_id(), GetRenderSurfaceList().at(0)->id());
}

TEST_F(DrawPropertiesTest, IsClippedIsSetCorrectlyLayerImpl) {
  // Tests that LayerImpl's IsClipped() property is set to true when:
  //  - the layer clips its subtree, e.g. masks to bounds,
  //  - the layer is clipped by an ancestor that contributes to the same
  //    render target,
  //  - a surface is clipped by an ancestor that contributes to the same
  //    render target.
  //
  // In particular, for a layer that owns a render surface:
  //  - the render surface inherits any clip from ancestors, and does NOT
  //    pass that clipped status to the layer itself.
  //  - but if the layer itself masks to bounds, it is considered clipped
  //    and propagates the clip to the subtree.

  LayerImpl* root = root_layer();
  LayerImpl* parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* leaf_node1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* leaf_node2 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  parent->SetBounds(gfx::Size(100, 100));
  parent->SetDrawsContent(true);
  child1->SetBounds(gfx::Size(100, 100));
  child1->SetDrawsContent(true);
  child2->SetBounds(gfx::Size(100, 100));
  child2->SetDrawsContent(true);
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);
  leaf_node1->SetBounds(gfx::Size(100, 100));
  leaf_node1->SetDrawsContent(true);
  leaf_node2->SetBounds(gfx::Size(100, 100));
  leaf_node2->SetDrawsContent(true);

  CopyProperties(root, parent);
  CopyProperties(parent, child1);
  CopyProperties(child1, grand_child);
  CopyProperties(grand_child, leaf_node1);
  CopyProperties(parent, child2);
  CreateTransformNode(child2);
  CreateEffectNode(child2).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(child2, leaf_node2);

  // Case 1: nothing is clipped except the root render surface.
  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(root));
  ASSERT_TRUE(GetRenderSurface(child2));

  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(GetRenderSurface(root)->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_FALSE(GetRenderSurface(child2)->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_FALSE(leaf_node2->is_clipped());

  // Case 2: parent MasksToBounds, so the parent, child1, and child2's
  // surface are clipped. But layers that contribute to child2's surface are
  // not clipped explicitly because child2's surface already accounts for
  // that clip.
  CreateClipNode(parent);
  child1->SetClipTreeIndex(parent->clip_tree_index());
  grand_child->SetClipTreeIndex(parent->clip_tree_index());
  leaf_node1->SetClipTreeIndex(parent->clip_tree_index());
  child2->SetClipTreeIndex(parent->clip_tree_index());
  GetEffectNode(child2)->clip_id = parent->clip_tree_index();
  leaf_node2->SetClipTreeIndex(parent->clip_tree_index());

  host_impl()->active_tree()->set_needs_update_draw_properties();
  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(root));
  ASSERT_TRUE(GetRenderSurface(child2));

  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(GetRenderSurface(root)->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_TRUE(child1->is_clipped());
  EXPECT_FALSE(child2->is_clipped());
  EXPECT_TRUE(GetRenderSurface(child2)->is_clipped());
  EXPECT_TRUE(grand_child->is_clipped());
  EXPECT_TRUE(leaf_node1->is_clipped());
  EXPECT_FALSE(leaf_node2->is_clipped());

  parent->SetClipTreeIndex(root->clip_tree_index());
  child1->SetClipTreeIndex(root->clip_tree_index());
  grand_child->SetClipTreeIndex(root->clip_tree_index());
  leaf_node1->SetClipTreeIndex(root->clip_tree_index());
  child2->SetClipTreeIndex(root->clip_tree_index());
  GetEffectNode(child2)->clip_id = root->clip_tree_index();
  leaf_node2->SetClipTreeIndex(root->clip_tree_index());

  // Case 3: child2 MasksToBounds. The layer and subtree are clipped, and
  // child2's render surface is not clipped.
  CreateClipNode(child2);
  leaf_node2->SetClipTreeIndex(child2->clip_tree_index());

  host_impl()->active_tree()->set_needs_update_draw_properties();
  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(root));
  ASSERT_TRUE(GetRenderSurface(child2));

  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(GetRenderSurface(root)->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_FALSE(child1->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_FALSE(GetRenderSurface(child2)->is_clipped());
  EXPECT_FALSE(grand_child->is_clipped());
  EXPECT_FALSE(leaf_node1->is_clipped());
  EXPECT_TRUE(leaf_node2->is_clipped());
}

TEST_F(DrawPropertiesTest, UpdateClipRectCorrectly) {
  // Tests that when as long as layer is clipped, it's clip rect is set to
  // correct value.
  LayerImpl* root = root_layer();
  LayerImpl* parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  parent->SetBounds(gfx::Size(100, 100));
  parent->SetDrawsContent(true);
  child->SetBounds(gfx::Size(100, 100));
  child->SetDrawsContent(true);

  CopyProperties(root, parent);
  CopyProperties(parent, child);
  CreateClipNode(child);

  UpdateActiveTreeDrawProperties();

  EXPECT_FALSE(root->is_clipped());
  EXPECT_FALSE(parent->is_clipped());
  EXPECT_TRUE(child->is_clipped());
  EXPECT_EQ(gfx::Rect(100, 100), child->clip_rect());

  CreateClipNode(parent);
  GetClipNode(child)->parent_id = parent->clip_tree_index();
  child->SetOffsetToTransformParent(gfx::Vector2dF(100.f, 100.f));
  GetClipNode(child)->clip += gfx::Vector2dF(100.f, 100.f);

  host_impl()->active_tree()->set_needs_update_draw_properties();
  UpdateActiveTreeDrawProperties();

  EXPECT_FALSE(root->is_clipped());
  EXPECT_TRUE(parent->is_clipped());
  EXPECT_TRUE(child->is_clipped());
  EXPECT_EQ(gfx::Rect(), child->clip_rect());
}

TEST_F(DrawPropertiesTest, DrawableContentRectForLayers) {
  // Verify that layers get the appropriate DrawableContentRect when their
  // parent MasksToBounds is true.
  //
  //   grand_child1 - completely inside the region; DrawableContentRect should
  //   be the layer rect expressed in target space.
  //   grand_child2 - partially clipped but NOT MasksToBounds; the clip rect
  //   will be the intersection of layer bounds and the mask region.
  //   grand_child3 - partially clipped and MasksToBounds; the
  //   DrawableContentRect will still be the intersection of layer bounds and
  //   the mask region.
  //   grand_child4 - outside parent's clip rect; the DrawableContentRect should
  //   be empty.

  LayerImpl* parent = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child3 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child4 = AddLayerInActiveTree<LayerImpl>();

  parent->SetBounds(gfx::Size(500, 500));
  child->SetBounds(gfx::Size(20, 20));
  grand_child1->SetBounds(gfx::Size(10, 10));
  grand_child1->SetDrawsContent(true);
  grand_child2->SetBounds(gfx::Size(10, 10));
  grand_child2->SetDrawsContent(true);
  grand_child3->SetBounds(gfx::Size(10, 10));
  grand_child3->SetDrawsContent(true);
  grand_child4->SetBounds(gfx::Size(10, 10));
  grand_child4->SetDrawsContent(true);

  CopyProperties(parent, child);
  CreateTransformNode(child);
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  CreateClipNode(child);
  CopyProperties(child, grand_child1);
  grand_child1->SetOffsetToTransformParent(gfx::Vector2dF(5.f, 5.f));
  CopyProperties(child, grand_child2);
  grand_child2->SetOffsetToTransformParent(gfx::Vector2dF(15.f, 15.f));
  CopyProperties(child, grand_child3);
  grand_child3->SetOffsetToTransformParent(gfx::Vector2dF(15.f, 15.f));
  CreateClipNode(grand_child3);
  CopyProperties(child, grand_child4);
  grand_child4->SetOffsetToTransformParent(gfx::Vector2dF(45.f, 45.f));

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::Rect(5, 5, 10, 10),
            grand_child1->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(15, 15, 5, 5),
            grand_child3->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(15, 15, 5, 5),
            grand_child3->visible_drawable_content_rect());
  EXPECT_TRUE(grand_child4->visible_drawable_content_rect().IsEmpty());
}

TEST_F(DrawPropertiesTest, ClipRectIsPropagatedCorrectlyToSurfaces) {
  // Verify that render surfaces (and their layers) get the appropriate
  // clip rects when their parent MasksToBounds is true.
  //
  // Layers that own render surfaces (at least for now) do not inherit any
  // clipping; instead the surface will enforce the clip for the entire subtree.
  // They may still have a clip rect of their own layer bounds, however, if
  // MasksToBounds was true.
  LayerImpl* parent = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child3 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child4 = AddLayerInActiveTree<LayerImpl>();
  // The leaf nodes ensure that these grand_children become render surfaces for
  // this test.
  LayerImpl* leaf_node1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* leaf_node2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* leaf_node3 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* leaf_node4 = AddLayerInActiveTree<LayerImpl>();

  parent->SetBounds(gfx::Size(500, 500));
  child->SetBounds(gfx::Size(20, 20));
  grand_child1->SetBounds(gfx::Size(10, 10));
  grand_child2->SetBounds(gfx::Size(10, 10));
  grand_child3->SetBounds(gfx::Size(10, 10));
  grand_child4->SetBounds(gfx::Size(10, 10));
  leaf_node1->SetBounds(gfx::Size(10, 10));
  leaf_node1->SetDrawsContent(true);
  leaf_node2->SetBounds(gfx::Size(10, 10));
  leaf_node2->SetDrawsContent(true);
  leaf_node3->SetBounds(gfx::Size(10, 10));
  leaf_node3->SetDrawsContent(true);
  leaf_node4->SetBounds(gfx::Size(10, 10));
  leaf_node4->SetDrawsContent(true);

  CopyProperties(parent, child);
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  CreateClipNode(child);
  CopyProperties(child, grand_child1);
  CreateTransformNode(grand_child1).post_translation = gfx::Vector2dF(5.f, 5.f);
  CreateEffectNode(grand_child1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(grand_child1, leaf_node1);
  CopyProperties(child, grand_child2);
  CreateTransformNode(grand_child2).post_translation =
      gfx::Vector2dF(15.f, 15.f);
  CreateEffectNode(grand_child2).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(grand_child2, leaf_node2);
  CopyProperties(child, grand_child3);
  CreateTransformNode(grand_child3).post_translation =
      gfx::Vector2dF(15.f, 15.f);
  CreateEffectNode(grand_child3).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(grand_child3);
  CopyProperties(grand_child3, leaf_node3);
  CopyProperties(child, grand_child4);
  CreateTransformNode(grand_child4).post_translation =
      gfx::Vector2dF(45.f, 45.f);
  CreateEffectNode(grand_child4).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(grand_child4);
  CopyProperties(grand_child4, leaf_node4);

  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(grand_child1));
  ASSERT_TRUE(GetRenderSurface(grand_child2));
  ASSERT_TRUE(GetRenderSurface(grand_child3));

  // Surfaces are clipped by their parent, but un-affected by the owning layer's
  // MasksToBounds.
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            GetRenderSurface(grand_child1)->clip_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            GetRenderSurface(grand_child2)->clip_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            GetRenderSurface(grand_child3)->clip_rect());
}

TEST_F(DrawPropertiesTest, AnimationsForRenderSurfaceHierarchy) {
  LayerImpl* root = root_layer();
  LayerImpl* top = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child_of_rs1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child_of_rs1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child_of_rs2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child_of_rs2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child_of_top = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child_of_top = AddLayerInActiveTree<LayerImpl>();
  SetElementIdsForTesting();

  top->SetDrawsContent(true);
  render_surface1->SetDrawsContent(true);
  child_of_rs1->SetDrawsContent(true);
  grand_child_of_rs1->SetDrawsContent(true);
  render_surface2->SetDrawsContent(true);
  child_of_rs2->SetDrawsContent(true);
  grand_child_of_rs2->SetDrawsContent(true);
  child_of_top->SetDrawsContent(true);
  grand_child_of_top->SetDrawsContent(true);

  gfx::Transform layer_transform;
  layer_transform.Translate(1.0, 1.0);

  root->SetBounds(gfx::Size(10, 10));
  top->SetBounds(gfx::Size(10, 10));
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface2->SetBounds(gfx::Size(10, 10));
  child_of_top->SetBounds(gfx::Size(10, 10));
  child_of_rs1->SetBounds(gfx::Size(10, 10));
  child_of_rs2->SetBounds(gfx::Size(10, 10));
  grand_child_of_top->SetBounds(gfx::Size(10, 10));
  grand_child_of_rs1->SetBounds(gfx::Size(10, 10));
  grand_child_of_rs2->SetBounds(gfx::Size(10, 10));

  CopyProperties(root, top);
  CreateTransformNode(top).local = layer_transform;
  CopyProperties(top, render_surface1);
  auto& render_surface1_transform_node = CreateTransformNode(render_surface1);
  render_surface1_transform_node.origin = gfx::Point3F(0.25f, 0.f, 0.f);
  render_surface1_transform_node.post_translation = gfx::Vector2dF(2.5f, 0.f);
  render_surface1_transform_node.local = layer_transform;
  CreateEffectNode(render_surface1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface1, child_of_rs1);
  auto& child_of_rs1_transform_node = CreateTransformNode(child_of_rs1);
  child_of_rs1_transform_node.origin = gfx::Point3F(0.25f, 0.f, 0.f);
  child_of_rs1_transform_node.post_translation = gfx::Vector2dF(2.5f, 0.f);
  child_of_rs1_transform_node.local = layer_transform;
  CopyProperties(child_of_rs1, grand_child_of_rs1);
  auto& grand_child_of_rs1_transform_node =
      CreateTransformNode(grand_child_of_rs1);
  grand_child_of_rs1_transform_node.origin = gfx::Point3F(0.25f, 0.f, 0.f);
  grand_child_of_rs1_transform_node.post_translation =
      gfx::Vector2dF(2.5f, 0.f);
  grand_child_of_rs1_transform_node.local = layer_transform;
  CopyProperties(render_surface1, render_surface2);
  auto& render_surface2_transform_node = CreateTransformNode(render_surface2);
  render_surface2_transform_node.origin = gfx::Point3F(0.25f, 0.f, 0.f);
  render_surface2_transform_node.post_translation = gfx::Vector2dF(2.5f, 0.f);
  render_surface2_transform_node.local = layer_transform;
  CreateEffectNode(render_surface2).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface2, child_of_rs2);
  auto& child_of_rs2_transform_node = CreateTransformNode(child_of_rs2);
  child_of_rs2_transform_node.origin = gfx::Point3F(0.25f, 0.f, 0.f);
  child_of_rs2_transform_node.post_translation = gfx::Vector2dF(2.5f, 0.f);
  child_of_rs2_transform_node.local = layer_transform;
  CopyProperties(child_of_rs2, grand_child_of_rs2);
  auto& grand_child_of_rs2_transform_node =
      CreateTransformNode(grand_child_of_rs2);
  grand_child_of_rs2_transform_node.origin = gfx::Point3F(0.25f, 0.f, 0.f);
  grand_child_of_rs2_transform_node.post_translation =
      gfx::Vector2dF(2.5f, 0.f);
  grand_child_of_rs2_transform_node.local = layer_transform;
  CopyProperties(top, child_of_top);
  auto& child_of_top_transform_node = CreateTransformNode(child_of_top);
  child_of_top_transform_node.origin = gfx::Point3F(0.25f, 0.f, 0.f);
  child_of_top_transform_node.post_translation = gfx::Vector2dF(2.5f, 0.f);
  child_of_top_transform_node.local = layer_transform;
  CopyProperties(child_of_top, grand_child_of_top);
  auto& grand_child_of_top_transform_node =
      CreateTransformNode(grand_child_of_top);
  grand_child_of_top_transform_node.origin = gfx::Point3F(0.25f, 0.f, 0.f);
  grand_child_of_top_transform_node.post_translation =
      gfx::Vector2dF(2.5f, 0.f);
  grand_child_of_top_transform_node.local = layer_transform;

  // Put an animated opacity on the render surface.
  AddOpacityTransitionToElementWithAnimation(
      render_surface1->element_id(), timeline_impl(), 10.0, 1.f, 0.f, false);

  // Also put an animated opacity on a layer without descendants.
  AddOpacityTransitionToElementWithAnimation(
      grand_child_of_top->element_id(), timeline_impl(), 10.0, 1.f, 0.f, false);

  // Put a transform animation on the render surface.
  AddAnimatedTransformToElementWithAnimation(render_surface2->element_id(),
                                             timeline_impl(), 10.0, 30, 0);

  // Also put transform animations on grand_child_of_top, and
  // grand_child_of_rs2
  AddAnimatedTransformToElementWithAnimation(grand_child_of_top->element_id(),
                                             timeline_impl(), 10.0, 30, 0);
  AddAnimatedTransformToElementWithAnimation(grand_child_of_rs2->element_id(),
                                             timeline_impl(), 10.0, 30, 0);

  UpdateActiveTreeDrawProperties();

  // Only layers that are associated with render surfaces should have an actual
  // RenderSurface() value.
  ASSERT_TRUE(GetRenderSurface(root));
  ASSERT_EQ(GetRenderSurface(top), GetRenderSurface(root));
  ASSERT_EQ(GetRenderSurface(child_of_top), GetRenderSurface(root));
  ASSERT_EQ(GetRenderSurface(grand_child_of_top), GetRenderSurface(root));

  ASSERT_NE(GetRenderSurface(render_surface1), GetRenderSurface(root));
  ASSERT_EQ(GetRenderSurface(child_of_rs1), GetRenderSurface(render_surface1));
  ASSERT_EQ(GetRenderSurface(grand_child_of_rs1),
            GetRenderSurface(render_surface1));

  ASSERT_NE(GetRenderSurface(render_surface2), GetRenderSurface(root));
  ASSERT_NE(GetRenderSurface(render_surface2),
            GetRenderSurface(render_surface1));
  ASSERT_EQ(GetRenderSurface(child_of_rs2), GetRenderSurface(render_surface2));
  ASSERT_EQ(GetRenderSurface(grand_child_of_rs2),
            GetRenderSurface(render_surface2));

  // Verify all render target accessors
  EXPECT_EQ(GetRenderSurface(root), root->render_target());
  EXPECT_EQ(GetRenderSurface(root), top->render_target());
  EXPECT_EQ(GetRenderSurface(root), child_of_top->render_target());
  EXPECT_EQ(GetRenderSurface(root), grand_child_of_top->render_target());

  EXPECT_EQ(GetRenderSurface(render_surface1),
            render_surface1->render_target());
  EXPECT_EQ(GetRenderSurface(render_surface1), child_of_rs1->render_target());
  EXPECT_EQ(GetRenderSurface(render_surface1),
            grand_child_of_rs1->render_target());

  EXPECT_EQ(GetRenderSurface(render_surface2),
            render_surface2->render_target());
  EXPECT_EQ(GetRenderSurface(render_surface2), child_of_rs2->render_target());
  EXPECT_EQ(GetRenderSurface(render_surface2),
            grand_child_of_rs2->render_target());

  // Verify screen_space_transform_is_animating values
  EXPECT_FALSE(root->screen_space_transform_is_animating());
  EXPECT_FALSE(child_of_top->screen_space_transform_is_animating());
  EXPECT_TRUE(grand_child_of_top->screen_space_transform_is_animating());
  EXPECT_FALSE(render_surface1->screen_space_transform_is_animating());
  EXPECT_FALSE(child_of_rs1->screen_space_transform_is_animating());
  EXPECT_FALSE(grand_child_of_rs1->screen_space_transform_is_animating());
  EXPECT_TRUE(render_surface2->screen_space_transform_is_animating());
  EXPECT_TRUE(child_of_rs2->screen_space_transform_is_animating());
  EXPECT_TRUE(grand_child_of_rs2->screen_space_transform_is_animating());

  // Sanity check. If these fail there is probably a bug in the test itself.
  // It is expected that we correctly set up transforms so that the y-component
  // of the screen-space transform encodes the "depth" of the layer in the tree.
  EXPECT_FLOAT_EQ(1.0, top->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(2.0, child_of_top->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(3.0, grand_child_of_top->ScreenSpaceTransform().rc(1, 3));

  EXPECT_FLOAT_EQ(2.0, render_surface1->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(3.0, child_of_rs1->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(4.0, grand_child_of_rs1->ScreenSpaceTransform().rc(1, 3));

  EXPECT_FLOAT_EQ(3.0, render_surface2->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(4.0, child_of_rs2->ScreenSpaceTransform().rc(1, 3));
  EXPECT_FLOAT_EQ(5.0, grand_child_of_rs2->ScreenSpaceTransform().rc(1, 3));
}

TEST_F(DrawPropertiesTest, LargeTransforms) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform large_transform;
  large_transform.Scale(SkDoubleToScalar(1e37), SkDoubleToScalar(1e37));

  root->SetBounds(gfx::Size(10, 10));
  child->SetBounds(gfx::Size(10, 10));
  grand_child->SetBounds(gfx::Size(10, 10));
  grand_child->SetDrawsContent(true);

  CopyProperties(root, child);
  CreateTransformNode(child).local = large_transform;
  CopyProperties(child, grand_child);
  CreateTransformNode(grand_child).local = large_transform;

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::Rect(), grand_child->visible_layer_rect());
}

static bool TransformIsAnimating(LayerImpl* layer) {
  MutatorHost* host = layer->layer_tree_impl()->mutator_host();
  return host->IsAnimatingProperty(layer->element_id(),
                                   layer->GetElementTypeForAnimation(),
                                   TargetProperty::TRANSFORM);
}

static bool HasPotentiallyRunningTransformAnimation(LayerImpl* layer) {
  MutatorHost* host = layer->layer_tree_impl()->mutator_host();
  return host->HasPotentiallyRunningAnimationForProperty(
      layer->element_id(), layer->GetElementTypeForAnimation(),
      TargetProperty::TRANSFORM);
}

TEST_F(DrawPropertiesTest,
       ScreenSpaceTransformIsAnimatingWithDelayedAnimation) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* great_grand_child = AddLayerInActiveTree<LayerImpl>();

  root->SetDrawsContent(true);
  child->SetDrawsContent(true);
  grand_child->SetDrawsContent(true);
  great_grand_child->SetDrawsContent(true);

  root->SetBounds(gfx::Size(10, 10));
  child->SetBounds(gfx::Size(10, 10));
  grand_child->SetBounds(gfx::Size(10, 10));
  great_grand_child->SetBounds(gfx::Size(10, 10));

  SetElementIdsForTesting();

  CopyProperties(root, child);
  CopyProperties(child, grand_child);
  CreateTransformNode(grand_child);  // for animation.
  CopyProperties(grand_child, great_grand_child);

  // Add a transform animation with a start delay to |grand_child|.
  std::unique_ptr<KeyframeModel> keyframe_model = KeyframeModel::Create(
      std::unique_ptr<gfx::AnimationCurve>(new FakeTransformTransition(1.0)), 0,
      1, KeyframeModel::TargetPropertyId(TargetProperty::TRANSFORM));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  keyframe_model->set_time_offset(base::Milliseconds(-1000));
  AddKeyframeModelToElementWithAnimation(
      grand_child->element_id(), timeline_impl(), std::move(keyframe_model));

  UpdateActiveTreeDrawProperties();

  EXPECT_FALSE(root->screen_space_transform_is_animating());
  EXPECT_FALSE(child->screen_space_transform_is_animating());

  EXPECT_FALSE(TransformIsAnimating(grand_child));
  EXPECT_TRUE(HasPotentiallyRunningTransformAnimation(grand_child));
  EXPECT_TRUE(grand_child->screen_space_transform_is_animating());
  EXPECT_TRUE(great_grand_child->screen_space_transform_is_animating());
}

// Test visible layer rect and drawable content rect are calculated correctly
// for identity transforms.
TEST_F(DrawPropertiesDrawRectsTest, DrawRectsForIdentityTransform) {
  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Layer is contained within the surface.
  gfx::Rect layer_content_rect = gfx::Rect(10, 10, 30, 30);
  gfx::Rect expected_visible_layer_rect = gfx::Rect(30, 30);
  gfx::Rect expected_drawable_content_rect = gfx::Rect(10, 10, 30, 30);
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());

  // Case 2: Layer is outside the surface rect.
  layer_content_rect = gfx::Rect(120, 120, 30, 30);
  expected_visible_layer_rect = gfx::Rect();
  expected_drawable_content_rect = gfx::Rect();
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());

  // Case 3: Layer is partially overlapping the surface rect.
  layer_content_rect = gfx::Rect(80, 80, 30, 30);
  expected_visible_layer_rect = gfx::Rect(20, 20);
  expected_drawable_content_rect = gfx::Rect(80, 80, 20, 20);
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());
}

// Test visible layer rect and drawable content rect are calculated correctly
// for rotations about z-axis (i.e. 2D rotations).
TEST_F(DrawPropertiesDrawRectsTest, DrawRectsFor2DRotations) {
  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 30, 30);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Layer is contained within the surface.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(50.0, 50.0);
  layer_to_surface_transform.Rotate(45.0);
  gfx::Rect expected_visible_layer_rect = gfx::Rect(30, 30);
  gfx::Rect expected_drawable_content_rect = gfx::Rect(28, 50, 44, 43);
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());

  // Case 2: Layer is outside the surface rect.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(-50.0, 0.0);
  layer_to_surface_transform.Rotate(45.0);
  expected_visible_layer_rect = gfx::Rect();
  expected_drawable_content_rect = gfx::Rect();
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());

  // Case 3: The layer is rotated about its top-left corner. In surface space,
  // the layer is oriented diagonally, with the left half outside of the render
  // surface. In this case, the g should still be the entire layer
  // (remember the g is computed in layer space); both the top-left
  // and bottom-right corners of the layer are still visible.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Rotate(45.0);
  expected_visible_layer_rect = gfx::Rect(30, 30);
  expected_drawable_content_rect = gfx::Rect(22, 43);
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());

  // Case 4: The layer is rotated about its top-left corner, and translated
  // upwards. In surface space, the layer is oriented diagonally, with only the
  // top corner of the surface overlapping the layer. In layer space, the render
  // surface overlaps the right side of the layer. The g should be
  // the layer's right half.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(0.0, -sqrt(2.0) * 15.0);
  layer_to_surface_transform.Rotate(45.0);
  // Right half of layer bounds.
  expected_visible_layer_rect = gfx::Rect(15, 0, 15, 30);
  expected_drawable_content_rect = gfx::Rect(22, 22);
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());
}

// Test visible layer rect and drawable content rect are calculated correctly
// for 3d transforms.
TEST_F(DrawPropertiesDrawRectsTest, DrawRectsFor3dOrthographicTransform) {
  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Orthographic projection of a layer rotated about y-axis by 45
  // degrees, should be fully contained in the render surface.
  // 100 is the un-rotated layer width; divided by sqrt(2) is the rotated width.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  gfx::Rect expected_visible_layer_rect = gfx::Rect(100, 100);
  gfx::Rect expected_drawable_content_rect = gfx::Rect(71, 100);
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());

  // Case 2: Orthographic projection of a layer rotated about y-axis by 45
  // degrees, but shifted to the side so only the right-half the layer would be
  // visible on the surface.
  // 50 is the un-rotated layer width; divided by sqrt(2) is the rotated width.
  SkScalar half_width_of_rotated_layer =
      SkDoubleToScalar((100.0 / sqrt(2.0)) * 0.5);
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(-half_width_of_rotated_layer, 0.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);  // Rotates about the left
                                                      // edge of the layer.
  // Tight half of the layer.
  expected_visible_layer_rect = gfx::Rect(50, 0, 50, 100);
  expected_drawable_content_rect = gfx::Rect(36, 100);
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());
}

// Test visible layer rect and drawable content rect are calculated correctly
// when the layer has a perspective projection onto the target surface.
TEST_F(DrawPropertiesDrawRectsTest, DrawRectsFor3dPerspectiveTransform) {
  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(-50, -50, 200, 200);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Even though the layer is twice as large as the surface, due to
  // perspective foreshortening, the layer will fit fully in the surface when
  // its translated more than the perspective amount.
  layer_to_surface_transform.MakeIdentity();

  // The following sequence of transforms applies the perspective about the
  // center of the surface.
  layer_to_surface_transform.Translate(50.0, 50.0);
  layer_to_surface_transform.ApplyPerspectiveDepth(9.0);
  layer_to_surface_transform.Translate(-50.0, -50.0);

  // This translate places the layer in front of the surface's projection plane.
  layer_to_surface_transform.Translate3d(0.0, 0.0, -27.0);

  // Layer position is (-50, -50), visible rect in layer space is layer bounds
  // offset by layer position.
  gfx::Rect expected_visible_layer_rect = gfx::Rect(50, 50, 150, 150);
  gfx::Rect expected_drawable_content_rect = gfx::Rect(38, 38);
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());

  // Case 2: same projection as before, except that the layer is also translated
  // to the side, so that only the right half of the layer should be visible.
  //
  // Explanation of expected result: The perspective ratio is (z distance
  // between layer and camera origin) / (z distance between projection plane and
  // camera origin) == ((-27 - 9) / 9) Then, by similar triangles, if we want to
  // move a layer by translating -25 units in projected surface units (so that
  // only half of it is visible), then we would need to translate by (-36 / 9) *
  // -25 == -100 in the layer's units.
  layer_to_surface_transform.Translate3d(-100.0, 0.0, 0.0);
  // Visible layer rect is moved by 100, and drawable content rect is in target
  // space and is moved by 25.
  expected_visible_layer_rect = gfx::Rect(150, 50, 50, 150);
  expected_drawable_content_rect = gfx::Rect(13, 38);
  drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());
}

// There is currently no explicit concept of an orthographic projection plane
// in our code (nor in the CSS spec to my knowledge). Therefore, layers that
// are technically behind the surface in an orthographic world should not be
// clipped when they are flattened to the surface.
TEST_F(DrawPropertiesDrawRectsTest,
       DrawRectsFor3dOrthographicIsNotClippedBehindSurface) {
  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // This sequence of transforms effectively rotates the layer about the y-axis
  // at the center of the layer.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(50.0, 0.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  layer_to_surface_transform.Translate(-50.0, 0.0);

  // Layer is rotated about Y Axis, and its width is 100/sqrt(2) in surface
  // space.
  gfx::Rect expected_visible_layer_rect = gfx::Rect(100, 100);
  gfx::Rect expected_drawable_content_rect = gfx::Rect(14, 0, 72, 100);
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());
}

// Test visible layer rect and drawable content rect are calculated correctly
// when projecting a surface onto a layer, but the layer is partially behind
// the camera (not just behind the projection plane). In this case, the
// cartesian coordinates may seem to be valid, but actually they are not. The
// visible rect needs to be properly clipped by the w = 0 plane in homogeneous
// coordinates before converting to cartesian coordinates. The drawable
// content rect would be entire surface rect because layer is rotated at the
// camera position.
TEST_F(DrawPropertiesDrawRectsTest, DrawRectsFor3dPerspectiveWhenClippedByW) {
  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 200, 200);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 20, 2);
  gfx::Transform layer_to_surface_transform;

  // The layer is positioned so that the right half of the layer should be in
  // front of the camera, while the other half is behind the surface's
  // projection plane. The following sequence of transforms applies the
  // perspective and rotation about the center of the layer.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.ApplyPerspectiveDepth(1.0);
  layer_to_surface_transform.Translate3d(10.0, 0.0, 1.0);
  layer_to_surface_transform.RotateAboutYAxis(-45.0);
  layer_to_surface_transform.Translate(-10, -1);

  // Sanity check that this transform does indeed cause w < 0 when applying the
  // transform, otherwise this code is not testing the intended scenario.
  bool clipped;
  MathUtil::MapQuad(layer_to_surface_transform,
                    gfx::QuadF(gfx::RectF(layer_content_rect)), &clipped);
  ASSERT_TRUE(clipped);

  gfx::Rect expected_visible_layer_rect = gfx::Rect(0, 1, 10, 1);
  gfx::Rect expected_drawable_content_rect = target_surface_rect;
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());
}

static bool ProjectionClips(const gfx::Transform& map_transform,
                            const gfx::RectF& mapped_rect) {
  gfx::Transform inverse = map_transform.GetCheckedInverse();
  bool clipped = false;
  if (!clipped)
    MathUtil::ProjectPoint(inverse, mapped_rect.top_right(), &clipped);
  if (!clipped)
    MathUtil::ProjectPoint(inverse, mapped_rect.origin(), &clipped);
  if (!clipped)
    MathUtil::ProjectPoint(inverse, mapped_rect.bottom_right(), &clipped);
  if (!clipped)
    MathUtil::ProjectPoint(inverse, mapped_rect.bottom_left(), &clipped);
  return clipped;
}

// To determine visible rect in layer space, there needs to be an
// un-projection from surface space to layer space. When the original
// transform was a perspective projection that was clipped, it returns a rect
// that encloses the clipped bounds.  Un-projecting this new rect may require
// clipping again.
TEST_F(DrawPropertiesDrawRectsTest, DrawRectsForPerspectiveUnprojection) {
  // This sequence of transforms causes one corner of the layer to protrude
  // across the w = 0 plane, and should be clipped.
  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 150, 150);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 20, 20);
  gfx::Transform layer_to_surface_transform;
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(10, 10);
  layer_to_surface_transform.ApplyPerspectiveDepth(1.0);
  layer_to_surface_transform.Translate3d(0.0, 0.0, -5.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  layer_to_surface_transform.RotateAboutXAxis(80.0);
  layer_to_surface_transform.Translate(-10, -10);

  // Sanity check that un-projection does indeed cause w < 0, otherwise this
  // code is not testing the intended scenario.
  gfx::RectF clipped_rect = MathUtil::MapClippedRect(
      layer_to_surface_transform, gfx::RectF(layer_content_rect));
  ASSERT_TRUE(ProjectionClips(layer_to_surface_transform, clipped_rect));

  // Only the corner of the layer is not visible on the surface because of being
  // clipped. But, the net result of rounding visible region to an axis-aligned
  // rect is that the entire layer should still be considered visible.
  gfx::Rect expected_visible_layer_rect = layer_content_rect;
  gfx::Rect expected_drawable_content_rect = target_surface_rect;
  LayerImpl* drawing_layer = TestVisibleRectAndDrawableContentRect(
      target_surface_rect, layer_to_surface_transform, layer_content_rect);
  EXPECT_EQ(expected_visible_layer_rect, drawing_layer->visible_layer_rect());
  EXPECT_EQ(expected_drawable_content_rect,
            drawing_layer->visible_drawable_content_rect());
}

TEST_F(DrawPropertiesTest, DrawableAndVisibleContentRectsForSimpleLayers) {
  LayerImpl* root = root_layer();
  LayerImpl* child1_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child2_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child3_layer = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child1_layer->SetBounds(gfx::Size(50, 50));
  child1_layer->SetDrawsContent(true);
  child2_layer->SetBounds(gfx::Size(50, 50));
  child2_layer->SetDrawsContent(true);
  child3_layer->SetBounds(gfx::Size(50, 50));
  child3_layer->SetDrawsContent(true);

  CopyProperties(root, child1_layer);
  CopyProperties(root, child2_layer);
  child2_layer->SetOffsetToTransformParent(gfx::Vector2dF(75.f, 75.f));
  CopyProperties(root, child3_layer);
  child3_layer->SetOffsetToTransformParent(gfx::Vector2dF(125.f, 125.f));

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            GetRenderSurface(root)->DrawableContentRect());

  // Layers that do not draw content should have empty visible_layer_rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());

  // layer visible_layer_rects are clipped by their target surface.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1_layer->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), child2_layer->visible_layer_rect());
  EXPECT_TRUE(child3_layer->visible_layer_rect().IsEmpty());

  // layer visible_drawable_content_rects are in target space, but still only
  // the visible part.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50),
            child1_layer->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 25, 25),
            child2_layer->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 0, 0),
            child3_layer->visible_drawable_content_rect());
}

TEST_F(DrawPropertiesTest,
       DrawableAndVisibleContentRectsForLayersClippedByLayer) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child3 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(100, 100));
  grand_child1->SetBounds(gfx::Size(50, 50));
  grand_child1->SetDrawsContent(true);
  grand_child2->SetBounds(gfx::Size(50, 50));
  grand_child2->SetDrawsContent(true);
  grand_child3->SetBounds(gfx::Size(50, 50));
  grand_child3->SetDrawsContent(true);

  CopyProperties(root, child);
  CreateClipNode(child);
  CopyProperties(child, grand_child1);
  grand_child1->SetOffsetToTransformParent(gfx::Vector2dF(5.f, 5.f));
  CopyProperties(child, grand_child2);
  grand_child2->SetOffsetToTransformParent(gfx::Vector2dF(75.f, 75.f));
  CopyProperties(child, grand_child3);
  grand_child3->SetOffsetToTransformParent(gfx::Vector2dF(125.f, 125.f));

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            GetRenderSurface(root)->DrawableContentRect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), child->visible_layer_rect());

  // All grandchild visible content rects should be clipped by child.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), grand_child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), grand_child2->visible_layer_rect());
  EXPECT_TRUE(grand_child3->visible_layer_rect().IsEmpty());

  // All grandchild DrawableContentRects should also be clipped by child.
  EXPECT_EQ(gfx::Rect(5, 5, 50, 50),
            grand_child1->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 25, 25),
            grand_child2->visible_drawable_content_rect());
  EXPECT_TRUE(grand_child3->visible_drawable_content_rect().IsEmpty());
}

TEST_F(DrawPropertiesTest, VisibleContentRectWithClippingAndScaling) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform child_scale_matrix;
  child_scale_matrix.Scale(0.25f, 0.25f);
  gfx::Transform grand_child_scale_matrix;
  grand_child_scale_matrix.Scale(0.246f, 0.246f);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);

  CopyProperties(root, child);
  CreateTransformNode(child).local = child_scale_matrix;
  CreateClipNode(child);
  CopyProperties(child, grand_child);
  CreateTransformNode(grand_child).local = grand_child_scale_matrix;

  UpdateActiveTreeDrawProperties();

  // The visible rect is expanded to integer coordinates.
  EXPECT_EQ(gfx::Rect(41, 41), grand_child->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, ClipRectWithClipParent) {
  LayerImpl* root = root_layer();
  LayerImpl* clip = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  CreateClipNode(root);

  clip->SetBounds(gfx::Size(10, 10));
  CopyProperties(root, clip);
  CreateClipNode(clip);

  child1->SetBounds(gfx::Size(20, 20));
  child1->SetDrawsContent(true);
  CopyProperties(clip, child1);
  child1->SetClipTreeIndex(root->clip_tree_index());

  child2->SetBounds(gfx::Size(20, 20));
  child2->SetDrawsContent(true);
  CopyProperties(clip, child2);

  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(child1->is_clipped());
  EXPECT_TRUE(child2->is_clipped());
  EXPECT_EQ(gfx::Rect(100, 100), child1->clip_rect());
  EXPECT_EQ(gfx::Rect(10, 10), child2->clip_rect());
}

TEST_F(DrawPropertiesTest, ClipRectWithClippedDescendantOfFilter) {
  LayerImpl* root = root_layer();
  LayerImpl* filter = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* filter_grand_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  clip->SetBounds(gfx::Size(10, 10));
  filter_grand_child->SetBounds(gfx::Size(20, 20));
  filter_grand_child->SetDrawsContent(true);

  CopyProperties(root, filter);
  CreateEffectNode(filter).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(filter, clip);
  CreateClipNode(clip);
  CopyProperties(clip, filter_grand_child);

  UpdateActiveTreeDrawProperties();
  EXPECT_TRUE(filter_grand_child->is_clipped());
  EXPECT_EQ(gfx::Rect(10, 10), filter_grand_child->clip_rect());

  FilterOperations blur_filter;
  blur_filter.Append(FilterOperation::CreateBlurFilter(4.0f));
  SetFilter(filter, blur_filter);

  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(filter_grand_child->is_clipped());
  EXPECT_EQ(gfx::Rect(10, 10), filter_grand_child->clip_rect());
}

TEST_F(DrawPropertiesTest,
       DrawableAndVisibleContentRectsForLayersInUnclippedRenderSurface) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child3 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  render_surface->SetBounds(gfx::Size(3, 4));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child3->SetBounds(gfx::Size(50, 50));
  child3->SetDrawsContent(true);

  CopyProperties(root, render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface, child1);
  child1->SetOffsetToTransformParent(gfx::Vector2dF(5.f, 5.f));
  CopyProperties(render_surface, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(75.f, 75.f));
  CopyProperties(render_surface, child3);
  child3->SetOffsetToTransformParent(gfx::Vector2dF(125.f, 125.f));

  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(render_surface));

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            GetRenderSurface(root)->DrawableContentRect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface->visible_layer_rect());

  // An unclipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree.
  EXPECT_EQ(gfx::RectF(5.f, 5.f, 95.f, 95.f),
            GetRenderSurface(render_surface)->DrawableContentRect());

  // All layers that draw content into the unclipped surface are also unclipped.
  // Only the viewport clip should apply
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), child3->visible_layer_rect());

  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), child1->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 25, 25), child2->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 0, 0), child3->visible_drawable_content_rect());
}

TEST_F(DrawPropertiesTest, VisibleContentRectsForClippedSurfaceWithEmptyClip) {
  LayerImpl* root = root_layer();
  LayerImpl* child1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child3 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child3->SetBounds(gfx::Size(50, 50));
  child3->SetDrawsContent(true);

  CopyProperties(root, child1);
  child1->SetOffsetToTransformParent(gfx::Vector2dF(5.f, 5.f));
  CopyProperties(root, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(75.f, 75.f));
  CopyProperties(root, child3);
  child3->SetOffsetToTransformParent(gfx::Vector2dF(125.f, 125.f));

  // Now set the root render surface an empty clip.
  // Not using UpdateActiveTreeDrawProperties() because we want a special
  // device viewport rect.
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect());
  UpdateDrawProperties(host_impl()->active_tree());

  ASSERT_TRUE(GetRenderSurface(root));
  EXPECT_FALSE(root->is_clipped());

  gfx::Rect empty;
  EXPECT_EQ(empty, GetRenderSurface(root)->clip_rect());
  EXPECT_TRUE(GetRenderSurface(root)->is_clipped());

  // Visible content rect calculation will check if the target surface is
  // clipped or not. An empty clip rect does not indicate the render surface
  // is unclipped.
  EXPECT_EQ(empty, child1->visible_layer_rect());
  EXPECT_EQ(empty, child2->visible_layer_rect());
  EXPECT_EQ(empty, child3->visible_layer_rect());
}

TEST_F(DrawPropertiesTest,
       DrawableAndVisibleContentRectsForLayersWithUninvertibleTransform) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(50, 50));
  child->SetDrawsContent(true);

  // Case 1: a truly degenerate matrix
  auto uninvertible_matrix = gfx::Transform::MakeScale(0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  CopyProperties(root, child);
  CreateTransformNode(child).local = uninvertible_matrix;

  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child->visible_drawable_content_rect().IsEmpty());

  // Case 2: a matrix with flattened z, uninvertible and not visible according
  // to the CSS spec.
  uninvertible_matrix.MakeIdentity();
  uninvertible_matrix.set_rc(2, 2, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  SetTransform(child, uninvertible_matrix);
  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child->visible_drawable_content_rect().IsEmpty());

  // Case 3: a matrix with flattened z, also uninvertible and not visible.
  uninvertible_matrix.MakeIdentity();
  uninvertible_matrix.Translate(500.0, 0.0);
  uninvertible_matrix.set_rc(2, 2, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  SetTransform(child, uninvertible_matrix);
  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child->visible_drawable_content_rect().IsEmpty());
}

TEST_F(DrawPropertiesTest,
       VisibleContentRectForLayerWithUninvertibleDrawTransform) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform scale = gfx::Transform::MakeScale(1e-15);
  EXPECT_TRUE(scale.IsInvertible());
  EXPECT_FALSE((scale * scale).IsInvertible());

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(100, 100));
  child->SetDrawsContent(true);
  grand_child->SetBounds(gfx::Size(100, 100));
  grand_child->SetDrawsContent(true);

  CopyProperties(root, child);
  auto& child_transform_node = CreateTransformNode(child);
  child_transform_node.flattens_inherited_transform = false;
  child_transform_node.post_translation = gfx::Vector2dF(10.f, 10.f);
  child_transform_node.sorting_context_id = 1;
  child_transform_node.local = scale;
  CopyProperties(child, grand_child);
  auto& grand_child_transform_node = CreateTransformNode(grand_child);
  grand_child_transform_node.flattens_inherited_transform = false;
  grand_child_transform_node.sorting_context_id = 1;
  grand_child_transform_node.local = scale;

  UpdateActiveTreeDrawProperties();

  // Though all layers have invertible transforms, matrix multiplication using
  // floating-point math makes the draw transform uninvertible.
  EXPECT_FALSE(GetTransformNode(grand_child)->ancestors_are_invertible);

  // CalcDrawProps skips a subtree when a layer's screen space transform is
  // uninvertible
  EXPECT_EQ(gfx::Rect(), grand_child->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, ClipExpanderWithUninvertibleTransform) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(50, 50));
  child->SetDrawsContent(true);

  auto uninvertible_matrix = gfx::Transform::MakeScale(0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  CopyProperties(root, child);
  CreateTransformNode(child).local = uninvertible_matrix;
  FilterOperations filters;
  auto& filter_node = CreateEffectNode(child);
  filter_node.render_surface_reason = RenderSurfaceReason::kFilter;
  filter_node.filters.Append(FilterOperation::CreateBlurFilter(10));
  auto& clip_node = CreateClipNode(child);
  clip_node.pixel_moving_filter_id = filter_node.id;

  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child->visible_drawable_content_rect().IsEmpty());
}

// Needs layer tree mode: mask layer.
TEST_F(DrawPropertiesTestWithLayerTree, OcclusionBySiblingOfTarget) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAllowUndamagedNonrootRenderPassToSkip);

  auto root = Layer::Create();
  auto child = Layer::Create();
  FakeContentLayerClient client;
  auto surface = PictureLayer::Create(&client);
  auto surface_child = PictureLayer::Create(&client);
  auto surface_sibling = PictureLayer::Create(&client);
  auto surface_child_mask = PictureLayer::Create(&client);

  surface->SetIsDrawable(true);
  surface_child->SetIsDrawable(true);
  surface_sibling->SetIsDrawable(true);
  surface_child_mask->SetIsDrawable(true);
  surface->SetContentsOpaque(true);
  surface_child->SetContentsOpaque(true);
  surface_sibling->SetContentsOpaque(true);
  surface_child_mask->SetContentsOpaque(true);

  gfx::Transform translate;
  translate.Translate(20.f, 20.f);

  root->SetBounds(gfx::Size(1000, 1000));
  child->SetBounds(gfx::Size(300, 300));
  surface->SetTransform(translate);
  surface->SetBounds(gfx::Size(300, 300));
  surface->SetForceRenderSurfaceForTesting(true);
  surface_child->SetBounds(gfx::Size(300, 300));
  surface_child->SetForceRenderSurfaceForTesting(true);
  surface_sibling->SetBounds(gfx::Size(200, 200));
  surface_child_mask->SetBounds(gfx::Size(300, 300));

  surface_child->SetMaskLayer(surface_child_mask);
  surface->AddChild(surface_child);
  child->AddChild(surface);
  child->AddChild(surface_sibling);
  root->AddChild(child);
  host()->SetRootLayer(root);

  CommitAndActivate();

  EXPECT_TRANSFORM_EQ(GetRenderSurfaceImpl(surface)->draw_transform(),
                      translate);
  // surface_sibling draws into the root render surface and occludes
  // surface_child's contents.
  Occlusion actual_occlusion =
      GetRenderSurfaceImpl(surface_child)->occlusion_in_content_space();
  Occlusion expected_occlusion(translate, SimpleEnclosedRegion(),
                               SimpleEnclosedRegion(gfx::Rect(200, 200)));
  EXPECT_TRUE(expected_occlusion.IsEqual(actual_occlusion));

  // Mask layer's occlusion is different because we create transform and render
  // surface for it in layer tree mode.
  actual_occlusion =
      ImplOf(surface_child_mask)->draw_properties().occlusion_in_content_space;
  expected_occlusion = Occlusion(
      gfx::Transform(), SimpleEnclosedRegion(gfx::Rect(-20, -20, 200, 200)),
      SimpleEnclosedRegion());
  EXPECT_TRUE(expected_occlusion.IsEqual(actual_occlusion));
}

// Occlusion immune with kAllowUndamagedNonrootRenderPassToSkip enabled.
TEST_F(DrawPropertiesTestWithLayerTree, OcclusionImmuneForSiblingOfTarget) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAllowUndamagedNonrootRenderPassToSkip);

  auto root = Layer::Create();
  auto child = Layer::Create();
  FakeContentLayerClient client;
  auto surface = PictureLayer::Create(&client);
  auto surface_child = PictureLayer::Create(&client);
  auto surface_sibling = PictureLayer::Create(&client);
  auto surface_child_mask = PictureLayer::Create(&client);

  surface->SetIsDrawable(true);
  surface_child->SetIsDrawable(true);
  surface_sibling->SetIsDrawable(true);
  surface_child_mask->SetIsDrawable(true);
  surface->SetContentsOpaque(true);
  surface_child->SetContentsOpaque(true);
  surface_sibling->SetContentsOpaque(true);
  surface_child_mask->SetContentsOpaque(true);

  gfx::Transform translate;
  translate.Translate(20.f, 20.f);

  root->SetBounds(gfx::Size(1000, 1000));
  child->SetBounds(gfx::Size(300, 300));
  surface->SetTransform(translate);
  surface->SetBounds(gfx::Size(300, 300));
  surface->SetForceRenderSurfaceForTesting(true);
  surface_child->SetBounds(gfx::Size(300, 300));
  surface_child->SetForceRenderSurfaceForTesting(true);
  surface_sibling->SetBounds(gfx::Size(200, 200));
  surface_child_mask->SetBounds(gfx::Size(300, 300));

  surface_child->SetMaskLayer(surface_child_mask);
  surface->AddChild(surface_child);
  child->AddChild(surface);
  child->AddChild(surface_sibling);
  root->AddChild(child);
  host()->SetRootLayer(root);

  CommitAndActivate();

  EXPECT_TRANSFORM_EQ(GetRenderSurfaceImpl(surface)->draw_transform(),
                      translate);
  // surface_sibling draws into the root render surface
  Occlusion actual_occlusion =
      GetRenderSurfaceImpl(surface_child)->occlusion_in_content_space();
  Occlusion expected_occlusion(translate, SimpleEnclosedRegion(),
                               SimpleEnclosedRegion(gfx::Rect(200, 200)));
  // With occlusion immune, it will not occlude surface_child's contents.
  EXPECT_FALSE(expected_occlusion.IsEqual(actual_occlusion));

  // Mask layer's occlusion is different because we create transform and render
  // surface for it in layer tree mode.
  actual_occlusion =
      ImplOf(surface_child_mask)->draw_properties().occlusion_in_content_space;
  expected_occlusion = Occlusion(
      gfx::Transform(), SimpleEnclosedRegion(gfx::Rect(-20, -20, 200, 200)),
      SimpleEnclosedRegion());
  // With occlusion immune, it will not occlude surface_child's contents.
  EXPECT_FALSE(expected_occlusion.IsEqual(actual_occlusion));
}

TEST_F(DrawPropertiesTest, OcclusionForLayerWithUninvertibleDrawTransform) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* occluding_child = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform scale = gfx::Transform::MakeScale(1e-15);
  EXPECT_TRUE(scale.IsInvertible());
  EXPECT_FALSE((scale * scale).IsInvertible());

  root->SetBounds(gfx::Size(1000, 1000));
  child->SetBounds(gfx::Size(300, 300));
  grand_child->SetBounds(gfx::Size(200, 200));
  occluding_child->SetBounds(gfx::Size(200, 200));

  child->SetDrawsContent(true);
  grand_child->SetDrawsContent(true);
  occluding_child->SetDrawsContent(true);
  occluding_child->SetContentsOpaque(true);

  CopyProperties(root, child);
  auto& child_transform_node = CreateTransformNode(child);
  child_transform_node.flattens_inherited_transform = false;
  child_transform_node.post_translation = gfx::Vector2dF(10.f, 10.f);
  child_transform_node.sorting_context_id = 1;
  child_transform_node.local = scale;
  CopyProperties(child, grand_child);
  auto& grand_child_transform_node = CreateTransformNode(grand_child);
  grand_child_transform_node.flattens_inherited_transform = false;
  grand_child_transform_node.sorting_context_id = 1;
  grand_child_transform_node.local = scale;
  CopyProperties(root, occluding_child);
  CreateTransformNode(occluding_child).flattens_inherited_transform = false;

  UpdateActiveTreeDrawProperties();

  // Though all layers have invertible transforms, matrix multiplication using
  // floating-point math makes the draw transform uninvertible.
  EXPECT_FALSE(GetTransformNode(grand_child)->ancestors_are_invertible);

  // Since |grand_child| has an uninvertible screen space transform, it is
  // skipped so
  // that we are not computing its occlusion_in_content_space.
  gfx::Rect layer_bounds = gfx::Rect();
  EXPECT_EQ(
      layer_bounds,
      grand_child->draw_properties()
          .occlusion_in_content_space.GetUnoccludedContentRect(layer_bounds));
}

TEST_F(DrawPropertiesTest,
       DrawableAndVisibleContentRectsForLayersInClippedRenderSurface) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child3 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  render_surface->SetBounds(gfx::Size(3, 4));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child3->SetBounds(gfx::Size(50, 50));
  child3->SetDrawsContent(true);

  CreateClipNode(root);
  CopyProperties(root, render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface, child1);
  child1->SetOffsetToTransformParent(gfx::Vector2dF(5.f, 5.f));
  CopyProperties(render_surface, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(75.f, 75.f));
  CopyProperties(render_surface, child3);
  child3->SetOffsetToTransformParent(gfx::Vector2dF(125.f, 125.f));

  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(render_surface));

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            GetRenderSurface(root)->DrawableContentRect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface->visible_layer_rect());

  // A clipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree, but also gets clamped by the ancestor's clip.
  EXPECT_EQ(gfx::RectF(5.f, 5.f, 95.f, 95.f),
            GetRenderSurface(render_surface)->DrawableContentRect());

  // All layers that draw content into the surface have their visible content
  // rect clipped by the surface clip rect.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), child2->visible_layer_rect());
  EXPECT_TRUE(child3->visible_layer_rect().IsEmpty());

  // The visible_drawable_content_rect would be the visible rect in target
  // space.
  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), child1->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 25, 25), child2->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 0, 0), child3->visible_drawable_content_rect());
}

// Check that clipping does not propagate down surfaces.
TEST_F(DrawPropertiesTest, DrawableAndVisibleContentRectsForSurfaceHierarchy) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child3 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  render_surface1->SetBounds(gfx::Size(3, 4));
  render_surface2->SetBounds(gfx::Size(7, 13));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child3->SetBounds(gfx::Size(50, 50));
  child3->SetDrawsContent(true);

  CreateClipNode(root);
  CopyProperties(root, render_surface1);
  CreateEffectNode(render_surface1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface1, render_surface2);
  CreateEffectNode(render_surface2).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface2, child1);
  child1->SetOffsetToTransformParent(gfx::Vector2dF(5.f, 5.f));
  CopyProperties(render_surface2, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(75.f, 75.f));
  CopyProperties(render_surface2, child3);
  child3->SetOffsetToTransformParent(gfx::Vector2dF(125.f, 125.f));

  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(render_surface1));
  ASSERT_TRUE(GetRenderSurface(render_surface2));

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            GetRenderSurface(root)->DrawableContentRect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface2->visible_layer_rect());

  // A clipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree, but also gets clamped by the ancestor's clip.
  EXPECT_EQ(gfx::RectF(5.f, 5.f, 95.f, 95.f),
            GetRenderSurface(render_surface1)->DrawableContentRect());

  // render_surface1 lives in the "unclipped universe" of render_surface1, and
  // is only implicitly clipped by render_surface1's content rect. So,
  // render_surface2 grows to enclose all visible drawable content of its
  // subtree.
  EXPECT_EQ(gfx::RectF(5.f, 5.f, 95.f, 95.f),
            GetRenderSurface(render_surface2)->DrawableContentRect());

  // All layers that draw content into render_surface2 think they are unclipped
  // by the surface. So, only the viewport clip applies.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), child3->visible_layer_rect());

  // DrawableContentRects are also unclipped.
  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), child1->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 25, 25), child2->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 0, 0), child3->visible_drawable_content_rect());
}

TEST_F(DrawPropertiesTest,
       VisibleRectsForClippedDescendantsOfUnclippedSurfaces) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface2 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  render_surface1->SetBounds(gfx::Size(100, 100));
  child1->SetBounds(gfx::Size(500, 500));
  child1->SetDrawsContent(true);
  child2->SetBounds(gfx::Size(700, 700));
  child2->SetDrawsContent(true);
  render_surface2->SetBounds(gfx::Size(1000, 1000));
  render_surface2->SetDrawsContent(true);

  CopyProperties(root, render_surface1);
  CreateEffectNode(render_surface1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface1, child1);
  CreateClipNode(child1);
  CopyProperties(child1, child2);
  CreateClipNode(child2);
  CopyProperties(child2, render_surface2);
  CreateEffectNode(render_surface2).render_surface_reason =
      RenderSurfaceReason::kTest;

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(100, 100), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(100, 100), render_surface2->visible_layer_rect());
}

TEST_F(DrawPropertiesTest,
       VisibleRectsWhenClipChildIsBetweenTwoRenderSurfaces) {
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface2 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));

  clip_parent->SetBounds(gfx::Size(50, 50));
  CopyProperties(root, clip_parent);
  CreateClipNode(clip_parent);

  render_surface1->SetBounds(gfx::Size(20, 20));
  render_surface1->SetDrawsContent(true);
  CopyProperties(clip_parent, render_surface1);
  CreateEffectNode(render_surface1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(render_surface1);

  clip_child->SetBounds(gfx::Size(60, 60));
  clip_child->SetDrawsContent(true);
  CopyProperties(render_surface1, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  render_surface2->SetBounds(gfx::Size(60, 60));
  render_surface2->SetDrawsContent(true);
  CopyProperties(clip_child, render_surface2);
  CreateEffectNode(render_surface2).render_surface_reason =
      RenderSurfaceReason::kTest;

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(20, 20), render_surface1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(50, 50), clip_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(50, 50), render_surface2->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, ClipRectOfSurfaceWhoseParentIsAClipChild) {
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface_layer1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface_layer2 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));

  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_parent->SetOffsetToTransformParent(gfx::Vector2dF(2, 2));
  CopyProperties(root, clip_parent);
  CreateClipNode(clip_parent);

  clip_layer->SetBounds(gfx::Size(50, 50));
  clip_layer->SetOffsetToTransformParent(gfx::Vector2dF(2, 2));
  CopyProperties(clip_parent, clip_layer);
  CreateClipNode(clip_layer);

  render_surface_layer1->SetBounds(gfx::Size(20, 20));
  render_surface_layer1->SetDrawsContent(true);
  CopyProperties(clip_layer, render_surface_layer1);
  CreateTransformNode(render_surface_layer1).post_translation =
      gfx::Vector2dF(2, 2);
  CreateEffectNode(render_surface_layer1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(render_surface_layer1);

  clip_child->SetBounds(gfx::Size(60, 60));
  clip_child->SetDrawsContent(true);
  CopyProperties(render_surface_layer1, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  render_surface_layer2->SetBounds(gfx::Size(60, 60));
  render_surface_layer2->SetDrawsContent(true);
  CopyProperties(clip_child, render_surface_layer2);
  CreateTransformNode(render_surface_layer2);
  CreateEffectNode(render_surface_layer2).render_surface_reason =
      RenderSurfaceReason::kTest;

  float device_scale_factor = 1.f;
  UpdateActiveTreeDrawProperties(device_scale_factor);
  auto* render_surface1 = GetRenderSurface(render_surface_layer1);
  EXPECT_TRUE(render_surface1->has_contributing_layer_that_escapes_clip());
  EXPECT_EQ(clip_layer->clip_tree_index(), render_surface1->ClipTreeIndex());
  if (base::FeatureList::IsEnabled(
          features::kRenderSurfaceCommonAncestorClip)) {
    EXPECT_EQ(clip_parent->clip_tree_index(),
              render_surface1->common_ancestor_clip_id());
    EXPECT_TRUE(render_surface1->is_clipped());
    EXPECT_EQ(gfx::Rect(2, 2, 50, 50), render_surface1->clip_rect());
  } else {
    EXPECT_EQ(clip_layer->clip_tree_index(),
              render_surface1->common_ancestor_clip_id());
    EXPECT_FALSE(render_surface1->is_clipped());
  }
  auto* render_surface2 = GetRenderSurface(render_surface_layer2);
  EXPECT_FALSE(render_surface2->has_contributing_layer_that_escapes_clip());
  EXPECT_EQ(clip_parent->clip_tree_index(), render_surface2->ClipTreeIndex());
  EXPECT_EQ(clip_parent->clip_tree_index(),
            render_surface2->common_ancestor_clip_id());
  if (base::FeatureList::IsEnabled(
          features::kRenderSurfaceCommonAncestorClip)) {
    // render_surface2 has the same clip as render_surface1, so it doesn't need
    // to clip.
    EXPECT_FALSE(render_surface2->is_clipped());
  } else {
    EXPECT_TRUE(render_surface2->is_clipped());
    EXPECT_EQ(gfx::Rect(50, 50), render_surface2->clip_rect());
  }

  device_scale_factor = 2.f;
  UpdateActiveTreeDrawProperties(device_scale_factor);
  if (base::FeatureList::IsEnabled(
          features::kRenderSurfaceCommonAncestorClip)) {
    EXPECT_TRUE(render_surface1->is_clipped());
    EXPECT_EQ(gfx::Rect(4, 4, 100, 100), render_surface1->clip_rect());
    EXPECT_FALSE(render_surface2->is_clipped());
  } else {
    EXPECT_FALSE(render_surface1->is_clipped());
    EXPECT_TRUE(render_surface2->is_clipped());
    EXPECT_EQ(gfx::Rect(100, 100), render_surface2->clip_rect());
  }
}

TEST_F(DrawPropertiesTest,
       RenderSurfaceCommonAncestorClipOnChangeOfChildLayerClip) {
  if (!base::FeatureList::IsEnabled(
          features::kRenderSurfaceCommonAncestorClip)) {
    return;
  }

  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));

  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_parent->SetOffsetToTransformParent(gfx::Vector2dF(2, 2));
  CopyProperties(root, clip_parent);
  CreateClipNode(clip_parent);

  clip_layer->SetBounds(gfx::Size(50, 50));
  clip_layer->SetOffsetToTransformParent(gfx::Vector2dF(3, 3));
  CopyProperties(clip_parent, clip_layer);
  CreateClipNode(clip_layer);

  render_surface_layer->SetBounds(gfx::Size(20, 20));
  render_surface_layer->SetDrawsContent(true);
  CopyProperties(clip_layer, render_surface_layer);
  CreateTransformNode(render_surface_layer).post_translation =
      gfx::Vector2dF(5, 5);
  CreateEffectNode(render_surface_layer).render_surface_reason =
      RenderSurfaceReason::kTest;

  clip_child->SetBounds(gfx::Size(60, 60));
  clip_child->SetDrawsContent(true);
  CopyProperties(render_surface_layer, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  UpdateActiveTreeDrawProperties();
  auto* render_surface = GetRenderSurface(render_surface_layer);
  EXPECT_TRUE(render_surface->has_contributing_layer_that_escapes_clip());
  EXPECT_EQ(clip_layer->clip_tree_index(), render_surface->ClipTreeIndex());
  EXPECT_EQ(clip_parent->clip_tree_index(),
            render_surface->common_ancestor_clip_id());
  EXPECT_TRUE(render_surface->is_clipped());
  EXPECT_EQ(gfx::Rect(2, 2, 50, 50), render_surface->clip_rect());

  // Now clip_child no longer escapes the clip of render surface.
  clip_child->SetClipTreeIndex(clip_layer->clip_tree_index());
  host_impl()->active_tree()->set_needs_update_draw_properties();
  UpdateActiveTreeDrawProperties();
  EXPECT_FALSE(render_surface->has_contributing_layer_that_escapes_clip());
  EXPECT_EQ(clip_layer->clip_tree_index(), render_surface->ClipTreeIndex());
  EXPECT_EQ(clip_layer->clip_tree_index(),
            render_surface->common_ancestor_clip_id());
  EXPECT_TRUE(render_surface->is_clipped());
  EXPECT_EQ(gfx::Rect(3, 3, 49, 49), render_surface->clip_rect());
}

// Test that only drawn layers contribute to render surface content rect.
TEST_F(DrawPropertiesTest, RenderSurfaceContentRectWhenLayerNotDrawn) {
  LayerImpl* root = root_layer();
  LayerImpl* surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* test_layer = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(200, 200));
  surface->SetBounds(gfx::Size(100, 100));
  surface->SetDrawsContent(true);
  test_layer->SetBounds(gfx::Size(150, 150));

  CopyProperties(root, surface);
  CreateEffectNode(surface).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(surface, test_layer);

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(100, 100), GetRenderSurface(surface)->content_rect());

  test_layer->SetDrawsContent(true);
  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(150, 150), GetRenderSurface(surface)->content_rect());
}

// Tests visible rects computation when we have unclipped_surface->
// surface_with_unclipped_descendants->clipped_surface, checks that the bounds
// of surface_with_unclipped_descendants doesn't propagate to the
// clipped_surface below it.
TEST_F(DrawPropertiesTest, VisibleRectsMultipleSurfaces) {
  LayerImpl* root = root_layer();
  LayerImpl* unclipped_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* unclipped_desc_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clipped_surface = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  unclipped_surface->SetBounds(gfx::Size(30, 30));
  unclipped_surface->SetDrawsContent(true);
  clip_parent->SetBounds(gfx::Size(50, 50));
  unclipped_desc_surface->SetBounds(gfx::Size(20, 20));
  unclipped_desc_surface->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(60, 60));
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());
  clipped_surface->SetBounds(gfx::Size(60, 60));
  clipped_surface->SetDrawsContent(true);

  CopyProperties(root, unclipped_surface);
  CreateEffectNode(unclipped_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(unclipped_surface, clip_parent);
  CreateClipNode(clip_parent);
  CopyProperties(clip_parent, unclipped_desc_surface);
  CreateEffectNode(unclipped_desc_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(unclipped_desc_surface, clip_child);
  CopyProperties(clip_child, clipped_surface);
  CreateEffectNode(clipped_surface).render_surface_reason =
      RenderSurfaceReason::kTest;

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(30, 30), unclipped_surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(20, 20), unclipped_desc_surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(50, 50), clipped_surface->visible_layer_rect());
}

// Tests visible rects computation when we have unclipped_surface->
// surface_with_unclipped_descendants->clipped_surface, checks that the bounds
// of root propagate to the clipped_surface.
TEST_F(DrawPropertiesTest, RootClipPropagationToClippedSurface) {
  LayerImpl* root = root_layer();
  LayerImpl* unclipped_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* unclipped_desc_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clipped_surface = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(10, 10));
  unclipped_surface->SetBounds(gfx::Size(50, 50));
  unclipped_surface->SetDrawsContent(true);
  clip_parent->SetBounds(gfx::Size(50, 50));
  unclipped_desc_surface->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(100, 100));
  clipped_surface->SetBounds(gfx::Size(50, 50));
  clipped_surface->SetDrawsContent(true);

  CopyProperties(root, unclipped_surface);
  CreateEffectNode(unclipped_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(unclipped_surface, clip_parent);
  CreateClipNode(clip_parent);
  CopyProperties(clip_parent, unclipped_desc_surface);
  CreateEffectNode(unclipped_desc_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(unclipped_desc_surface);
  CopyProperties(unclipped_desc_surface, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());
  CopyProperties(clip_child, clipped_surface);
  CreateEffectNode(clipped_surface).render_surface_reason =
      RenderSurfaceReason::kTest;

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(10, 10), unclipped_surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), unclipped_desc_surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), clipped_surface->visible_layer_rect());
}

// Layers that have non-axis aligned bounds (due to transforms) have an
// expanded, axis-aligned DrawableContentRect and visible content rect.
TEST_F(DrawPropertiesTest,
       DrawableAndVisibleContentRectsWithTransformOnUnclippedSurface) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child1 = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform child_rotation;
  child_rotation.Rotate(45.0);

  root->SetBounds(gfx::Size(100, 100));
  render_surface->SetBounds(gfx::Size(3, 4));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);

  CopyProperties(root, render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface, child1);
  auto& child1_transform_node = CreateTransformNode(child1);
  child1_transform_node.origin = gfx::Point3F(25.f, 25.f, 0.f);
  child1_transform_node.post_translation = gfx::Vector2dF(25.f, 25.f);
  child1_transform_node.local = child_rotation;

  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(render_surface));

  EXPECT_EQ(gfx::RectF(100.f, 100.f),
            GetRenderSurface(root)->DrawableContentRect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface->visible_layer_rect());

  // The unclipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree.
  int diagonal_radius = ceil(sqrt(2.0) * 25.0);
  gfx::Rect expected_surface_drawable_content =
      gfx::Rect(50 - diagonal_radius, 50 - diagonal_radius, diagonal_radius * 2,
                diagonal_radius * 2);
  EXPECT_EQ(gfx::RectF(expected_surface_drawable_content),
            GetRenderSurface(render_surface)->DrawableContentRect());

  // All layers that draw content into the unclipped surface are also unclipped.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(expected_surface_drawable_content,
            child1->visible_drawable_content_rect());
}

// Layers that have non-axis aligned bounds (due to transforms) have an
// expanded, axis-aligned DrawableContentRect and visible content rect.
TEST_F(DrawPropertiesTest,
       DrawableAndVisibleContentRectsWithTransformOnClippedSurface) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child1 = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform child_rotation;
  child_rotation.Rotate(45.0);

  root->SetBounds(gfx::Size(50, 50));
  render_surface->SetBounds(gfx::Size(3, 4));
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);

  CreateClipNode(root);
  CopyProperties(root, render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface, child1);
  // Make sure render surface takes content outside visible rect into
  // consideration.
  root->layer_tree_impl()
      ->property_trees()
      ->effect_tree_mutable()
      .Node(child1->effect_tree_index())
      ->backdrop_filters.Append(
          FilterOperation::CreateZoomFilter(1.f /* zoom */, 0 /* inset */));

  auto& child1_transform_node = CreateTransformNode(child1);
  child1_transform_node.origin = gfx::Point3F(25.f, 25.f, 0.f);
  child1_transform_node.post_translation = gfx::Vector2dF(25.f, 25.f);
  child1_transform_node.local = child_rotation;

  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(render_surface));

  // The clipped surface clamps the DrawableContentRect that encloses the
  // rotated layer.
  int diagonal_radius = ceil(sqrt(2.0) * 25.0);
  gfx::Rect unclipped_surface_content =
      gfx::Rect(50 - diagonal_radius, 50 - diagonal_radius, diagonal_radius * 2,
                diagonal_radius * 2);
  gfx::RectF expected_surface_drawable_content(
      gfx::IntersectRects(unclipped_surface_content, gfx::Rect(50, 50)));
  EXPECT_EQ(expected_surface_drawable_content,
            GetRenderSurface(render_surface)->DrawableContentRect());

  // On the clipped surface, only a quarter  of the child1 is visible, but when
  // rotating it back to  child1's content space, the actual enclosing rect ends
  // up covering the full left half of child1.
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50), child1->visible_layer_rect());

  // The child's DrawableContentRect is unclipped.
  EXPECT_EQ(unclipped_surface_content, child1->visible_drawable_content_rect());
}

TEST_F(DrawPropertiesTest, DrawableAndVisibleContentRectsInHighDPI) {
  LayerImpl* root = root_layer();
  FakePictureLayerImpl* render_surface1 =
      AddLayerInActiveTree<FakePictureLayerImpl>();
  FakePictureLayerImpl* render_surface2 =
      AddLayerInActiveTree<FakePictureLayerImpl>();
  FakePictureLayerImpl* child1 = AddLayerInActiveTree<FakePictureLayerImpl>();
  FakePictureLayerImpl* child2 = AddLayerInActiveTree<FakePictureLayerImpl>();
  FakePictureLayerImpl* child3 = AddLayerInActiveTree<FakePictureLayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  render_surface1->SetBounds(gfx::Size(3, 4));
  render_surface1->SetDrawsContent(true);
  render_surface2->SetBounds(gfx::Size(7, 13));
  render_surface2->SetDrawsContent(true);
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetDrawsContent(true);
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetDrawsContent(true);
  child3->SetBounds(gfx::Size(50, 50));
  child3->SetDrawsContent(true);

  CreateClipNode(root);
  CopyProperties(root, render_surface1);
  CreateTransformNode(render_surface1).post_translation =
      gfx::Vector2dF(5.f, 5.f);
  CreateEffectNode(render_surface1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface1, render_surface2);
  CreateTransformNode(render_surface2).post_translation =
      gfx::Vector2dF(5.f, 5.f);
  CreateEffectNode(render_surface2).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface2, child1);
  child1->SetOffsetToTransformParent(gfx::Vector2dF(5.f, 5.f));
  CopyProperties(render_surface2, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(75.f, 75.f));
  CopyProperties(render_surface2, child3);
  child3->SetOffsetToTransformParent(gfx::Vector2dF(125.f, 125.f));

  float device_scale_factor = 2.f;
  UpdateActiveTreeDrawProperties(device_scale_factor);

  ASSERT_TRUE(GetRenderSurface(render_surface1));
  ASSERT_TRUE(GetRenderSurface(render_surface2));

  // drawable_content_rects for all layers and surfaces are scaled by
  // device_scale_factor.
  EXPECT_EQ(gfx::RectF(200.f, 200.f),
            GetRenderSurface(root)->DrawableContentRect());
  EXPECT_EQ(gfx::RectF(10.f, 10.f, 190.f, 190.f),
            GetRenderSurface(render_surface1)->DrawableContentRect());

  // render_surface2 lives in the "unclipped universe" of render_surface1, and
  // is only implicitly clipped by render_surface1, though it would only contain
  // the visible content.
  EXPECT_EQ(gfx::RectF(10.f, 10.f, 180.f, 180.f),
            GetRenderSurface(render_surface2)->DrawableContentRect());

  EXPECT_EQ(gfx::Rect(10, 10, 100, 100),
            child1->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(150, 150, 30, 30),
            child2->visible_drawable_content_rect());
  EXPECT_EQ(gfx::Rect(250, 250, 0, 0), child3->visible_drawable_content_rect());

  // The root layer does not actually draw content of its own.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());

  // All layer visible content rects are not expressed in content space of each
  // layer, so they are not scaled by the device_scale_factor.
  EXPECT_EQ(gfx::Rect(0, 0, 3, 4), render_surface1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 7, 13), render_surface2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 15, 15), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), child3->visible_layer_rect());
}

using DrawPropertiesScalingTest = DrawPropertiesTest;

// Verify draw and screen space transforms of layers not in a surface.
TEST_F(DrawPropertiesScalingTest, LayerTransformsInHighDPI) {
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);

  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();
  child2->SetBounds(gfx::Size(5, 5));
  child2->SetDrawsContent(true);

  float device_scale_factor = 2.5f;

  CopyProperties(root, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(2.f, 2.f));
  CopyProperties(root, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(2.f, 2.f));

  UpdateActiveTreeDrawProperties(device_scale_factor);

  EXPECT_EQ(gfx::Vector2dF(device_scale_factor, device_scale_factor),
            root->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(device_scale_factor, device_scale_factor),
            child->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(device_scale_factor, device_scale_factor),
            child2->GetIdealContentsScale());

  EXPECT_EQ(1u, GetRenderSurfaceList().size());

  // Verify root transforms
  gfx::Transform expected_root_transform;
  expected_root_transform.Scale(device_scale_factor, device_scale_factor);
  EXPECT_TRANSFORM_EQ(expected_root_transform, root->ScreenSpaceTransform());
  EXPECT_TRANSFORM_EQ(expected_root_transform, root->DrawTransform());

  // Verify results of transformed root rects
  gfx::RectF root_bounds(gfx::SizeF(root->bounds()));

  gfx::RectF root_draw_rect =
      MathUtil::MapClippedRect(root->DrawTransform(), root_bounds);
  gfx::RectF root_screen_space_rect =
      MathUtil::MapClippedRect(root->ScreenSpaceTransform(), root_bounds);

  gfx::RectF expected_root_draw_rect(gfx::SizeF(root->bounds()));
  expected_root_draw_rect.Scale(device_scale_factor);
  EXPECT_RECTF_EQ(expected_root_draw_rect, root_draw_rect);
  EXPECT_RECTF_EQ(expected_root_draw_rect, root_screen_space_rect);

  // Verify child and child2 transforms. They should match.
  gfx::Transform expected_child_transform;
  expected_child_transform.Scale(device_scale_factor, device_scale_factor);
  expected_child_transform.Translate(child->offset_to_transform_parent());
  EXPECT_TRANSFORM_EQ(expected_child_transform, child->DrawTransform());
  EXPECT_TRANSFORM_EQ(expected_child_transform, child->ScreenSpaceTransform());
  EXPECT_TRANSFORM_EQ(expected_child_transform, child2->DrawTransform());
  EXPECT_TRANSFORM_EQ(expected_child_transform, child2->ScreenSpaceTransform());

  // Verify results of transformed child and child2 rects. They should
  // match.
  gfx::RectF child_bounds(gfx::SizeF(child->bounds()));

  gfx::RectF child_draw_rect =
      MathUtil::MapClippedRect(child->DrawTransform(), child_bounds);
  gfx::RectF child_screen_space_rect =
      MathUtil::MapClippedRect(child->ScreenSpaceTransform(), child_bounds);

  gfx::RectF child2_draw_rect =
      MathUtil::MapClippedRect(child2->DrawTransform(), child_bounds);
  gfx::RectF child2_screen_space_rect =
      MathUtil::MapClippedRect(child2->ScreenSpaceTransform(), child_bounds);

  gfx::RectF expected_child_draw_rect(
      gfx::PointAtOffsetFromOrigin(child->offset_to_transform_parent()),
      gfx::SizeF(child->bounds()));
  expected_child_draw_rect.Scale(device_scale_factor);
  EXPECT_RECTF_EQ(expected_child_draw_rect, child_draw_rect);
  EXPECT_RECTF_EQ(expected_child_draw_rect, child_screen_space_rect);
  EXPECT_RECTF_EQ(expected_child_draw_rect, child2_draw_rect);
  EXPECT_RECTF_EQ(expected_child_draw_rect, child2_screen_space_rect);
}

// Verify draw and screen space transforms of layers in a surface.
TEST_F(DrawPropertiesScalingTest, SurfaceLayerTransformsInHighDPI) {
  gfx::Transform perspective_matrix;
  perspective_matrix.ApplyPerspectiveDepth(2.0);
  perspective_matrix.RotateAboutYAxis(15.0);
  gfx::Vector2dF perspective_surface_offset(2.f, 2.f);

  gfx::Transform scale_small_matrix;
  scale_small_matrix.Scale(SK_Scalar1 / 10.f, SK_Scalar1 / 12.f);

  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));
  SetupViewport(root, gfx::Size(100, 100), gfx::Size(100, 100));

  LayerImpl* parent = AddLayerInActiveTree<LayerImpl>();
  parent->SetBounds(gfx::Size(100, 100));
  parent->SetDrawsContent(true);

  LayerImpl* perspective_surface = AddLayerInActiveTree<LayerImpl>();
  perspective_surface->SetBounds(gfx::Size(10, 10));
  perspective_surface->SetDrawsContent(true);

  LayerImpl* scale_surface = AddLayerInActiveTree<LayerImpl>();
  scale_surface->SetBounds(gfx::Size(10, 10));
  scale_surface->SetDrawsContent(true);

  CopyProperties(OuterViewportScrollLayer(), parent);
  CopyProperties(parent, perspective_surface);
  auto& perspective_surface_transform =
      CreateTransformNode(perspective_surface);
  perspective_surface_transform.local = perspective_matrix * scale_small_matrix;
  perspective_surface_transform.post_translation = perspective_surface_offset;
  CreateEffectNode(perspective_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(parent, scale_surface);
  auto& scale_surface_transform = CreateTransformNode(scale_surface);
  scale_surface_transform.local = scale_small_matrix;
  scale_surface_transform.post_translation = gfx::Vector2dF(2.f, 2.f);
  CreateEffectNode(scale_surface).render_surface_reason =
      RenderSurfaceReason::kTest;

  float device_scale_factor = 2.5f;
  float page_scale_factor = 3.f;
  float contents_scale_factor = device_scale_factor * page_scale_factor;
  host_impl()->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  UpdateActiveTreeDrawProperties(device_scale_factor);

  EXPECT_EQ(gfx::Vector2dF(contents_scale_factor, contents_scale_factor),
            parent->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(contents_scale_factor, contents_scale_factor),
            perspective_surface->GetIdealContentsScale());
  // Ideal scale is the max 2d scale component of the combined transform up to
  // the nearest render target. Here this includes the layer transform as well
  // as the device and page scale factors.
  gfx::Transform transform = scale_small_matrix;
  transform.Scale(contents_scale_factor, contents_scale_factor);
  gfx::Vector2dF scales =
      gfx::ComputeTransform2dScaleComponents(transform, 0.f);
  EXPECT_EQ(scales, scale_surface->GetIdealContentsScale());

  // The ideal scale will draw 1:1 with its render target space along
  // the larger-scale axis.
  gfx::Vector2dF target_space_transform_scales =
      gfx::ComputeTransform2dScaleComponents(
          scale_surface->draw_properties().target_space_transform, 0.f);
  EXPECT_EQ(scales, target_space_transform_scales);

  EXPECT_EQ(3u, GetRenderSurfaceList().size());

  gfx::Transform expected_parent_draw_transform;
  expected_parent_draw_transform.Scale(contents_scale_factor,
                                       contents_scale_factor);
  EXPECT_TRANSFORM_EQ(expected_parent_draw_transform, parent->DrawTransform());

  // The scale for the perspective surface is not known, so it is rendered 1:1
  // with the screen, and then scaled during drawing.
  gfx::Transform expected_perspective_surface_draw_transform;
  expected_perspective_surface_draw_transform.Scale(contents_scale_factor,
                                                    contents_scale_factor);
  expected_perspective_surface_draw_transform.Translate(
      perspective_surface_offset);
  expected_perspective_surface_draw_transform.PreConcat(perspective_matrix);
  expected_perspective_surface_draw_transform.PreConcat(scale_small_matrix);
  expected_perspective_surface_draw_transform.Scale(
      1.0f / contents_scale_factor, 1.0f / contents_scale_factor);
  gfx::Transform expected_perspective_surface_layer_draw_transform;
  expected_perspective_surface_layer_draw_transform.Scale(
      contents_scale_factor, contents_scale_factor);
  EXPECT_TRANSFORM_EQ(expected_perspective_surface_draw_transform,
                      GetRenderSurface(perspective_surface)->draw_transform());
  EXPECT_TRANSFORM_EQ(expected_perspective_surface_layer_draw_transform,
                      perspective_surface->DrawTransform());
}

TEST_F(DrawPropertiesScalingTest, SmallIdealScale) {
  gfx::Transform parent_scale_matrix;
  SkScalar initial_parent_scale = 1.75;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkScalar initial_child_scale = 0.25;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* page_scale = AddLayerInActiveTree<LayerImpl>();
  page_scale->SetBounds(gfx::Size(100, 100));

  LayerImpl* parent = AddLayerInActiveTree<LayerImpl>();
  parent->SetBounds(gfx::Size(100, 100));
  parent->SetDrawsContent(true);

  LayerImpl* child_scale = AddLayerInActiveTree<LayerImpl>();
  child_scale->SetBounds(gfx::Size(10, 10));
  child_scale->SetDrawsContent(true);

  float device_scale_factor = 2.5f;
  float page_scale_factor = 0.01f;

  CopyProperties(root, page_scale);
  CreateTransformNode(page_scale).in_subtree_of_page_scale_layer = true;
  CopyProperties(page_scale, parent);
  CreateTransformNode(parent).local = parent_scale_matrix;
  CopyProperties(parent, child_scale);
  CreateTransformNode(child_scale).local = child_scale_matrix;

  ViewportPropertyIds viewport_property_ids;
  viewport_property_ids.page_scale_transform =
      page_scale->transform_tree_index();
  host_impl()->active_tree()->SetViewportPropertyIds(viewport_property_ids);

  host_impl()->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  UpdateActiveTreeDrawProperties(device_scale_factor);

  // The ideal scale is able to go below 1.
  float expected_ideal_scale =
      device_scale_factor * page_scale_factor * initial_parent_scale;
  EXPECT_LT(expected_ideal_scale, 1.f);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(expected_ideal_scale, expected_ideal_scale),
      parent->GetIdealContentsScale());

  expected_ideal_scale = device_scale_factor * page_scale_factor *
                         initial_parent_scale * initial_child_scale;
  EXPECT_LT(expected_ideal_scale, 1.f);
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(expected_ideal_scale, expected_ideal_scale),
      child_scale->GetIdealContentsScale());
}

TEST_F(DrawPropertiesScalingTest, IdealScaleForAnimatingLayer) {
  gfx::Transform parent_scale_matrix;
  SkScalar initial_parent_scale = 1.75;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkScalar initial_child_scale = 1.25;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* parent = AddLayerInActiveTree<LayerImpl>();
  parent->SetBounds(gfx::Size(100, 100));
  parent->SetDrawsContent(true);

  LayerImpl* child_scale = AddLayerInActiveTree<LayerImpl>();
  child_scale->SetBounds(gfx::Size(10, 10));
  child_scale->SetDrawsContent(true);

  CopyProperties(root, parent);
  CreateTransformNode(parent).local = parent_scale_matrix;
  CopyProperties(parent, child_scale);
  CreateTransformNode(child_scale).local = child_scale_matrix;

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::Vector2dF(initial_parent_scale, initial_parent_scale),
            parent->GetIdealContentsScale());
  // Animating layers compute ideal scale in the same way as when
  // they are static.
  EXPECT_EQ(gfx::Vector2dF(initial_child_scale * initial_parent_scale,
                           initial_child_scale * initial_parent_scale),
            child_scale->GetIdealContentsScale());
}

TEST_F(DrawPropertiesTest, RenderSurfaceTransformsInHighDPI) {
  LayerImpl* parent = root_layer();
  parent->SetBounds(gfx::Size(30, 30));
  parent->SetDrawsContent(true);

  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  // This layer should end up in the same surface as child, with the same draw
  // and screen space transforms.
  LayerImpl* duplicate_child_non_owner = AddLayerInActiveTree<LayerImpl>();
  duplicate_child_non_owner->SetBounds(gfx::Size(10, 10));
  duplicate_child_non_owner->SetDrawsContent(true);

  float device_scale_factor = 1.5f;
  gfx::Vector2dF child_offset(2.f, 2.f);

  CopyProperties(parent, child);
  CreateTransformNode(child).post_translation = child_offset;
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(child, duplicate_child_non_owner);
  duplicate_child_non_owner->SetOffsetToTransformParent(
      child->offset_to_transform_parent());

  UpdateActiveTreeDrawProperties(device_scale_factor);

  // We should have two render surfaces. The root's render surface and child's
  // render surface (it needs one because of force_render_surface).
  EXPECT_EQ(2u, GetRenderSurfaceList().size());

  gfx::Transform expected_parent_transform;
  expected_parent_transform.Scale(device_scale_factor, device_scale_factor);
  EXPECT_TRANSFORM_EQ(expected_parent_transform,
                      parent->ScreenSpaceTransform());
  EXPECT_TRANSFORM_EQ(expected_parent_transform, parent->DrawTransform());

  gfx::Transform expected_draw_transform;
  expected_draw_transform.Scale(device_scale_factor, device_scale_factor);
  EXPECT_TRANSFORM_EQ(expected_draw_transform, child->DrawTransform());

  gfx::Transform expected_screen_space_transform;
  expected_screen_space_transform.Scale(device_scale_factor,
                                        device_scale_factor);
  expected_screen_space_transform.Translate(child_offset);
  EXPECT_TRANSFORM_EQ(expected_screen_space_transform,
                      child->ScreenSpaceTransform());

  gfx::Transform expected_duplicate_child_draw_transform =
      child->DrawTransform();
  EXPECT_TRANSFORM_EQ(expected_duplicate_child_draw_transform,
                      duplicate_child_non_owner->DrawTransform());
  EXPECT_TRANSFORM_EQ(child->ScreenSpaceTransform(),
                      duplicate_child_non_owner->ScreenSpaceTransform());
  EXPECT_EQ(child->visible_drawable_content_rect(),
            duplicate_child_non_owner->visible_drawable_content_rect());
  EXPECT_EQ(child->bounds(), duplicate_child_non_owner->bounds());

  gfx::Transform expected_render_surface_draw_transform;
  expected_render_surface_draw_transform.Translate(
      device_scale_factor * child_offset.x(),
      device_scale_factor * child_offset.y());
  EXPECT_TRANSFORM_EQ(expected_render_surface_draw_transform,
                      GetRenderSurface(child)->draw_transform());

  gfx::Transform expected_surface_draw_transform;
  expected_surface_draw_transform.Translate(device_scale_factor * 2.f,
                                            device_scale_factor * 2.f);
  EXPECT_TRANSFORM_EQ(expected_surface_draw_transform,
                      GetRenderSurface(child)->draw_transform());

  gfx::Transform expected_surface_screen_space_transform;
  expected_surface_screen_space_transform.Translate(device_scale_factor * 2.f,
                                                    device_scale_factor * 2.f);
  EXPECT_TRANSFORM_EQ(expected_surface_screen_space_transform,
                      GetRenderSurface(child)->screen_space_transform());
}

TEST_F(DrawPropertiesTest,
       RenderSurfaceTransformsInHighDPIAccurateScaleZeroPosition) {
  LayerImpl* parent = root_layer();
  parent->SetBounds(gfx::Size(33, 31));
  parent->SetDrawsContent(true);

  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  child->SetBounds(gfx::Size(13, 11));
  child->SetDrawsContent(true);

  float device_scale_factor = 1.7f;

  CopyProperties(parent, child);
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;

  UpdateActiveTreeDrawProperties(device_scale_factor);

  // We should have two render surfaces. The root's render surface and child's
  // render surface (it needs one because of force_render_surface).
  EXPECT_EQ(2u, GetRenderSurfaceList().size());

  EXPECT_TRANSFORM_EQ(gfx::Transform(),
                      GetRenderSurface(child)->draw_transform());
  EXPECT_TRANSFORM_EQ(gfx::Transform(),
                      GetRenderSurface(child)->draw_transform());
  EXPECT_TRANSFORM_EQ(gfx::Transform(),
                      GetRenderSurface(child)->screen_space_transform());
}

// Needs layer tree mode: mask layer. Not using impl-side PropertyTreeBuilder.
TEST_F(DrawPropertiesTestWithLayerTree, LayerSearch) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  FakeContentLayerClient client;
  scoped_refptr<PictureLayer> mask_layer = PictureLayer::Create(&client);

  child->AddChild(grand_child.get());
  child->SetMaskLayer(mask_layer);
  root->AddChild(child.get());

  host()->SetRootLayer(root);

  int nonexistent_id = -1;
  EXPECT_EQ(root.get(), host()->LayerById(root->id()));
  EXPECT_EQ(child.get(), host()->LayerById(child->id()));
  EXPECT_EQ(grand_child.get(), host()->LayerById(grand_child->id()));
  EXPECT_EQ(mask_layer.get(), host()->LayerById(mask_layer->id()));
  EXPECT_FALSE(host()->LayerById(nonexistent_id));
}

TEST_F(DrawPropertiesTest, TransparentChildRenderSurfaceCreation) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  grand_child->SetBounds(gfx::Size(10, 10));
  grand_child->SetDrawsContent(true);

  CopyProperties(root, child);
  CreateEffectNode(child).opacity = 0.5f;
  CopyProperties(child, grand_child);

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(GetRenderSurface(child), GetRenderSurface(root));
}

TEST_F(DrawPropertiesTest, OpacityAnimatingOnPendingTree) {
  host_impl()->CreatePendingTree();
  LayerImpl* root = EnsureRootLayerInPendingTree();
  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);

  auto* child = AddLayerInPendingTree<LayerImpl>();
  child->SetBounds(gfx::Size(50, 50));
  child->SetDrawsContent(true);

  host_impl()->pending_tree()->SetElementIdsForTesting();
  CopyProperties(root, child);
  CreateEffectNode(child).opacity = 0.0f;

  // Add opacity animation.
  AddOpacityTransitionToElementWithAnimation(
      child->element_id(), timeline_impl(), 10.0, 0.0f, 1.0f, false);

  UpdatePendingTreeDrawProperties();

  // We should have one render surface and two layers. The child
  // layer should be included even though it is transparent.
  ASSERT_EQ(1u, host_impl()->pending_tree()->GetRenderSurfaceList().size());
  ASSERT_EQ(2, GetRenderSurface(root)->num_contributors());

  // If the root itself is hidden, the child should not be drawn even if it has
  // an animating opacity.
  SetOpacity(root, 0.0f);
  UpdatePendingTreeDrawProperties();

  EXPECT_FALSE(GetEffectNode(child)->is_drawn);

  // A layer should be drawn and it should contribute to drawn surface when
  // it has animating opacity even if it has opacity 0.
  SetOpacity(root, 1.0f);
  SetOpacity(child, 0.0f);
  UpdatePendingTreeDrawProperties();

  EXPECT_TRUE(GetEffectNode(child)->is_drawn);
  EXPECT_TRUE(GetPropertyTrees(root)->effect_tree().ContributesToDrawnSurface(
      child->effect_tree_index()));

  // But if the opacity of the layer remains 0 after activation, it should not
  // be drawn.
  host_impl()->ActivateSyncTree();
  LayerTreeImpl* active_tree = host_impl()->active_tree();
  LayerImpl* active_child = active_tree->LayerById(child->id());

  const EffectTree& active_effect_tree =
      active_tree->property_trees()->effect_tree();
  EXPECT_TRUE(active_effect_tree.needs_update());

  UpdateActiveTreeDrawProperties();

  EXPECT_FALSE(GetEffectNode(active_child)->is_drawn);
  EXPECT_FALSE(active_effect_tree.ContributesToDrawnSurface(
      active_child->effect_tree_index()));
}

class BackfaceVisibilityInteropTest : public DrawPropertiesTestBase,
                                      public testing::Test {
 public:
  BackfaceVisibilityInteropTest()
      : DrawPropertiesTestBase(BackfaceVisibilityInteropSettings()) {}

 protected:
  LayerTreeSettings BackfaceVisibilityInteropSettings() {
    LayerTreeSettings settings = CommitToPendingTreeLayerListSettings();
    settings.enable_backface_visibility_interop = true;
    return settings;
  }
};

TEST_F(BackfaceVisibilityInteropTest, BackfaceInvisibleTransform) {
  LayerImpl* root = root_layer();
  root->SetDrawsContent(true);
  LayerImpl* back_facing = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* back_facing_double_sided = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* front_facing = AddLayerInActiveTree<LayerImpl>();
  back_facing->SetDrawsContent(true);
  back_facing_double_sided->SetDrawsContent(true);
  front_facing->SetDrawsContent(true);

  root->SetBounds(gfx::Size(50, 50));
  back_facing->SetBounds(gfx::Size(50, 50));
  back_facing_double_sided->SetBounds(gfx::Size(50, 50));
  front_facing->SetBounds(gfx::Size(50, 50));
  CopyProperties(root, back_facing);
  CopyProperties(root, front_facing);

  back_facing->SetShouldCheckBackfaceVisibility(true);
  back_facing_double_sided->SetShouldCheckBackfaceVisibility(false);
  front_facing->SetShouldCheckBackfaceVisibility(true);

  auto& back_facing_transform_node = CreateTransformNode(back_facing);
  back_facing_transform_node.flattens_inherited_transform = false;
  back_facing_transform_node.sorting_context_id = 1;
  gfx::Transform rotate_about_y;
  rotate_about_y.RotateAboutYAxis(180.0);
  back_facing_transform_node.local = rotate_about_y;
  back_facing_transform_node.will_change_transform = true;

  CopyProperties(back_facing, back_facing_double_sided);

  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(draw_property_utils::IsLayerBackFaceVisibleForTesting(
      back_facing, host_impl()->active_tree()->property_trees()));
  EXPECT_TRUE(draw_property_utils::IsLayerBackFaceVisibleForTesting(
      back_facing_double_sided, host_impl()->active_tree()->property_trees()));
  EXPECT_FALSE(draw_property_utils::IsLayerBackFaceVisibleForTesting(
      front_facing, host_impl()->active_tree()->property_trees()));

  EXPECT_TRUE(back_facing->raster_even_if_not_drawn());
  EXPECT_TRUE(
      draw_property_utils::LayerShouldBeSkippedForDrawPropertiesComputation(
          back_facing, host_impl()->active_tree()->property_trees()));
  EXPECT_FALSE(
      draw_property_utils::LayerShouldBeSkippedForDrawPropertiesComputation(
          back_facing_double_sided,
          host_impl()->active_tree()->property_trees()));
  EXPECT_FALSE(
      draw_property_utils::LayerShouldBeSkippedForDrawPropertiesComputation(
          front_facing, host_impl()->active_tree()->property_trees()));
}

// Needs layer tree mode: hide_layer_and_subtree.
TEST_F(DrawPropertiesTestWithLayerTree, SubtreeHidden_SingleLayerImpl) {
  auto root = Layer::Create();
  root->SetBounds(gfx::Size(50, 50));
  root->SetIsDrawable(true);

  auto child = Layer::Create();
  root->AddChild(child);
  child->SetBounds(gfx::Size(40, 40));
  child->SetIsDrawable(true);

  auto grand_child = Layer::Create();
  child->AddChild(grand_child);
  grand_child->SetBounds(gfx::Size(30, 30));
  grand_child->SetIsDrawable(true);
  grand_child->SetHideLayerAndSubtree(true);

  child->AddChild(grand_child);
  root->AddChild(child);
  host()->SetRootLayer(root);

  CommitAndActivate();

  // We should have one render surface and two layers. The grand child has
  // hidden itself.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(2, GetRenderSurfaceImpl(root)->num_contributors());
  EXPECT_TRUE(ImplOf(root)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child)->contributes_to_drawn_render_surface());
}

// Needs layer tree mode: hide_layer_and_subtree.
TEST_F(DrawPropertiesTestWithLayerTree, SubtreeHidden_TwoLayersImpl) {
  auto root = Layer::Create();
  root->SetBounds(gfx::Size(50, 50));
  root->SetIsDrawable(true);

  auto child = Layer::Create();
  child->SetBounds(gfx::Size(40, 40));
  child->SetIsDrawable(true);
  child->SetHideLayerAndSubtree(true);

  auto grand_child = Layer::Create();
  grand_child->SetBounds(gfx::Size(30, 30));
  grand_child->SetIsDrawable(true);

  child->AddChild(grand_child);
  root->AddChild(child);
  host()->SetRootLayer(root);

  CommitAndActivate();

  // We should have one render surface and one layer. The child has
  // hidden itself and the grand child.
  ASSERT_EQ(1u, GetRenderSurfaceList().size());
  ASSERT_EQ(1, GetRenderSurfaceImpl(root)->num_contributors());
  EXPECT_TRUE(ImplOf(root)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child)->contributes_to_drawn_render_surface());
}

// Needs layer tree mode: mask layer, hide_layer_and_subtree and copy request.
TEST_F(DrawPropertiesTestWithLayerTree, SubtreeHiddenWithCopyRequest) {
  auto root = Layer::Create();
  root->SetBounds(gfx::Size(50, 50));
  root->SetIsDrawable(true);

  auto copy_grand_parent = Layer::Create();
  copy_grand_parent->SetBounds(gfx::Size(40, 40));
  copy_grand_parent->SetIsDrawable(true);

  auto copy_parent = Layer::Create();
  copy_parent->SetBounds(gfx::Size(30, 30));
  copy_parent->SetIsDrawable(true);
  copy_parent->SetForceRenderSurfaceForTesting(true);

  auto copy_layer = Layer::Create();
  copy_layer->SetBounds(gfx::Size(20, 20));
  copy_layer->SetIsDrawable(true);
  copy_layer->SetForceRenderSurfaceForTesting(true);

  auto copy_child = Layer::Create();
  copy_child->SetBounds(gfx::Size(20, 20));
  copy_child->SetIsDrawable(true);

  auto copy_grand_child = Layer::Create();
  copy_grand_child->SetBounds(gfx::Size(20, 20));
  copy_grand_child->SetIsDrawable(true);

  auto copy_grand_parent_sibling_before = Layer::Create();
  copy_grand_parent_sibling_before->SetBounds(gfx::Size(40, 40));
  copy_grand_parent_sibling_before->SetIsDrawable(true);

  auto copy_grand_parent_sibling_after = Layer::Create();
  copy_grand_parent_sibling_after->SetBounds(gfx::Size(40, 40));
  copy_grand_parent_sibling_after->SetIsDrawable(true);

  copy_child->AddChild(copy_grand_child);
  copy_layer->AddChild(copy_child);
  copy_parent->AddChild(copy_layer);
  copy_grand_parent->AddChild(copy_parent);
  root->AddChild(copy_grand_parent_sibling_before);
  root->AddChild(copy_grand_parent);
  root->AddChild(copy_grand_parent_sibling_after);
  host()->SetRootLayer(root);

  // Hide the copy_grand_parent and its subtree. But make a copy request in that
  // hidden subtree on copy_layer. Also hide the copy grand child and its
  // subtree.
  copy_grand_parent->SetHideLayerAndSubtree(true);
  copy_grand_parent_sibling_before->SetHideLayerAndSubtree(true);
  copy_grand_parent_sibling_after->SetHideLayerAndSubtree(true);
  copy_grand_child->SetHideLayerAndSubtree(true);

  copy_layer->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());

  host()->SetElementIdsForTesting();
  CommitAndActivate();

  EXPECT_TRUE(GetEffectNode(ImplOf(root))->subtree_has_copy_request);
  EXPECT_TRUE(
      GetEffectNode(ImplOf(copy_grand_parent))->subtree_has_copy_request);
  EXPECT_TRUE(GetEffectNode(ImplOf(copy_parent))->subtree_has_copy_request);
  EXPECT_TRUE(GetEffectNode(ImplOf(copy_layer))->subtree_has_copy_request);

  // We should have four render surfaces, one for the root, one for the grand
  // parent since it has opacity and two drawing descendants, one for the parent
  // since it owns a surface, and one for the copy_layer.
  ASSERT_EQ(4u, GetRenderSurfaceList().size());
  EXPECT_EQ(root->element_id(), GetRenderSurfaceList().at(0)->id());
  EXPECT_EQ(copy_grand_parent->element_id(),
            GetRenderSurfaceList().at(1)->id());
  EXPECT_EQ(copy_parent->element_id(), GetRenderSurfaceList().at(2)->id());
  EXPECT_EQ(copy_layer->element_id(), GetRenderSurfaceList().at(3)->id());

  // The root render surface should have 2 contributing layers.
  EXPECT_EQ(2, GetRenderSurfaceImpl(root)->num_contributors());
  EXPECT_TRUE(ImplOf(root)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(
      ImplOf(copy_grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(copy_grand_parent_sibling_before)
                   ->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(copy_grand_parent_sibling_after)
                   ->contributes_to_drawn_render_surface());

  // Nothing actually draws into the copy parent, so only the copy_layer will
  // appear in its list, since it needs to be drawn for the copy request.
  ASSERT_EQ(1, GetRenderSurfaceImpl(copy_parent)->num_contributors());
  EXPECT_FALSE(ImplOf(copy_parent)->contributes_to_drawn_render_surface());

  // The copy layer's render surface should have 2 contributing layers.
  ASSERT_EQ(2, GetRenderSurfaceImpl(copy_layer)->num_contributors());
  EXPECT_TRUE(ImplOf(copy_layer)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(copy_child)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(copy_grand_child)->contributes_to_drawn_render_surface());

  // copy_grand_parent, copy_parent shouldn't be drawn because they are hidden,
  // but the copy_layer and copy_child should be drawn for the copy request.
  // copy grand child should not be drawn as its hidden even in the copy
  // request.
  EXPECT_FALSE(GetEffectNode(ImplOf(copy_grand_parent))->is_drawn);
  EXPECT_FALSE(GetEffectNode(ImplOf(copy_parent))->is_drawn);
  EXPECT_TRUE(GetEffectNode(ImplOf(copy_layer))->is_drawn);
  EXPECT_TRUE(GetEffectNode(ImplOf(copy_child))->is_drawn);
  EXPECT_FALSE(GetEffectNode(ImplOf(copy_grand_child))->is_drawn);

  // Though copy_layer is drawn, it shouldn't contribute to drawn surface as its
  // actually hidden.
  EXPECT_FALSE(
      GetRenderSurfaceImpl(copy_layer)->contributes_to_drawn_surface());
}

// Needs layer tree mode: copy request.
TEST_F(DrawPropertiesTestWithLayerTree, ClippedOutCopyRequest) {
  auto root = Layer::Create();
  root->SetBounds(gfx::Size(50, 50));
  root->SetIsDrawable(true);

  auto copy_parent = Layer::Create();
  copy_parent->SetIsDrawable(true);
  copy_parent->SetMasksToBounds(true);

  auto copy_layer = Layer::Create();
  copy_layer->SetBounds(gfx::Size(30, 30));
  copy_layer->SetIsDrawable(true);
  copy_layer->SetForceRenderSurfaceForTesting(true);

  auto copy_child = Layer::Create();
  copy_child->SetBounds(gfx::Size(20, 20));
  copy_child->SetIsDrawable(true);

  copy_layer->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());

  copy_layer->AddChild(copy_child);
  copy_parent->AddChild(copy_layer);
  root->AddChild(copy_parent);

  host()->SetRootLayer(root);
  host()->SetElementIdsForTesting();

  CommitAndActivate();

  // We should have two render surface, as the others are clipped out.
  ASSERT_EQ(2u, GetRenderSurfaceList().size());
  EXPECT_EQ(root->element_id(), GetRenderSurfaceList().at(0)->id());

  // The root render surface should have only 2 contributing layer, since the
  // other layers are clipped away.
  ASSERT_EQ(2, GetRenderSurfaceImpl(root)->num_contributors());
  EXPECT_TRUE(ImplOf(root)->contributes_to_drawn_render_surface());
}

// Needs layer tree mode: copy request.
TEST_F(DrawPropertiesTestWithLayerTree, SingularTransformAndCopyRequests) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  root->SetBounds(gfx::Size(50, 50));
  root->SetIsDrawable(true);

  auto singular_transform_layer = Layer::Create();
  root->AddChild(singular_transform_layer);
  singular_transform_layer->SetBounds(gfx::Size(100, 100));
  singular_transform_layer->SetIsDrawable(true);
  gfx::Transform singular;
  singular.Scale3d(6.f, 6.f, 0.f);
  singular_transform_layer->SetTransform(singular);

  auto copy_layer = Layer::Create();
  singular_transform_layer->AddChild(copy_layer);
  copy_layer->SetBounds(gfx::Size(100, 100));
  copy_layer->SetIsDrawable(true);
  copy_layer->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());

  auto copy_child = Layer::Create();
  copy_layer->AddChild(copy_child);
  copy_child->SetBounds(gfx::Size(100, 100));
  copy_child->SetIsDrawable(true);

  auto copy_grand_child = Layer::Create();
  copy_child->AddChild(copy_grand_child);
  copy_grand_child->SetBounds(gfx::Size(100, 100));
  copy_grand_child->SetIsDrawable(true);
  copy_grand_child->SetTransform(singular);

  ASSERT_TRUE(copy_layer->HasCopyRequest());
  CommitAndActivate();
  ASSERT_FALSE(copy_layer->HasCopyRequest());

  // A layer with singular transform should not contribute to drawn render
  // surface.
  EXPECT_FALSE(
      ImplOf(singular_transform_layer)->contributes_to_drawn_render_surface());
  // Even though copy_layer and copy_child have singular screen space transform,
  // they still contribute to drawn render surface as their transform to the
  // closest ancestor with copy request is not singular.
  EXPECT_TRUE(ImplOf(copy_layer)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(copy_child)->contributes_to_drawn_render_surface());
  // copy_grand_child's transform to its closest ancestor with copy request is
  // also singular. So, it doesn't contribute to drawn render surface.
  EXPECT_FALSE(ImplOf(copy_grand_child)->contributes_to_drawn_render_surface());
}

// Needs layer tree mode: copy request.
TEST_F(DrawPropertiesTestWithLayerTree, VisibleRectInNonRootCopyRequest) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  root->SetBounds(gfx::Size(50, 50));
  root->SetIsDrawable(true);
  root->SetMasksToBounds(true);

  auto copy_layer = Layer::Create();
  root->AddChild(copy_layer);
  copy_layer->SetBounds(gfx::Size(100, 100));
  copy_layer->SetIsDrawable(true);
  copy_layer->SetForceRenderSurfaceForTesting(true);

  auto copy_child = Layer::Create();
  copy_layer->AddChild(copy_child);
  copy_child->SetPosition(gfx::PointF(40.f, 40.f));
  copy_child->SetBounds(gfx::Size(20, 20));
  copy_child->SetIsDrawable(true);

  auto copy_clip = Layer::Create();
  copy_layer->AddChild(copy_clip);
  copy_clip->SetBounds(gfx::Size(55, 55));
  copy_clip->SetMasksToBounds(true);

  auto copy_clipped_child = Layer::Create();
  copy_clip->AddChild(copy_clipped_child);
  copy_clipped_child->SetPosition(gfx::PointF(40.f, 40.f));
  copy_clipped_child->SetBounds(gfx::Size(20, 20));
  copy_clipped_child->SetIsDrawable(true);

  auto copy_surface = Layer::Create();
  copy_clip->AddChild(copy_surface);
  copy_surface->SetPosition(gfx::PointF(45.f, 45.f));
  copy_surface->SetBounds(gfx::Size(20, 20));
  copy_surface->SetIsDrawable(true);
  copy_surface->SetForceRenderSurfaceForTesting(true);

  copy_layer->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());
  ASSERT_TRUE(copy_layer->HasCopyRequest());
  CommitAndActivate();
  ASSERT_FALSE(copy_layer->HasCopyRequest());

  EXPECT_EQ(gfx::Rect(100, 100), ImplOf(copy_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(20, 20), ImplOf(copy_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(15, 15),
            ImplOf(copy_clipped_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(copy_surface)->visible_layer_rect());

  // Case 2: When the non root copy request layer is clipped.
  copy_layer->SetBounds(gfx::Size(50, 50));
  copy_layer->SetMasksToBounds(true);
  copy_layer->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());
  ASSERT_TRUE(copy_layer->HasCopyRequest());
  CommitAndActivate();
  ASSERT_FALSE(copy_layer->HasCopyRequest());

  EXPECT_EQ(gfx::Rect(50, 50), ImplOf(copy_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(copy_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10),
            ImplOf(copy_clipped_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(5, 5), ImplOf(copy_surface)->visible_layer_rect());

  // Case 3: When there is device scale factor.
  SetDeviceScaleAndUpdateViewportRect(host(), 2.f);
  copy_layer->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());

  ASSERT_TRUE(copy_layer->HasCopyRequest());
  CommitAndActivate();
  ASSERT_FALSE(copy_layer->HasCopyRequest());

  EXPECT_EQ(gfx::Rect(50, 50), ImplOf(copy_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(copy_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10),
            ImplOf(copy_clipped_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(5, 5), ImplOf(copy_surface)->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, TransformedClipParent) {
  // Ensure that a transform between the layer and its render surface is not a
  // problem. Constructs the following layer tree.
  //
  // Virtual layer tree:
  //   root (a render surface)
  //     + render_surface
  //       + clip_parent (scaled)
  //         + intervening_clipping_layer
  //           + clip_child (clipped_by_clip_parent)
  //
  // The render surface should be resized correctly and the clip child should
  // inherit the right clip rect.
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(50, 50));

  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  render_surface->SetBounds(gfx::Size(10, 10));
  CopyProperties(root, render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;

  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  clip_parent->SetDrawsContent(true);
  clip_parent->SetBounds(gfx::Size(10, 10));
  CopyProperties(render_surface, clip_parent);
  auto& clip_parent_transform = CreateTransformNode(clip_parent);
  clip_parent_transform.local.Scale(2, 2);
  clip_parent_transform.post_translation = gfx::Vector2dF(1, 1);
  CreateClipNode(clip_parent);

  LayerImpl* intervening = AddLayerInActiveTree<LayerImpl>();
  intervening->SetDrawsContent(true);
  intervening->SetBounds(gfx::Size(5, 5));
  intervening->SetOffsetToTransformParent(gfx::Vector2dF(1, 1));
  CopyProperties(clip_parent, intervening);
  CreateClipNode(intervening);

  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();
  clip_child->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(10, 10));
  clip_child->SetOffsetToTransformParent(gfx::Vector2dF(2, 2));
  CopyProperties(intervening, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  UpdateActiveTreeDrawProperties();

  ASSERT_TRUE(GetRenderSurface(root));
  ASSERT_TRUE(GetRenderSurface(render_surface));

  // Ensure that we've inherited our clip parent's clip and weren't affected
  // by the intervening clip layer.
  ASSERT_EQ(gfx::Rect(1, 1, 20, 20), clip_parent->clip_rect());
  ASSERT_EQ(clip_parent->clip_rect(), clip_child->clip_rect());
  ASSERT_EQ(gfx::Rect(3, 3, 10, 10), intervening->clip_rect());

  // Ensure that the render surface reports a content rect that has been grown
  // to accomodate for the clip child.
  ASSERT_EQ(gfx::Rect(1, 1, 20, 20),
            GetRenderSurface(render_surface)->content_rect());

  // The above check implies the two below, but they nicely demonstrate that
  // we've grown, despite the intervening layer's clip.
  ASSERT_TRUE(clip_parent->clip_rect().Contains(
      GetRenderSurface(render_surface)->content_rect()));
  ASSERT_FALSE(intervening->clip_rect().Contains(
      GetRenderSurface(render_surface)->content_rect()));
}

TEST_F(DrawPropertiesTest, ClipParentWithInterveningRenderSurface) {
  // Ensure that intervening render surfaces are not a problem in the basic
  // case. In the following tree, both render surfaces should be resized to
  // accomodate for the clip child, despite an intervening clip.
  //
  // Virtual layer tree:
  //   root (a render surface)
  //    + clip_parent (clips)
  //      + render_surface1 (sets opacity)
  //        + intervening (clips)
  //          + render_surface2 (also sets opacity)
  //            + clip_child (clipped by clip_parent)
  //
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* intervening = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(50, 50));

  clip_parent->SetBounds(gfx::Size(40, 40));
  clip_parent->SetOffsetToTransformParent(gfx::Vector2dF(1, 1));
  CopyProperties(root, clip_parent);
  CreateClipNode(clip_parent);

  render_surface1->SetDrawsContent(true);
  render_surface1->SetBounds(gfx::Size(10, 10));
  CopyProperties(clip_parent, render_surface1);
  CreateTransformNode(render_surface1).post_translation = gfx::Vector2dF(1, 1);
  CreateEffectNode(render_surface1).render_surface_reason =
      RenderSurfaceReason::kTest;

  intervening->SetBounds(gfx::Size(5, 5));
  intervening->SetOffsetToTransformParent(gfx::Vector2dF(1, 1));
  CopyProperties(render_surface1, intervening);
  CreateClipNode(intervening);

  render_surface2->SetDrawsContent(true);
  render_surface2->SetBounds(gfx::Size(10, 10));
  CopyProperties(intervening, render_surface2);
  CreateTransformNode(render_surface2).post_translation = gfx::Vector2dF(1, 1);
  CreateEffectNode(render_surface2).render_surface_reason =
      RenderSurfaceReason::kTest;

  clip_child->SetBounds(gfx::Size(60, 60));
  clip_child->SetDrawsContent(true);
  clip_child->SetOffsetToTransformParent(gfx::Vector2dF(-10, -10));
  CopyProperties(render_surface2, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(GetRenderSurface(root));
  EXPECT_TRUE(GetRenderSurface(render_surface1));
  EXPECT_TRUE(GetRenderSurface(render_surface2));

  // render_surface1 should apply the clip from clip_parent. Though there is a
  // clip child, render_surface1 can apply the clip as there are no clips
  // between the clip parent and render_surface1
  EXPECT_EQ(gfx::Rect(1, 1, 40, 40),
            GetRenderSurface(render_surface1)->clip_rect());
  EXPECT_TRUE(GetRenderSurface(render_surface1)->is_clipped());
  EXPECT_EQ(gfx::Rect(), render_surface1->clip_rect());
  EXPECT_FALSE(render_surface1->is_clipped());

  // render_surface2 could have expanded, as there is a clip between
  // clip_child's clip (clip_parent) and render_surface2's clip (intervening).
  // So, it should not be clipped (their bounds would no longer be reliable).
  // We should resort to layer clipping in this case.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0),
            GetRenderSurface(render_surface2)->clip_rect());
  EXPECT_FALSE(GetRenderSurface(render_surface2)->is_clipped());

  // This value is inherited from the clipping ancestor layer, 'intervening'.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), render_surface2->clip_rect());
  EXPECT_TRUE(render_surface2->is_clipped());

  // The clip child should have inherited the clip parent's clip (projected to
  // the right space, of course), but as render_surface1 already applies that
  // clip, clip_child need not apply it again.
  EXPECT_EQ(gfx::Rect(), clip_child->clip_rect());
  EXPECT_EQ(gfx::Rect(9, 9, 40, 40), clip_child->visible_layer_rect());
  EXPECT_FALSE(clip_child->is_clipped());

  // The content rects of render_surface2 should have expanded to contain the
  // clip child, but only the visible part of the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40),
            GetRenderSurface(render_surface1)->content_rect());
  EXPECT_EQ(clip_child->visible_drawable_content_rect(),
            GetRenderSurface(render_surface2)->content_rect());
}

TEST_F(DrawPropertiesTest, ClipParentScrolledInterveningLayer) {
  // Ensure that intervening render surfaces are not a problem, even if there
  // is a scroll involved. Note, we do _not_ have to consider any other sort
  // of transform.
  //
  // Virtual layer tree:
  //   root (a render surface)
  //    + clip_parent (clips and transforms)
  //      + render_surface1 (has render surface)
  //        + intervening (clips AND scrolls)
  //          + render_surface2 (also has render surface)
  //            + clip_child (clipped by clip_parent)
  //
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* intervening = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(50, 50));

  clip_parent->SetBounds(gfx::Size(40, 40));
  CopyProperties(root, clip_parent);
  auto& clip_parent_transform = CreateTransformNode(clip_parent);
  clip_parent_transform.local.Translate(2, 2);
  clip_parent_transform.post_translation = gfx::Vector2dF(1, 1);
  CreateClipNode(clip_parent);

  render_surface1->SetDrawsContent(true);
  render_surface1->SetBounds(gfx::Size(10, 10));
  CopyProperties(clip_parent, render_surface1);
  CreateEffectNode(render_surface1).render_surface_reason =
      RenderSurfaceReason::kTest;

  intervening->SetBounds(gfx::Size(5, 5));
  intervening->SetElementId(LayerIdToElementIdForTesting(intervening->id()));
  CopyProperties(render_surface1, intervening);
  CreateTransformNode(intervening).post_translation = gfx::Vector2dF(1, 1);
  CreateScrollNode(intervening, gfx::Size(1, 1));
  CreateClipNode(intervening);

  render_surface2->SetDrawsContent(true);
  render_surface2->SetBounds(gfx::Size(10, 10));
  CopyProperties(intervening, render_surface2);
  CreateTransformNode(render_surface2);
  CreateEffectNode(render_surface2).render_surface_reason =
      RenderSurfaceReason::kTest;

  clip_child->SetBounds(gfx::Size(60, 60));
  clip_child->SetDrawsContent(true);
  clip_child->SetOffsetToTransformParent(gfx::Vector2dF(-10, -10));
  CopyProperties(render_surface2, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  SetScrollOffset(intervening, gfx::PointF(3, 3));
  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(GetRenderSurface(root));
  EXPECT_TRUE(GetRenderSurface(render_surface1));
  EXPECT_TRUE(GetRenderSurface(render_surface2));

  // render_surface1 should apply the clip from clip_parent. Though there is a
  // clip child, render_surface1 can apply the clip as there are no clips
  // between the clip parent and render_surface1
  EXPECT_EQ(gfx::Rect(3, 3, 40, 40),
            GetRenderSurface(render_surface1)->clip_rect());
  EXPECT_TRUE(GetRenderSurface(render_surface1)->is_clipped());
  EXPECT_EQ(gfx::Rect(), render_surface1->clip_rect());
  EXPECT_FALSE(render_surface1->is_clipped());

  // render_surface2 could have expanded, as there is a clip between
  // clip_child's clip (clip_parent) and render_surface2's clip (intervening).
  // So, it should not be clipped (their bounds would no longer be reliable).
  // We should resort to layer clipping in this case.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0),
            GetRenderSurface(render_surface2)->clip_rect());
  EXPECT_FALSE(GetRenderSurface(render_surface2)->is_clipped());
  // This value is inherited from the clipping ancestor layer, 'intervening'.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), render_surface2->clip_rect());
  EXPECT_TRUE(render_surface2->is_clipped());

  // The clip child should have inherited the clip parent's clip (projected to
  // the right space, of course), but as render_surface1 already applies that
  // clip, clip_child need not apply it again.
  EXPECT_EQ(gfx::Rect(), clip_child->clip_rect());
  EXPECT_EQ(gfx::Rect(12, 12, 40, 40), clip_child->visible_layer_rect());
  EXPECT_FALSE(clip_child->is_clipped());

  // The content rects of render_surface2 should have expanded to contain the
  // clip child, but only the visible part of the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40),
            GetRenderSurface(render_surface1)->content_rect());
  EXPECT_EQ(clip_child->visible_drawable_content_rect(),
            GetRenderSurface(render_surface2)->content_rect());
}

TEST_F(DrawPropertiesTest, DescendantsOfClipChildren) {
  // Ensures that descendants of the clip child inherit the correct clip.
  //
  // Virtual layer tree:
  //   root (a render surface)
  //    + clip_parent (clips)
  //      + intervening (clips)
  //        + clip_child (clipped by clip_parent, skipping intervening)
  //          + child
  //
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* intervening = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(50, 50));

  clip_parent->SetBounds(gfx::Size(40, 40));
  CopyProperties(root, clip_parent);
  CreateClipNode(clip_parent);

  intervening->SetBounds(gfx::Size(5, 5));
  CopyProperties(clip_parent, intervening);
  CreateClipNode(intervening);

  clip_child->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(60, 60));
  CopyProperties(intervening, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  child->SetDrawsContent(true);
  child->SetBounds(gfx::Size(60, 60));
  CopyProperties(clip_child, child);

  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(GetRenderSurface(root));

  // Neither the clip child nor its descendant should have inherited the clip
  // from |intervening|.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), clip_child->clip_rect());
  EXPECT_TRUE(clip_child->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), child->visible_layer_rect());
  EXPECT_TRUE(child->is_clipped());
}

TEST_F(DrawPropertiesTest,
       SurfacesShouldBeUnaffectedByNonDescendantClipChildren) {
  // Ensures that non-descendant clip children in the tree do not affect
  // render surfaces.
  //
  //   root (a render surface)
  //    + clip_parent (clips)
  //      + clip_layer (clips)
  //        + render_surface1
  //          + clip_child (clipped by clip_parent)
  //        + render_surface2
  //          + non_clip_child (in normal clip hierarchy)
  //
  // In this example render_surface2 should be unaffected by clip_child.
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface_layer1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface_layer2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* non_clip_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(15, 15));
  clip_parent->SetBounds(gfx::Size(10, 10));
  clip_layer->SetBounds(gfx::Size(10, 10));
  render_surface_layer1->SetDrawsContent(true);
  render_surface_layer1->SetBounds(gfx::Size(5, 5));
  render_surface_layer2->SetDrawsContent(true);
  render_surface_layer2->SetBounds(gfx::Size(5, 5));
  clip_child->SetDrawsContent(true);
  clip_child->SetOffsetToTransformParent(gfx::Vector2dF(-1, 1));
  clip_child->SetBounds(gfx::Size(10, 10));
  non_clip_child->SetDrawsContent(true);
  non_clip_child->SetBounds(gfx::Size(5, 5));

  CopyProperties(root, clip_parent);
  CreateClipNode(clip_parent);
  CopyProperties(clip_parent, clip_layer);
  CreateClipNode(clip_layer);
  CopyProperties(clip_layer, render_surface_layer1);
  CreateTransformNode(render_surface_layer1).post_translation =
      gfx::Vector2dF(5, 5);
  CreateEffectNode(render_surface_layer1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(clip_layer, render_surface_layer2);
  CreateTransformNode(render_surface_layer2).post_translation =
      gfx::Vector2dF(5, 5);
  CreateEffectNode(render_surface_layer2).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface_layer1, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());
  CopyProperties(render_surface_layer2, non_clip_child);

  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(GetRenderSurface(root));
  auto* render_surface1 = GetRenderSurface(render_surface_layer1);
  ASSERT_TRUE(render_surface1);
  auto* render_surface2 = GetRenderSurface(render_surface_layer2);
  ASSERT_TRUE(render_surface2);

  EXPECT_EQ(gfx::Rect(-5, -5, 10, 10), render_surface_layer1->clip_rect());
  EXPECT_TRUE(render_surface_layer1->is_clipped());

  // The render should clip to clip_layer (it has unclipped descendants),
  // instead it should rely on layer clipping.
  EXPECT_TRUE(render_surface1->has_contributing_layer_that_escapes_clip());
  EXPECT_EQ(clip_layer->clip_tree_index(), render_surface1->ClipTreeIndex());
  if (base::FeatureList::IsEnabled(
          features::kRenderSurfaceCommonAncestorClip)) {
    EXPECT_EQ(clip_parent->clip_tree_index(),
              render_surface1->common_ancestor_clip_id());
    EXPECT_TRUE(render_surface1->is_clipped());
    EXPECT_EQ(gfx::Rect(0, 0, 10, 10), render_surface1->clip_rect());
  } else {
    EXPECT_EQ(clip_layer->clip_tree_index(),
              render_surface1->common_ancestor_clip_id());
    EXPECT_FALSE(render_surface1->is_clipped());
    EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface1->clip_rect());
  }

  // That said, it should have grown to accommodate the unclipped descendant
  // and its own size.
  EXPECT_EQ(gfx::Rect(-1, 0, 6, 5), render_surface1->content_rect());

  // This render surface should clip. It has no unclipped descendants.
  EXPECT_FALSE(render_surface2->has_contributing_layer_that_escapes_clip());
  EXPECT_EQ(clip_layer->clip_tree_index(), render_surface2->ClipTreeIndex());
  EXPECT_EQ(clip_layer->clip_tree_index(),
            render_surface2->common_ancestor_clip_id());
  EXPECT_TRUE(render_surface2->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), render_surface2->clip_rect());
  EXPECT_FALSE(render_surface_layer2->is_clipped());

  // It also shouldn't have grown to accommodate the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5), render_surface2->content_rect());
}

TEST_F(DrawPropertiesTest, TransformAnimationUpdatesBackfaceVisibility) {
  LayerImpl* root = root_layer();
  root->SetDrawsContent(true);
  LayerImpl* back_facing = AddLayerInActiveTree<LayerImpl>();
  back_facing->SetDrawsContent(true);
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  render_surface1->SetDrawsContent(true);
  LayerImpl* render_surface2 = AddLayerInActiveTree<LayerImpl>();
  render_surface2->SetDrawsContent(true);
  gfx::Transform rotate_about_y;
  rotate_about_y.RotateAboutYAxis(180.0);

  root->SetBounds(gfx::Size(50, 50));
  back_facing->SetBounds(gfx::Size(50, 50));
  render_surface1->SetBounds(gfx::Size(30, 30));
  render_surface2->SetBounds(gfx::Size(30, 30));
  SetElementIdsForTesting();

  CopyProperties(root, back_facing);
  auto& back_facing_transform_node = CreateTransformNode(back_facing);
  back_facing_transform_node.flattens_inherited_transform = false;
  back_facing_transform_node.sorting_context_id = 1;
  back_facing_transform_node.local = rotate_about_y;
  CopyProperties(back_facing, render_surface1);
  auto& render_surface1_transform_node = CreateTransformNode(render_surface1);
  render_surface1_transform_node.flattens_inherited_transform = false;
  render_surface1_transform_node.sorting_context_id = 1;
  auto& render_surface1_effect_node = CreateEffectNode(render_surface1);
  render_surface1_effect_node.render_surface_reason =
      RenderSurfaceReason::kTest;
  render_surface1_effect_node.double_sided = false;
  CopyProperties(back_facing, render_surface2);
  auto& render_surface2_transform_node = CreateTransformNode(render_surface2);
  render_surface2_transform_node.flattens_inherited_transform = false;
  render_surface2_transform_node.sorting_context_id = 1;
  auto& render_surface2_effect_node = CreateEffectNode(render_surface2);
  render_surface2_effect_node.render_surface_reason =
      RenderSurfaceReason::kTest;
  render_surface2_effect_node.double_sided = false;

  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(GetEffectNode(render_surface1)->hidden_by_backface_visibility);
  EXPECT_EQ(gfx::Rect(), render_surface1->visible_layer_rect());
  EXPECT_TRUE(GetEffectNode(render_surface2)->hidden_by_backface_visibility);
  EXPECT_EQ(gfx::Rect(), render_surface2->visible_layer_rect());

  EXPECT_EQ(1u, GetRenderSurfaceList().size());

  root->layer_tree_impl()->SetTransformMutated(back_facing->element_id(),
                                               gfx::Transform());
  root->layer_tree_impl()->SetTransformMutated(render_surface2->element_id(),
                                               rotate_about_y);
  UpdateActiveTreeDrawProperties();
  EXPECT_FALSE(GetEffectNode(render_surface1)->hidden_by_backface_visibility);
  EXPECT_EQ(gfx::Rect(0, 0, 30, 30), render_surface1->visible_layer_rect());
  EXPECT_TRUE(GetEffectNode(render_surface2)->hidden_by_backface_visibility);
  EXPECT_EQ(gfx::Rect(), render_surface2->visible_layer_rect());

  EXPECT_EQ(2u, GetRenderSurfaceList().size());

  root->layer_tree_impl()->SetTransformMutated(render_surface1->element_id(),
                                               rotate_about_y);
  UpdateActiveTreeDrawProperties();
  EXPECT_TRUE(GetEffectNode(render_surface1)->hidden_by_backface_visibility);
  // Draw properties are only updated for visible layers, so this remains the
  // cached value from last time. The expectation is commented out because
  // this result is not required.
  //  EXPECT_EQ(gfx::Rect(0, 0, 30, 30), render_surface1->visible_layer_rect());
  EXPECT_TRUE(GetEffectNode(render_surface2)->hidden_by_backface_visibility);
  EXPECT_EQ(gfx::Rect(), render_surface2->visible_layer_rect());

  EXPECT_EQ(1u, GetRenderSurfaceList().size());
}

TEST_F(DrawPropertiesTest, ScrollChildAndScrollParentDifferentTargets) {
  // Tests the computation of draw transform for the scroll child when its
  // render surface is different from its scroll parent's render surface.
  LayerImpl* root = root_layer();
  LayerImpl* scroll_child_target = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* scroll_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* scroll_parent_target = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* scroll_parent = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(50, 50));
  scroll_child_target->SetBounds(gfx::Size(50, 50));
  scroll_parent_target->SetBounds(gfx::Size(50, 50));
  scroll_parent->SetBounds(gfx::Size(50, 50));
  scroll_parent->SetDrawsContent(true);
  scroll_child->SetBounds(gfx::Size(50, 50));
  scroll_child->SetDrawsContent(true);

  CopyProperties(root, scroll_child_target);
  auto& child_target_effect_node = CreateEffectNode(scroll_child_target);
  child_target_effect_node.render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(scroll_child_target, scroll_parent_target);
  CreateTransformNode(scroll_parent_target).post_translation =
      gfx::Vector2dF(10, 10);
  CreateScrollNode(scroll_parent_target, gfx::Size());
  CreateEffectNode(scroll_parent_target).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(scroll_parent_target);
  CopyProperties(scroll_parent_target, scroll_parent);
  // Let |scroll_child| inherit |scroll_parent|'s transform/clip/scroll states,
  CopyProperties(scroll_parent, scroll_child);
  // and |scroll_child_target|'s effect state.
  scroll_child->SetEffectTreeIndex(scroll_child_target->effect_tree_index());
  scroll_child->SetOffsetToTransformParent(gfx::Vector2dF(-10, -10));

  float device_scale_factor = 1.5f;
  UpdateActiveTreeDrawProperties(device_scale_factor);
  EXPECT_EQ(scroll_child->effect_tree_index(),
            scroll_child_target->effect_tree_index());
  EXPECT_EQ(scroll_child->visible_layer_rect(), gfx::Rect(10, 10, 40, 40));
  EXPECT_EQ(scroll_child->clip_rect(), gfx::Rect(15, 15, 75, 75));
  gfx::Transform scale;
  scale.Scale(device_scale_factor, device_scale_factor);
  EXPECT_TRANSFORM_EQ(scroll_child->DrawTransform(), scale);
}

TEST_F(DrawPropertiesTest, SingularTransformSubtreesDoNotDraw) {
  LayerImpl* root = root_layer();
  LayerImpl* parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(50, 50));
  root->SetDrawsContent(true);
  parent->SetBounds(gfx::Size(30, 30));
  parent->SetDrawsContent(true);
  child->SetBounds(gfx::Size(20, 20));
  child->SetDrawsContent(true);

  CopyProperties(root, parent);
  CreateTransformNode(parent).sorting_context_id = 1;
  CreateEffectNode(parent).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(parent, child);
  CreateTransformNode(child).sorting_context_id = 1;
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(3u, GetRenderSurfaceList().size());

  gfx::Transform singular_transform;
  singular_transform.Scale3d(SkDoubleToScalar(1.0), SkDoubleToScalar(1.0),
                             SkDoubleToScalar(0.0));

  SetTransform(child, singular_transform);
  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(2u, GetRenderSurfaceList().size());

  // Ensure that the entire subtree under a layer with singular transform does
  // not get rendered.
  SetTransform(parent, singular_transform);
  SetTransform(child, gfx::Transform());
  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(1u, GetRenderSurfaceList().size());
}

TEST_F(DrawPropertiesTest, ScrollSnapping) {
  // This test verifies that a scrolling layer gets scroll offset snapped to
  // integer coordinates.
  //
  // Virtual layer hierarchy:
  // + root
  //   + container
  //     + scroller
  //
  LayerImpl* root = root_layer();
  LayerImpl* container = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* scroller = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(50, 50));

  container->SetBounds(gfx::Size(40, 40));
  container->SetDrawsContent(true);
  CopyProperties(root, container);

  gfx::Vector2dF container_offset(10, 20);

  scroller->SetElementId(LayerIdToElementIdForTesting(scroller->id()));
  scroller->SetBounds(gfx::Size(30, 30));
  scroller->SetDrawsContent(true);
  CopyProperties(container, scroller);
  CreateTransformNode(scroller).post_translation = container_offset;
  CreateScrollNode(scroller, container->bounds());

  // Rounded to integers already.
  {
    gfx::Vector2dF scroll_delta(3.0, 5.0);
    SetScrollOffsetDelta(scroller, scroll_delta);
    UpdateActiveTreeDrawProperties();

    EXPECT_VECTOR2DF_EQ(
        scroller->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset - scroll_delta);
  }

  // Scroll delta requiring rounding.
  {
    gfx::Vector2dF scroll_delta(4.1f, 8.1f);
    SetScrollOffsetDelta(scroller, scroll_delta);
    UpdateActiveTreeDrawProperties();

    gfx::Vector2dF rounded_scroll_delta(4.f, 8.f);
    EXPECT_VECTOR2DF_EQ(
        scroller->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset - rounded_scroll_delta);
  }
}

TEST_F(DrawPropertiesTest, ScrollSnappingWithAnimatedScreenSpaceTransform) {
  // This test verifies that a scrolling layer whose screen space transform is
  // animating doesn't get snapped to integer coordinates.
  //
  // Virtual layer hierarchy:
  // + root
  //   + animated layer
  //     + surface
  //       + container
  //         + scroller
  //
  LayerImpl* root = root_layer();
  LayerImpl* animated_layer = AddLayerInActiveTree<FakePictureLayerImpl>();
  LayerImpl* surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* container = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* scroller = AddLayerInActiveTree<LayerImpl>();
  SetElementIdsForTesting();

  root->SetBounds(gfx::Size(50, 50));

  gfx::Transform start_scale;
  start_scale.Scale(1.5f, 1.5f);

  animated_layer->SetBounds(gfx::Size(50, 50));
  CopyProperties(root, animated_layer);
  CreateTransformNode(animated_layer).local = start_scale;

  surface->SetBounds(gfx::Size(50, 50));
  CopyProperties(animated_layer, surface);
  CreateEffectNode(surface).render_surface_reason = RenderSurfaceReason::kTest;

  container->SetBounds(gfx::Size(50, 50));
  CopyProperties(surface, container);

  scroller->SetBounds(gfx::Size(100, 100));
  scroller->SetDrawsContent(true);
  CopyProperties(container, scroller);
  CreateTransformNode(scroller);
  CreateScrollNode(scroller, container->bounds());

  gfx::Transform end_scale;
  end_scale.Scale(2.f, 2.f);
  gfx::TransformOperations start_operations;
  start_operations.AppendMatrix(start_scale);
  gfx::TransformOperations end_operations;
  end_operations.AppendMatrix(end_scale);

  AddAnimatedTransformToElementWithAnimation(animated_layer->element_id(),
                                             timeline_impl(), 1.0,
                                             start_operations, end_operations);

  gfx::Vector2dF scroll_delta(5.f, 9.f);
  SetScrollOffsetDelta(scroller, scroll_delta);

  UpdateActiveTreeDrawProperties();

  gfx::Vector2dF expected_draw_transform_translation(-7.5f, -13.5f);
  EXPECT_VECTOR2DF_EQ(expected_draw_transform_translation,
                      scroller->DrawTransform().To2dTranslation());
}

TEST_F(DrawPropertiesTest, ScrollSnappingWithScrollChild) {
  // This test verifies that a scrolling child of a scrolling layer doesn't get
  // snapped to integer coordinates.
  //
  // Virtual layer hierarchy:
  // + root
  //   + container
  //     + scroller
  //   + scroll_child (transform parent is scroller)
  //
  LayerImpl* root = root_layer();
  LayerImpl* container = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* scroller = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* scroll_child = AddLayerInActiveTree<LayerImpl>();
  SetElementIdsForTesting();

  root->SetBounds(gfx::Size(50, 50));

  container->SetBounds(gfx::Size(50, 50));
  CopyProperties(root, container);
  gfx::Vector2dF container_offset(10.3f, 10.3f);

  scroller->SetBounds(gfx::Size(100, 100));
  CopyProperties(container, scroller);
  CreateTransformNode(scroller).post_translation = container_offset;
  CreateScrollNode(scroller, container->bounds());

  scroll_child->SetBounds(gfx::Size(10, 10));
  CopyProperties(root, scroll_child);
  auto& scroll_child_transform =
      CreateTransformNode(scroll_child, scroller->transform_tree_index());
  scroll_child_transform.local.RotateAboutYAxis(30);
  scroll_child_transform.post_translation = -container_offset;

  gfx::Vector2dF scroll_delta(5.f, 9.f);
  SetScrollOffsetDelta(scroller, scroll_delta);
  UpdateActiveTreeDrawProperties();

  gfx::Vector2dF expected_scroller_screen_space_transform_translation(5.f, 1.f);
  EXPECT_VECTOR2DF_EQ(expected_scroller_screen_space_transform_translation,
                      scroller->ScreenSpaceTransform().To2dTranslation());

  gfx::Transform expected_scroll_child_screen_space_transform;
  expected_scroll_child_screen_space_transform.Translate(-5.3f, -9.3f);
  expected_scroll_child_screen_space_transform.RotateAboutYAxis(30);
  EXPECT_TRANSFORM_EQ(expected_scroll_child_screen_space_transform,
                      scroll_child->ScreenSpaceTransform());
}

class DrawPropertiesStickyPositionTest : public DrawPropertiesTest {
 protected:
  // Setup layers and property trees.
  // Virtual layer hierarchy:
  // + root
  //   + container
  //     + scroller
  //       + sticky_pos
  void CreateTree() {
    CreateRootAndScroller();
    sticky_pos_ = CreateSticky(scroller_.get());
  }

  void CreateRootAndScroller() {
    root_ = Layer::Create();
    container_ = Layer::Create();
    scroller_ = Layer::Create();
    scroller_->SetElementId(LayerIdToElementIdForTesting(scroller_->id()));

    root_->SetBounds(gfx::Size(100, 100));
    host()->SetRootLayer(root_);
    SetupRootProperties(root_.get());

    container_->SetBounds(gfx::Size(100, 100));
    CopyProperties(root_.get(), container_.get());
    root_->AddChild(container_);

    scroller_->SetBounds(gfx::Size(1000, 1000));
    CopyProperties(container_.get(), scroller_.get());
    CreateTransformNode(scroller_.get());
    CreateScrollNode(scroller_.get(), container_->bounds());
    root_->AddChild(scroller_);
  }

  scoped_refptr<Layer> CreateSticky(Layer* parent) {
    scoped_refptr<Layer> sticky = Layer::Create();
    sticky->SetBounds(gfx::Size(10, 10));
    CopyProperties(parent, sticky.get());
    CreateTransformNode(sticky.get());
    EnsureStickyData(sticky.get()).scroll_ancestor =
        parent->scroll_tree_index();
    root_->AddChild(sticky);
    return sticky;
  }

  void CommitAndUpdateImplPointers() {
    UpdateMainDrawProperties();
    host_impl()->CreatePendingTree();
    host()->CommitToPendingTree();
    host_impl()->ActivateSyncTree();
    LayerTreeImpl* layer_tree_impl = host_impl()->active_tree();
    root_impl_ = layer_tree_impl->LayerById(root_->id());
    scroller_impl_ = layer_tree_impl->LayerById(scroller_->id());
    sticky_pos_impl_ = layer_tree_impl->LayerById(sticky_pos_->id());
  }

  StickyPositionNodeData& EnsureStickyData(Layer* layer) {
    return GetPropertyTrees(layer)
        ->transform_tree_mutable()
        .EnsureStickyPositionData(layer->transform_tree_index());
  }

  scoped_refptr<Layer> root_;
  scoped_refptr<Layer> container_;
  scoped_refptr<Layer> scroller_;
  scoped_refptr<Layer> sticky_pos_;
  raw_ptr<LayerImpl> root_impl_;
  raw_ptr<LayerImpl> scroller_impl_;
  raw_ptr<LayerImpl> sticky_pos_impl_;
};

TEST_F(DrawPropertiesStickyPositionTest, StickyPositionTop) {
  CreateTree();

  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(10, 20));
  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 10.0f;
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(10, 20, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 50, 50);

  CommitAndUpdateImplPointers();

  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10.f, 20.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(5.f, 5.f));

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(5.f, 15.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the Y coordinate should now be clamped.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(15.f, 15.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(15.f, 25.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the end of the sticky container (note: this element does not
  // have its own layer as it does not need to be composited).
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(15.f, 50.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, -10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(DrawPropertiesStickyPositionTest, StickyPositionTopRounded) {
  CreateTree();

  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(10, 20));
  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 10.5f;
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(10, 20, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 50, 50);

  CommitAndUpdateImplPointers();

  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10.f, 20.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(5.f, 5.f));

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(5.f, 15.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the Y coordinate should now be clamped.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(15.f, 15.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 11.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(15.f, 25.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 11.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the end of the sticky container (note: this element does not
  // have its own layer as it does not need to be composited).
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(15.f, 50.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, -10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(DrawPropertiesStickyPositionTest, StickyPositionSubpixelScroll) {
  CreateTree();

  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(0, 200));
  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_bottom = true;
  sticky_position.bottom_offset = 10.0f;
  sticky_position.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(0, 200, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 100, 500);

  CommitAndUpdateImplPointers();

  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 0.8f));

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 80.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(DrawPropertiesStickyPositionTest, StickyPositionBottom) {
  CreateTree();

  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(0, 150));
  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_bottom = true;
  sticky_position.bottom_offset = 10.0f;
  sticky_position.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(0, 150, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 100, 50, 50);

  CommitAndUpdateImplPointers();

  // Initially the sticky element is moved up to the top of the container.
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 100.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 5.f));

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 95.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Once we get past the top of the container it moves to be aligned 10px
  // up from the the bottom of the scroller.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 25.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 80.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 30.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 80.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Once we scroll past its initial location, it sticks there.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 150.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(DrawPropertiesStickyPositionTest, StickyPositionBottomRounded) {
  CreateTree();

  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(0, 150));
  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_bottom = true;
  sticky_position.bottom_offset = 10.5f;
  sticky_position.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(0, 150, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 100, 50, 50);

  CommitAndUpdateImplPointers();

  // Initially the sticky element is moved up to the top of the container.
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 100.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 5.f));

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 95.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Once we get past the top of the container it moves to be aligned 10px
  // up from the the bottom of the scroller.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 25.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 79.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 30.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 79.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(DrawPropertiesStickyPositionTest,
       StickyPositionBottomOuterViewportDelta) {
  CreateTree();

  GetScrollNode(scroller_.get())->scrolls_outer_viewport = true;

  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(0, 70));
  GetPropertyTrees(sticky_pos_.get())
      ->transform_tree_mutable()
      .AddNodeAffectedByOuterViewportBoundsDelta(
          sticky_pos_->transform_tree_index());
  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_bottom = true;
  sticky_position.bottom_offset = 10.0f;
  sticky_position.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(0, 70, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 60, 100, 100);

  CommitAndUpdateImplPointers();

  // Initially the sticky element is moved to the bottom of the container.
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 70.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // We start to hide the toolbar, but not far enough that the sticky element
  // should be moved up yet.
  GetPropertyTrees(scroller_impl_)
      ->SetOuterViewportContainerBoundsDelta(gfx::Vector2dF(0.f, -10.f));

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 70.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // On hiding more of the toolbar the sticky element starts to stick.
  GetPropertyTrees(scroller_impl_)
      ->SetOuterViewportContainerBoundsDelta(gfx::Vector2dF(0.f, -20.f));
  UpdateActiveTreeDrawProperties();

  // On hiding more the sticky element stops moving as it has reached its
  // limit.
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 60.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  GetPropertyTrees(scroller_impl_)
      ->SetOuterViewportContainerBoundsDelta(gfx::Vector2dF(0.f, -30.f));
  UpdateActiveTreeDrawProperties();

  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 60.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(DrawPropertiesStickyPositionTest, StickyPositionLeftRight) {
  CreateTree();

  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(145, 0));
  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_left = true;
  sticky_position.is_anchored_right = true;
  sticky_position.left_offset = 10.0f;
  sticky_position.right_offset = 10.0f;
  sticky_position.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(145, 0, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(100, 0, 100, 100);

  CommitAndUpdateImplPointers();

  // Initially the sticky element is moved the leftmost side of the container.

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(100.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(5.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(95.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Once we get past the left side of the container it moves to be aligned 10px
  // up from the the right of the scroller.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(25.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(80.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(30.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(80.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // When we get to the sticky element's original position we stop sticking
  // to the right.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(95.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(50.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(105.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(40.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // The element starts sticking to the left once we scroll far enough.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(150.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(155.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Finally it stops sticking when it hits the right side of the container.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(190.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(195.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

// This test ensures that the compositor sticky position code correctly accounts
// for the element having been given a position from the main thread sticky
// position code.
TEST_F(DrawPropertiesStickyPositionTest, StickyPositionMainThreadUpdates) {
  CreateTree();

  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(10, 20));
  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 10.0f;
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(10, 20, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 50, 50);

  CommitAndUpdateImplPointers();

  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10.f, 20.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(5.f, 5.f));

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(5.f, 15.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the Y coordinate should now be clamped.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(15.f, 15.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Now the main thread commits the new position of the sticky element.
  SetScrollOffset(scroller_.get(), gfx::PointF(15, 15));
  // Shift the layer by -offset_for_position_sticky.
  SetPostTranslation(sticky_pos_.get(),
                     gfx::PointF(10, 25) - gfx::PointF(0, 5));
  GetPropertyTrees(scroller_.get())
      ->transform_tree_mutable()
      .set_needs_update(true);

  CommitAndUpdateImplPointers();

  // The element should still be where it was before. We reset the delta to
  // (0, 0) because we have synced a scroll offset of (15, 15) from the main
  // thread.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // And if we scroll a little further it remains there.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 10.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5.f, 10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

// This tests the main thread updates with a composited sticky container. In
// this case the position received from main is relative to the container but
// the constraint rects are relative to the ancestor scroller.
TEST_F(DrawPropertiesStickyPositionTest, StickyPositionCompositedContainer) {
  CreateRootAndScroller();

  scoped_refptr<Layer> sticky_container = Layer::Create();
  sticky_container->SetBounds(gfx::Size(30, 30));
  sticky_container->SetOffsetToTransformParent(gfx::Vector2dF(20, 20));
  CopyProperties(scroller_.get(), sticky_container.get());
  root_->AddChild(sticky_container);

  sticky_pos_ = CreateSticky(sticky_container.get());
  SetPostTranslation(
      sticky_pos_.get(),
      gfx::Vector2dF(0, 10) + sticky_container->offset_to_transform_parent());
  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 10.0f;
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(20, 30, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(20, 20, 30, 30);

  CommitAndUpdateImplPointers();

  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 30.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 5.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 25.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the Y coordinate should now be clamped.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 25.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Now the main thread commits the new position of the sticky element.
  SetScrollOffset(scroller_.get(), gfx::PointF(0, 25));
  // Shift the layer by -offset_for_position_sticky.
  SetPostTranslation(sticky_pos_.get(),
                     gfx::PointF(0, 15) - gfx::PointF(0, 5) +
                         sticky_container->offset_to_transform_parent());
  CommitAndUpdateImplPointers();

  // The element should still be where it was before. We reset the delta to
  // (0, 0) because we have synced a scroll offset of (0, 25) from the main
  // thread.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 0.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // And if we scroll a little further it remains there.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 5.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // And hits the bottom of the container.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 10.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(20.f, 5.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

// A transform on a sticky element should not affect its sticky position.
TEST_F(DrawPropertiesStickyPositionTest, StickyPositionScaledStickyBox) {
  CreateTree();

  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(0, 20));
  gfx::Transform scale;
  scale.Scale(2, 2);
  SetTransform(sticky_pos_.get(), scale);

  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 0.0f;
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(0, 20, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 50, 50);

  CommitAndUpdateImplPointers();

  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 20.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 15.f));

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 5.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the box is positioned at the scroller
  // edge.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 25.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 30.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 0.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the end of the sticky container.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 50.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, -10.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

// Tests that a transform does not affect the sticking points. The sticky
// element will however move relative to the viewport due to its transform.
TEST_F(DrawPropertiesStickyPositionTest, StickyPositionScaledContainer) {
  CreateRootAndScroller();

  scoped_refptr<Layer> sticky_container = Layer::Create();
  sticky_container->SetBounds(gfx::Size(50, 50));
  CopyProperties(scroller_.get(), sticky_container.get());
  CreateTransformNode(sticky_container.get()).local.Scale(2, 2);
  root_->AddChild(sticky_container);

  sticky_pos_ = CreateSticky(sticky_container.get());
  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(0, 20));
  auto& sticky_position = EnsureStickyData(sticky_pos_.get()).constraints;
  sticky_position.is_anchored_top = true;
  sticky_position.top_offset = 0.0f;
  sticky_position.scroll_container_relative_sticky_box_rect =
      gfx::RectF(0, 20, 10, 10);
  sticky_position.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 50, 50);

  CommitAndUpdateImplPointers();

  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 40.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than sticking point, sticky element should move with scroll as
  // we haven't gotten to the initial sticky item location yet.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 15.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 25.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the sticking point, the box is positioned at the scroller
  // edge but is also scaled by its container so it begins to move down.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 25.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 25.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 30.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 30.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());

  // Scroll past the end of the sticky container.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 50.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 30.f),
      sticky_pos_impl_->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(DrawPropertiesStickyPositionTest, StickyPositionNested) {
  CreateTree();

  SetPostTranslation(sticky_pos_.get(), gfx::Vector2dF(0, 50));
  auto& outer_sticky_pos = EnsureStickyData(sticky_pos_.get()).constraints;
  outer_sticky_pos.is_anchored_top = true;
  outer_sticky_pos.top_offset = 10.0f;
  outer_sticky_pos.scroll_container_relative_sticky_box_rect =
      gfx::RectF(0, 50, 10, 50);
  outer_sticky_pos.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 50, 400);

  scoped_refptr<Layer> inner_sticky = CreateSticky(sticky_pos_.get());
  auto& inner_sticky_pos = EnsureStickyData(inner_sticky.get()).constraints;
  inner_sticky_pos.is_anchored_top = true;
  inner_sticky_pos.top_offset = 25.0f;
  inner_sticky_pos.scroll_container_relative_sticky_box_rect =
      gfx::RectF(0, 50, 10, 10);
  inner_sticky_pos.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 50, 10, 50);
  EnsureStickyData(inner_sticky.get()).nearest_node_shifting_containing_block =
      sticky_pos_->transform_tree_index();

  CommitAndUpdateImplPointers();
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();
  LayerImpl* outer_sticky_impl = sticky_pos_impl_;
  LayerImpl* inner_sticky_impl = layer_tree_impl->LayerById(inner_sticky->id());

  // Before any scrolling is done, the sticky elements should still be at their
  // original positions.
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 50.f),
      outer_sticky_impl->ScreenSpaceTransform().To2dTranslation());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 50.f),
      inner_sticky_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll less than the sticking point. Both sticky elements should move with
  // scroll as we haven't gotten to the sticky item locations yet.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 5.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 45.f),
      outer_sticky_impl->ScreenSpaceTransform().To2dTranslation());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 45.f),
      inner_sticky_impl->ScreenSpaceTransform().To2dTranslation());

  // Scroll such that the inner sticky should stick, but the outer one should
  // keep going as it hasn't reached its position yet.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 30.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 20.f),
      outer_sticky_impl->ScreenSpaceTransform().To2dTranslation());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 25.f),
      inner_sticky_impl->ScreenSpaceTransform().To2dTranslation());

  // Keep going, both should stick.
  SetScrollOffsetDelta(scroller_impl_, gfx::Vector2dF(0.f, 100.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 10.f),
      outer_sticky_impl->ScreenSpaceTransform().To2dTranslation());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0.f, 25.f),
      inner_sticky_impl->ScreenSpaceTransform().To2dTranslation());
}

class DrawPropertiesAnchorPositionScrollTest : public DrawPropertiesTest {
 protected:
  void CreateRoot() {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(100, 100));
    host()->SetRootLayer(root_);
    SetupRootProperties(root_.get());
  }

  std::pair<scoped_refptr<Layer>, scoped_refptr<Layer>> CreateScroller(
      Layer* parent) {
    scoped_refptr<Layer> container = Layer::Create();
    scoped_refptr<Layer> scroller = Layer::Create();
    scroller->SetElementId(LayerIdToElementIdForTesting(scroller->id()));

    container->SetBounds(gfx::Size(100, 100));
    CopyProperties(parent, container.get());
    root_->AddChild(container);

    scroller->SetBounds(gfx::Size(1000, 1000));
    CopyProperties(container.get(), scroller.get());
    CreateTransformNode(scroller.get());
    CreateScrollNode(scroller.get(), container->bounds());
    root_->AddChild(scroller);

    return std::make_pair(std::move(container), std::move(scroller));
  }

  scoped_refptr<Layer> CreateAnchored(
      Layer* parent,
      std::vector<ElementId> adjustment_container_ids) {
    scoped_refptr<Layer> anchored = Layer::Create();
    anchored->SetElementId(LayerIdToElementIdForTesting(anchored->id()));
    anchored->SetBounds(gfx::Size(10, 10));
    CopyProperties(parent, anchored.get());
    CreateTransformNode(anchored.get());
    auto& data =
        GetPropertyTrees(anchored.get())
            ->transform_tree_mutable()
            .EnsureAnchorPositionScrollData(anchored->transform_tree_index());
    data.needs_scroll_adjustment_in_x = true;
    data.needs_scroll_adjustment_in_y = true;
    data.adjustment_container_ids = std::move(adjustment_container_ids);
    root_->AddChild(anchored);
    return anchored;
  }

  void Commit() {
    UpdateMainDrawProperties();
    host_impl()->CreatePendingTree();
    host()->CommitToPendingTree();
    host_impl()->ActivateSyncTree();
  }

  LayerImpl* GetImpl(Layer* layer) {
    LayerTreeImpl* layer_tree_impl = host_impl()->active_tree();
    return layer_tree_impl->LayerById(layer->id());
  }

  scoped_refptr<Layer> root_;
};

TEST_F(DrawPropertiesAnchorPositionScrollTest, Basics) {
  // Virtual layer hierarchy:
  // + root
  //   + container
  //     + scroller <-- anchor
  //   + anchored
  CreateRoot();

  scoped_refptr<Layer> container;
  scoped_refptr<Layer> scroller;
  std::tie(container, scroller) = CreateScroller(root_.get());

  scoped_refptr<Layer> anchored =
      CreateAnchored(root_.get(), {scroller->element_id()});

  SetPostTranslation(anchored.get(), gfx::Vector2dF(10, 20));
  Commit();

  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10, 20),
      GetImpl(anchored.get())->ScreenSpaceTransform().To2dTranslation());

  // Scroll the scroller. Anchored element should always move with it.

  SetScrollOffsetDelta(GetImpl(scroller.get()), gfx::Vector2dF(5, 5));

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(5, 15),
      GetImpl(anchored.get())->ScreenSpaceTransform().To2dTranslation());

  SetScrollOffsetDelta(GetImpl(scroller.get()), gfx::Vector2dF(15, 25));

  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5, -5),
      GetImpl(anchored.get())->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(DrawPropertiesAnchorPositionScrollTest, NestedScrollers) {
  // Virtual layer hierarchy:
  // + root
  //   + container1
  //     + scroller1
  //       + container2
  //         + scroller2
  //           + container3
  //             + scroller3 <-- anchor
  //       + anchored
  CreateRoot();

  scoped_refptr<Layer> container1;
  scoped_refptr<Layer> scroller1;
  std::tie(container1, scroller1) = CreateScroller(root_.get());

  scoped_refptr<Layer> container2;
  scoped_refptr<Layer> scroller2;
  std::tie(container2, scroller2) = CreateScroller(scroller1.get());

  scoped_refptr<Layer> container3;
  scoped_refptr<Layer> scroller3;
  std::tie(container3, scroller3) = CreateScroller(scroller2.get());

  scoped_refptr<Layer> anchored = CreateAnchored(
      scroller1.get(), {scroller3->element_id(), scroller2->element_id()});

  SetPostTranslation(anchored.get(), gfx::Vector2dF(10, 20));
  Commit();

  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(10, 20),
      GetImpl(anchored.get())->ScreenSpaceTransform().To2dTranslation());

  // Scrolling scroller3 will apply the same translation offset to the anchored
  // element even if it's not a descendant of scroller 3.
  SetScrollOffsetDelta(GetImpl(scroller3.get()), gfx::Vector2dF(5, 5));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(5, 15),
      GetImpl(anchored.get())->ScreenSpaceTransform().To2dTranslation());

  // Same to scroller 2.
  SetScrollOffsetDelta(GetImpl(scroller2.get()), gfx::Vector2dF(10, 15));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-5, 0),
      GetImpl(anchored.get())->ScreenSpaceTransform().To2dTranslation());

  // Scrolling scroller1 natually moves the anchored element because it's
  // already a descendant. Note that we should not apply a double translation
  // offset to it.
  SetScrollOffsetDelta(GetImpl(scroller1.get()), gfx::Vector2dF(15, 20));
  UpdateActiveTreeDrawProperties();
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(-20, -20),
      GetImpl(anchored.get())->ScreenSpaceTransform().To2dTranslation());
}
class AnimationScaleFactorTrackingLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<AnimationScaleFactorTrackingLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id) {
    return base::WrapUnique(
        new AnimationScaleFactorTrackingLayerImpl(tree_impl, id));
  }

  ~AnimationScaleFactorTrackingLayerImpl() override = default;

 private:
  explicit AnimationScaleFactorTrackingLayerImpl(LayerTreeImpl* tree_impl,
                                                 int id)
      : LayerImpl(tree_impl, id) {
    SetDrawsContent(true);
  }
};

TEST_F(DrawPropertiesTest, MaximumAnimationScaleFactor) {
  LayerImpl* root = root_layer();
  auto* grand_parent =
      AddLayerInActiveTree<AnimationScaleFactorTrackingLayerImpl>();
  auto* parent = AddLayerInActiveTree<AnimationScaleFactorTrackingLayerImpl>();
  auto* child = AddLayerInActiveTree<AnimationScaleFactorTrackingLayerImpl>();
  auto* grand_child =
      AddLayerInActiveTree<AnimationScaleFactorTrackingLayerImpl>();
  SetElementIdsForTesting();

  root->SetBounds(gfx::Size(1, 2));
  grand_parent->SetBounds(gfx::Size(1, 2));
  parent->SetBounds(gfx::Size(1, 2));
  child->SetBounds(gfx::Size(1, 2));
  grand_child->SetBounds(gfx::Size(1, 2));

  CopyProperties(root, grand_parent);
  CreateTransformNode(grand_parent);
  CopyProperties(grand_parent, parent);
  CreateTransformNode(parent);
  CopyProperties(parent, child);
  CreateTransformNode(child);
  CopyProperties(child, grand_child);
  CreateTransformNode(grand_child);

  UpdateActiveTreeDrawProperties();

  // No layers have animations.
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_parent));

  gfx::TransformOperations translation;
  translation.AppendTranslate(1.f, 2.f, 3.f);

  scoped_refptr<Animation> grand_parent_animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline_impl()->AttachAnimation(grand_parent_animation);
  grand_parent_animation->AttachElement(grand_parent->element_id());

  scoped_refptr<Animation> parent_animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline_impl()->AttachAnimation(parent_animation);
  parent_animation->AttachElement(parent->element_id());

  scoped_refptr<Animation> child_animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline_impl()->AttachAnimation(child_animation);
  child_animation->AttachElement(child->element_id());

  scoped_refptr<Animation> grand_child_animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline_impl()->AttachAnimation(grand_child_animation);
  grand_child_animation->AttachElement(grand_child->element_id());

  AddAnimatedTransformToAnimation(parent_animation.get(), 1.0,
                                  gfx::TransformOperations(), translation);

  // No layers have scale-affecting animations.
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_parent));

  gfx::TransformOperations scale;
  scale.AppendScale(5.f, 4.f, 3.f);

  AddAnimatedTransformToAnimation(child_animation.get(), 1.0,
                                  gfx::TransformOperations(), scale);
  UpdateActiveTreeDrawProperties();

  // Only |child| has a scale-affecting animation.
  EXPECT_EQ(5.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(5.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_parent));

  AddAnimatedTransformToAnimation(grand_parent_animation.get(), 1.0,
                                  gfx::TransformOperations(), scale);
  UpdateActiveTreeDrawProperties();

  // |grand_parent| and |child| have scale-affecting animations.
  // With nested animated scales, the child will use the parent's maximum
  // animation scale, without combining the multiple animated scales.
  EXPECT_EQ(5.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(5.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(5.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(5.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_parent));

  AddAnimatedTransformToAnimation(parent_animation.get(), 1.0,
                                  gfx::TransformOperations(), scale);
  UpdateActiveTreeDrawProperties();

  // |grand_parent|, |parent|, and |child| have scale-affecting animations.
  // For nested scale animations, the child uses the parent's maximum scale
  // instead of combining them.
  EXPECT_EQ(5.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(5.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(5.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(5.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_parent));

  grand_parent_animation->AbortKeyframeModelsWithProperty(
      TargetProperty::TRANSFORM, false);
  parent_animation->AbortKeyframeModelsWithProperty(TargetProperty::TRANSFORM,
                                                    false);
  child_animation->AbortKeyframeModelsWithProperty(TargetProperty::TRANSFORM,
                                                   false);

  // Recreate child_animation containing keyframes with perspective.
  timeline_impl()->DetachAnimation(child_animation);
  child_animation = Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline_impl()->AttachAnimation(child_animation);
  child_animation->AttachElement(child->element_id());

  gfx::TransformOperations perspective;
  perspective.AppendPerspective(10.f);

  AddAnimatedTransformToAnimation(child_animation.get(), 1.0, perspective,
                                  perspective);
  UpdateActiveTreeDrawProperties();

  // |child| has a scale-affecting animation but computing the maximum of this
  // animation is not supported.
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_TRUE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_TRUE(AnimationAffectedByInvalidScale(child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_parent));

  child_animation->AbortKeyframeModelsWithProperty(TargetProperty::TRANSFORM,
                                                   false);

  gfx::Transform scale_matrix;
  scale_matrix.Scale(1.f, 2.f);
  SetTransform(grand_parent, scale_matrix);
  SetTransform(parent, scale_matrix);
  SetTransform(child, scale_matrix);

  AddAnimatedTransformToAnimation(parent_animation.get(), 1.0,
                                  gfx::TransformOperations(), scale);
  UpdateActiveTreeDrawProperties();

  // |grand_parent|, |parent| and |child| each has scale 2.f. |parent| has a
  // scale animation with maximum scale 5.f.
  EXPECT_EQ(20.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(20.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(10.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(2.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_parent));

  gfx::Transform perspective_matrix;
  perspective_matrix.ApplyPerspectiveDepth(2.f);
  SetTransform(child, perspective_matrix);
  UpdateActiveTreeDrawProperties();

  // |child| has a transform with perspective. Use |parent|'s maximum animation
  // scale.
  EXPECT_EQ(10.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(10.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(10.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(2.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_TRUE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_TRUE(AnimationAffectedByInvalidScale(child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_parent));

  SetTransform(parent, perspective_matrix);
  UpdateActiveTreeDrawProperties();

  // |parent| and |child| have transforms with perspective. Use |grand_parent|'s
  // maximum animation scale.
  EXPECT_EQ(2.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(2.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(2.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(2.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_TRUE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_TRUE(AnimationAffectedByInvalidScale(child));
  EXPECT_TRUE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_parent));

  SetTransform(parent, gfx::Transform());
  SetTransform(child, gfx::Transform());
  SetTransform(grand_parent, perspective_matrix);
  UpdateActiveTreeDrawProperties();

  // |grand_parent| has a transform with perspective.
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(1.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_TRUE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_TRUE(AnimationAffectedByInvalidScale(child));
  EXPECT_TRUE(AnimationAffectedByInvalidScale(parent));
  EXPECT_TRUE(AnimationAffectedByInvalidScale(grand_parent));

  gfx::Transform rotation_skew_scale;
  rotation_skew_scale.Rotate(45.f);
  rotation_skew_scale.Skew(45.f, 0.f);
  rotation_skew_scale.Scale(2.f, 1.f);
  SetTransform(grand_parent, rotation_skew_scale);
  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(10.f, MaximumAnimationToScreenScale(grand_child));
  EXPECT_EQ(10.f, MaximumAnimationToScreenScale(child));
  EXPECT_EQ(10.f, MaximumAnimationToScreenScale(parent));
  EXPECT_EQ(2.f, MaximumAnimationToScreenScale(grand_parent));

  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grand_parent));
}

static void GatherDrawnLayers(LayerTreeImpl* tree_impl,
                              std::set<LayerImpl*>* drawn_layers) {
  for (EffectTreeLayerListIterator it(tree_impl);
       it.state() != EffectTreeLayerListIterator::State::kEnd; ++it) {
    if (it.state() == EffectTreeLayerListIterator::State::kLayer) {
      drawn_layers->insert(it.current_layer());
    }

    if (it.state() !=
        EffectTreeLayerListIterator::State::kContributingSurface) {
      continue;
    }
  }
}

// Needs layer tree mode: mask layer.
TEST_F(DrawPropertiesTestWithLayerTree, RenderSurfaceLayerListMembership) {
  auto root = Layer::Create();
  auto grand_parent = Layer::Create();
  auto parent = Layer::Create();
  auto child = Layer::Create();
  auto grand_child1 = Layer::Create();
  auto grand_child2 = Layer::Create();

  root->SetBounds(gfx::Size(1, 2));
  grand_parent->SetBounds(gfx::Size(1, 2));
  parent->SetBounds(gfx::Size(1, 2));
  child->SetBounds(gfx::Size(1, 2));
  grand_child1->SetBounds(gfx::Size(1, 2));
  grand_child2->SetBounds(gfx::Size(1, 2));

  child->AddChild(grand_child1);
  child->AddChild(grand_child2);
  parent->AddChild(child);
  grand_parent->AddChild(parent);
  root->AddChild(grand_parent);
  host()->SetRootLayer(root);

  // Start with nothing being drawn.
  CommitAndActivate();

  EXPECT_FALSE(ImplOf(grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child1)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child2)->contributes_to_drawn_render_surface());

  std::set<LayerImpl*> expected;
  std::set<LayerImpl*> actual;
  GatherDrawnLayers(host_impl()->active_tree(), &actual);
  EXPECT_EQ(expected, actual);

  // If we force render surface, but none of the layers are in the layer list,
  // then this layer should not appear in RSLL.
  grand_child1->SetForceRenderSurfaceForTesting(true);

  CommitAndActivate();

  EXPECT_FALSE(ImplOf(grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child1)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child2)->contributes_to_drawn_render_surface());

  expected.clear();
  actual.clear();
  GatherDrawnLayers(host_impl()->active_tree(), &actual);
  EXPECT_EQ(expected, actual);

  // However, if we say that this layer also draws content, it will appear in
  // RSLL.
  grand_child1->SetIsDrawable(true);

  CommitAndActivate();

  EXPECT_FALSE(ImplOf(grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(grand_child1)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child2)->contributes_to_drawn_render_surface());

  expected.clear();
  expected.insert(ImplOf(grand_child1));

  actual.clear();
  GatherDrawnLayers(host_impl()->active_tree(), &actual);
  EXPECT_EQ(expected, actual);

  // Now child is forced to have a render surface, and one if its children draws
  // content.
  grand_child1->SetIsDrawable(false);
  grand_child1->SetForceRenderSurfaceForTesting(false);
  child->SetForceRenderSurfaceForTesting(true);
  grand_child2->SetIsDrawable(true);

  CommitAndActivate();

  EXPECT_FALSE(ImplOf(grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child1)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(grand_child2)->contributes_to_drawn_render_surface());

  expected.clear();
  expected.insert(ImplOf(grand_child2));

  actual.clear();
  GatherDrawnLayers(host_impl()->active_tree(), &actual);
  EXPECT_EQ(expected, actual);

  // Add a mask layer to child.
  FakeContentLayerClient client;
  auto mask = PictureLayer::Create(&client);
  mask->SetBounds(child->bounds());
  child->SetMaskLayer(mask);

  CommitAndActivate();

  EXPECT_FALSE(ImplOf(grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(mask)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child1)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(grand_child2)->contributes_to_drawn_render_surface());

  expected.clear();
  expected.insert(ImplOf(grand_child2));
  expected.insert(ImplOf(mask));

  actual.clear();
  GatherDrawnLayers(host_impl()->active_tree(), &actual);
  EXPECT_EQ(expected, actual);

  CommitAndActivate();

  EXPECT_FALSE(ImplOf(grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(mask)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child1)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(grand_child2)->contributes_to_drawn_render_surface());

  expected.clear();
  expected.insert(ImplOf(grand_child2));
  expected.insert(ImplOf(mask));

  actual.clear();
  GatherDrawnLayers(host_impl()->active_tree(), &actual);
  EXPECT_EQ(expected, actual);

  // With nothing drawing, we should have no layers.
  grand_child2->SetIsDrawable(false);

  CommitAndActivate();

  EXPECT_FALSE(ImplOf(grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(child)->contributes_to_drawn_render_surface());
  // Mask layer has its own render surface in layer tree mode.
  EXPECT_TRUE(ImplOf(mask)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child1)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child2)->contributes_to_drawn_render_surface());

  expected.clear();
  expected.insert(ImplOf(mask));
  actual.clear();
  GatherDrawnLayers(host_impl()->active_tree(), &actual);
  EXPECT_EQ(expected, actual);

  // When the child is drawable, both the child and the mask should be in the
  // render surface list.
  child->SetIsDrawable(true);

  CommitAndActivate();

  EXPECT_FALSE(ImplOf(grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(parent)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(mask)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child1)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(grand_child2)->contributes_to_drawn_render_surface());

  expected.clear();
  expected.insert(ImplOf(child));
  expected.insert(ImplOf(mask));
  actual.clear();
  GatherDrawnLayers(host_impl()->active_tree(), &actual);
  EXPECT_EQ(expected, actual);

  child->SetMaskLayer(nullptr);

  // Now everyone's a member!
  grand_parent->SetIsDrawable(true);
  parent->SetIsDrawable(true);
  child->SetIsDrawable(true);
  grand_child1->SetIsDrawable(true);
  grand_child2->SetIsDrawable(true);

  CommitAndActivate();

  EXPECT_TRUE(ImplOf(grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(parent)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(grand_child1)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(grand_child2)->contributes_to_drawn_render_surface());

  expected.clear();
  expected.insert(ImplOf(grand_parent));
  expected.insert(ImplOf(parent));
  expected.insert(ImplOf(child));
  expected.insert(ImplOf(grand_child1));
  expected.insert(ImplOf(grand_child2));

  actual.clear();
  GatherDrawnLayers(host_impl()->active_tree(), &actual);
  EXPECT_EQ(expected, actual);
}

// Needs layer tree mode: mask layer.
TEST_F(DrawPropertiesTestWithLayerTree, DrawPropertyDeviceScale) {
  auto root = Layer::Create();
  auto child1 = Layer::Create();
  auto child2 = Layer::Create();

  gfx::Transform scale_transform_child1, scale_transform_child2;
  scale_transform_child1.Scale(2, 3);
  scale_transform_child2.Scale(4, 5);

  root->SetBounds(gfx::Size(1, 1));
  root->SetIsDrawable(true);
  child1->SetTransform(scale_transform_child1);
  child1->SetBounds(gfx::Size(1, 1));
  child1->SetIsDrawable(true);

  FakeContentLayerClient client;
  auto mask = PictureLayer::Create(&client);
  mask->SetBounds(child1->bounds());
  child1->SetMaskLayer(mask);

  root->AddChild(child1);
  root->AddChild(child2);
  host()->SetRootLayer(root);
  host()->SetElementIdsForTesting();

  gfx::TransformOperations scale;
  scale.AppendScale(5.f, 8.f, 3.f);

  child2->SetTransform(scale_transform_child2);
  child2->SetBounds(gfx::Size(1, 1));
  child2->SetIsDrawable(true);
  AddAnimatedTransformToElementWithAnimation(
      child2->element_id(), timeline(), 1.0, gfx::TransformOperations(), scale);

  CommitAndActivate();

  EXPECT_EQ(gfx::Vector2dF(1.f, 1.f), ImplOf(root)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(2.f, 3.f), ImplOf(child1)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(2.f, 3.f), ImplOf(mask)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(4.f, 5.f), ImplOf(child2)->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(8.f, MaximumAnimationToScreenScale(ImplOf(child2)));
  EXPECT_FLOAT_EQ(3.f, MaximumAnimationToScreenScale(ImplOf(child1)));
  EXPECT_FLOAT_EQ(1.f, MaximumAnimationToScreenScale(ImplOf(root)));

  // Changing device-scale would affect ideal_contents_scale and
  // maximum_animation_contents_scale.

  float device_scale_factor = 4.0f;
  CommitAndActivate(device_scale_factor);

  EXPECT_EQ(gfx::Vector2dF(4.f, 4.f), ImplOf(root)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(8.f, 12.f), ImplOf(child1)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(8.f, 12.f), ImplOf(mask)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(16.f, 20.f),
            ImplOf(child2)->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(32.f, MaximumAnimationToScreenScale(ImplOf(child2)));
  EXPECT_FLOAT_EQ(12.f, MaximumAnimationToScreenScale(ImplOf(child1)));
  EXPECT_FLOAT_EQ(4.f, MaximumAnimationToScreenScale(ImplOf(root)));
}

TEST_F(DrawPropertiesTest, DrawPropertyScales) {
  auto root = Layer::Create();
  auto page_scale = Layer::Create();
  auto child1 = Layer::Create();
  auto child2 = Layer::Create();

  gfx::Transform scale_transform_child1, scale_transform_child2;
  scale_transform_child1.Scale(2, 3);
  scale_transform_child2.Scale(4, 5);

  root->SetBounds(gfx::Size(1, 1));
  root->SetIsDrawable(true);
  child1->SetBounds(gfx::Size(1, 1));
  child1->SetIsDrawable(true);
  child2->SetBounds(gfx::Size(1, 1));
  child2->SetIsDrawable(true);

  root->AddChild(child1);
  root->AddChild(child2);
  root->AddChild(page_scale);
  host()->SetRootLayer(root);
  host()->SetElementIdsForTesting();

  SetupRootProperties(root.get());
  CopyProperties(root.get(), page_scale.get());
  CreateTransformNode(page_scale.get());
  CopyProperties(page_scale.get(), child1.get());
  CreateTransformNode(child1.get()).local = scale_transform_child1;
  CopyProperties(page_scale.get(), child2.get());
  CreateTransformNode(child2.get()).local = scale_transform_child2;

  ViewportPropertyIds viewport_property_ids;
  viewport_property_ids.page_scale_transform =
      page_scale->transform_tree_index();
  host()->RegisterViewportPropertyIds(viewport_property_ids);

  gfx::TransformOperations scale;
  scale.AppendScale(5.f, 8.f, 3.f);

  AddAnimatedTransformToElementWithAnimation(
      child2->element_id(), timeline(), 1.0, gfx::TransformOperations(), scale);

  CommitAndActivate();

  EXPECT_EQ(gfx::Vector2dF(1.f, 1.f), ImplOf(root)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(1.f, 1.f),
            ImplOf(page_scale)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(2.f, 3.f), ImplOf(child1)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(4.f, 5.f), ImplOf(child2)->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(8.f, MaximumAnimationToScreenScale(ImplOf(child2)));
  EXPECT_FLOAT_EQ(3.f, MaximumAnimationToScreenScale(ImplOf(child1)));
  EXPECT_FLOAT_EQ(1.f, MaximumAnimationToScreenScale(ImplOf(page_scale)));
  EXPECT_FLOAT_EQ(1.f, MaximumAnimationToScreenScale(ImplOf(root)));

  // Changing page-scale would affect ideal_contents_scale and
  // maximum_animation_contents_scale.

  float device_scale_factor = 1.0f;
  host()->SetPageScaleFactorAndLimits(3.f, 3.f, 3.f);
  CommitAndActivate();

  EXPECT_EQ(gfx::Vector2dF(1.f, 1.f), ImplOf(root)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(3.f, 3.f),
            ImplOf(page_scale)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(6.f, 9.f), ImplOf(child1)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(12.f, 15.f),
            ImplOf(child2)->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(24.f, MaximumAnimationToScreenScale(ImplOf(child2)));
  EXPECT_FLOAT_EQ(9.f, MaximumAnimationToScreenScale(ImplOf(child1)));
  EXPECT_FLOAT_EQ(3.f, MaximumAnimationToScreenScale(ImplOf(page_scale)));
  EXPECT_FLOAT_EQ(1.f, MaximumAnimationToScreenScale(ImplOf(root)));

  // Changing device-scale would affect ideal_contents_scale and
  // maximum_animation_contents_scale.

  device_scale_factor = 4.0f;
  CommitAndActivate(device_scale_factor);

  EXPECT_EQ(gfx::Vector2dF(4.f, 4.f), ImplOf(root)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(12.f, 12.f),
            ImplOf(page_scale)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(24.f, 36.f),
            ImplOf(child1)->GetIdealContentsScale());
  EXPECT_EQ(gfx::Vector2dF(48.f, 60.f),
            ImplOf(child2)->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(96.f, MaximumAnimationToScreenScale(ImplOf(child2)));
  EXPECT_FLOAT_EQ(36.f, MaximumAnimationToScreenScale(ImplOf(child1)));
  EXPECT_FLOAT_EQ(12.f, MaximumAnimationToScreenScale(ImplOf(page_scale)));
  EXPECT_FLOAT_EQ(4.f, MaximumAnimationToScreenScale(ImplOf(root)));
}

TEST_F(DrawPropertiesTest, AnimationScales) {
  LayerImpl* root = root_layer();
  auto* child1 = AddLayerInActiveTree<LayerImpl>();
  auto* child2 = AddLayerInActiveTree<LayerImpl>();
  SetElementIdsForTesting();

  gfx::Transform scale_transform_child1, scale_transform_child2;
  scale_transform_child1.Scale(2, 3);
  scale_transform_child2.Scale(4, 5);

  root->SetBounds(gfx::Size(1, 1));
  child1->SetBounds(gfx::Size(1, 1));
  child2->SetBounds(gfx::Size(1, 1));

  CopyProperties(root, child1);
  CreateTransformNode(child1).local = scale_transform_child1;
  CopyProperties(child1, child2);
  CreateTransformNode(child2).local = scale_transform_child2;

  gfx::TransformOperations scale;
  scale.AppendScale(5.f, 8.f, 3.f);

  AddAnimatedTransformToElementWithAnimation(child2->element_id(),
                                             timeline_impl(), 1.0,
                                             gfx::TransformOperations(), scale);
  UpdateActiveTreeDrawProperties();

  EXPECT_FLOAT_EQ(24.f, MaximumAnimationToScreenScale(child2));
  EXPECT_FLOAT_EQ(3.f, MaximumAnimationToScreenScale(child1));
  EXPECT_FLOAT_EQ(1.f, MaximumAnimationToScreenScale(root));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(child2));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(child1));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(root));

  // Correctly updates animation scale when layer property changes.
  SetTransform(child1, gfx::Transform());
  root->layer_tree_impl()->SetTransformMutated(child1->element_id(),
                                               gfx::Transform());
  UpdateActiveTreeDrawProperties();
  EXPECT_FLOAT_EQ(8.f, MaximumAnimationToScreenScale(child2));

  // Do not update animation scale if already updated.
  bool affected_by_invalid_scale = true;
  host_impl()
      ->active_tree()
      ->property_trees()
      ->SetMaximumAnimationToScreenScaleForTesting(
          child2->transform_tree_index(), 100.f, affected_by_invalid_scale);
  EXPECT_FLOAT_EQ(100.f, MaximumAnimationToScreenScale(child2));
  EXPECT_TRUE(AnimationAffectedByInvalidScale(child2));
}

TEST_F(DrawPropertiesTest, AnimationScaleFromSmallToOne) {
  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(1, 1));
  auto* parent = AddLayerInActiveTree<LayerImpl>();
  parent->SetBounds(gfx::Size(1, 1));
  auto* child = AddLayerInActiveTree<LayerImpl>();
  child->SetBounds(gfx::Size(1, 1));
  auto* grandchild = AddLayerInActiveTree<LayerImpl>();
  grandchild->SetBounds(gfx::Size(1, 1));
  SetElementIdsForTesting();

  gfx::Transform parent_scale;
  parent_scale.Scale(1, 2);
  gfx::Transform small_scale;
  small_scale.Scale(0.1, 0.2);

  CopyProperties(root, parent);
  CreateTransformNode(parent).local = parent_scale;
  CopyProperties(parent, child);
  CreateTransformNode(child).local = small_scale;
  CopyProperties(child, grandchild);
  CreateTransformNode(grandchild).local = small_scale;

  gfx::TransformOperations small_scale_operations;
  small_scale_operations.AppendMatrix(small_scale);
  gfx::TransformOperations scale_one_operations;

  // Both child and grandchild animate scale from 0.1x0.2 to 1.
  AddAnimatedTransformToElementWithAnimation(
      child->element_id(), timeline_impl(), 1.0, small_scale_operations,
      scale_one_operations);
  AddAnimatedTransformToElementWithAnimation(
      grandchild->element_id(), timeline_impl(), 1.0, small_scale_operations,
      scale_one_operations);
  UpdateActiveTreeDrawProperties();

  EXPECT_FLOAT_EQ(2.f, MaximumAnimationToScreenScale(grandchild));
  EXPECT_FLOAT_EQ(2.f, MaximumAnimationToScreenScale(child));
  EXPECT_FLOAT_EQ(2.f, MaximumAnimationToScreenScale(parent));
  EXPECT_FLOAT_EQ(1.f, MaximumAnimationToScreenScale(root));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(grandchild));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(child));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(parent));
  EXPECT_FALSE(AnimationAffectedByInvalidScale(root));
}

TEST_F(DrawPropertiesTest, VisibleContentRectInChildRenderSurface) {
  LayerImpl* root = root_layer();
  LayerImpl* clip = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* content = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(768 / 2, 3000));
  root->SetDrawsContent(true);
  clip->SetBounds(gfx::Size(768 / 2, 10000));
  content->SetBounds(gfx::Size(768 / 2, 10000));
  content->SetDrawsContent(true);

  CopyProperties(root, clip);
  CreateClipNode(clip);
  CopyProperties(clip, content);
  CreateEffectNode(content).render_surface_reason = RenderSurfaceReason::kTest;

  // Not calling UpdateActiveTreeDrawProperties() because we want to set a
  // special device viewport rect.
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(768, 582));
  float device_scale_factor = 2.f;
  host_impl()->active_tree()->SetDeviceScaleFactor(device_scale_factor);
  UpdateDrawProperties(host_impl()->active_tree());

  // Layers in the root render surface have their visible content rect clipped
  // by the viewport.
  EXPECT_EQ(gfx::Rect(768 / device_scale_factor, 582 / device_scale_factor),
            root->visible_layer_rect());

  // Layers drawing to a child render surface should still have their visible
  // content rect clipped by the viewport.
  EXPECT_EQ(gfx::Rect(768 / device_scale_factor, 582 / device_scale_factor),
            content->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, ViewportBoundsDeltaAffectVisibleContentRect) {
  gfx::Size container_size = gfx::Size(300, 500);
  gfx::Size scroll_size = gfx::Size(300, 1000);

  // Device viewport accomidated the root and the browser controls.
  gfx::Rect device_viewport_rect = gfx::Rect(300, 600);

  LayerTreeImpl* active_tree = host_impl()->active_tree();
  active_tree->SetDeviceViewportRect(device_viewport_rect);
  active_tree->SetBrowserControlsParams({50, 0, 0, 0, false, true});
  active_tree->PushPageScaleFromMainThread(1.0f, 1.0f, 1.0f);

  LayerImpl* root = root_layer();
  root->SetBounds(device_viewport_rect.size());
  SetupViewport(root, container_size, scroll_size);

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  scroll_layer->SetDrawsContent(true);

  active_tree->SetCurrentBrowserControlsShownRatio(1.0f, 1.0f);
  active_tree->UpdateViewportContainerSizes();
  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(container_size), scroll_layer->visible_layer_rect());

  active_tree->SetCurrentBrowserControlsShownRatio(0.0f, 0.0f);
  active_tree->UpdateViewportContainerSizes();
  UpdateActiveTreeDrawProperties();

  gfx::Rect affected_by_delta(container_size.width(),
                              container_size.height() + 50);
  EXPECT_EQ(affected_by_delta, scroll_layer->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, VisibleContentRectForAnimatedLayer) {
  host_impl()->CreatePendingTree();
  LayerImpl* root = EnsureRootLayerInPendingTree();
  LayerImpl* animated = AddLayerInPendingTree<LayerImpl>();

  animated->SetDrawsContent(true);
  host_impl()->pending_tree()->SetElementIdsForTesting();

  root->SetBounds(gfx::Size(100, 100));
  animated->SetBounds(gfx::Size(20, 20));

  CopyProperties(root, animated);
  CreateEffectNode(animated).opacity = 0.f;

  AddOpacityTransitionToElementWithAnimation(
      animated->element_id(), timeline_impl(), 10.0, 0.f, 1.f, false);
  UpdatePendingTreeDrawProperties();

  EXPECT_FALSE(animated->visible_layer_rect().IsEmpty());
}

TEST_F(DrawPropertiesTest,
       VisibleContentRectForAnimatedLayerWithSingularTransform) {
  host_impl()->CreatePendingTree();
  LayerImpl* root = EnsureRootLayerInPendingTree();
  LayerImpl* clip = AddLayerInPendingTree<LayerImpl>();
  LayerImpl* animated = AddLayerInPendingTree<LayerImpl>();
  LayerImpl* surface = AddLayerInPendingTree<LayerImpl>();
  LayerImpl* descendant_of_keyframe_model = AddLayerInPendingTree<LayerImpl>();
  host_impl()->pending_tree()->SetElementIdsForTesting();

  root->SetDrawsContent(true);
  animated->SetDrawsContent(true);
  surface->SetDrawsContent(true);
  descendant_of_keyframe_model->SetDrawsContent(true);

  gfx::Transform uninvertible_matrix;
  uninvertible_matrix.Scale3d(6.f, 6.f, 0.f);

  root->SetBounds(gfx::Size(100, 100));
  clip->SetBounds(gfx::Size(10, 10));
  animated->SetBounds(gfx::Size(120, 120));
  surface->SetBounds(gfx::Size(100, 100));
  descendant_of_keyframe_model->SetBounds(gfx::Size(200, 200));

  CopyProperties(root, clip);
  CreateClipNode(clip);
  CopyProperties(clip, animated);
  CreateTransformNode(animated).local = uninvertible_matrix;
  CopyProperties(animated, surface);
  CreateTransformNode(surface);
  CreateEffectNode(surface).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(surface, descendant_of_keyframe_model);

  gfx::TransformOperations start_transform_operations;
  start_transform_operations.AppendMatrix(uninvertible_matrix);
  gfx::TransformOperations end_transform_operations;

  AddAnimatedTransformToElementWithAnimation(
      animated->element_id(), timeline_impl(), 10.0, start_transform_operations,
      end_transform_operations);
  UpdatePendingTreeDrawProperties();
  // Since animated has singular transform, it is not be part of render
  // surface layer list but should be rastered.
  EXPECT_FALSE(animated->contributes_to_drawn_render_surface());
  EXPECT_TRUE(animated->raster_even_if_not_drawn());

  // The animated layer has a singular transform and maps to a non-empty rect in
  // clipped target space, so is treated as fully visible.
  EXPECT_EQ(gfx::Rect(120, 120), animated->visible_layer_rect());

  // The singular transform on |animated| is flattened when inherited by
  // |surface|, and this happens to make it invertible.
  EXPECT_EQ(gfx::Rect(2, 2), surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(2, 2),
            descendant_of_keyframe_model->visible_layer_rect());

  gfx::Transform zero_matrix;
  zero_matrix.Scale3d(0.f, 0.f, 0.f);
  root->layer_tree_impl()->SetTransformMutated(animated->element_id(),
                                               zero_matrix);
  UpdatePendingTreeDrawProperties();

  // The animated layer will be treated as fully visible when we combine clips
  // in screen space.
  EXPECT_EQ(gfx::Rect(120, 120), animated->visible_layer_rect());

  // This time, flattening does not make |animated|'s transform invertible. This
  // means the clip cannot be projected into |surface|'s space, so we treat
  // |surface| and layers that draw into it as having empty visible rect.
  EXPECT_EQ(gfx::Rect(100, 100), surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(200, 200),
            descendant_of_keyframe_model->visible_layer_rect());

  host_impl()->ActivateSyncTree();
  LayerImpl* active_root = host_impl()->active_tree()->LayerById(root->id());
  UpdateActiveTreeDrawProperties();

  // Since the animated has singular transform, it is not be part of render
  // surface layer list.
  LayerImpl* active_animated =
      host_impl()->active_tree()->LayerById(animated->id());
  EXPECT_TRUE(active_root->contributes_to_drawn_render_surface());
  EXPECT_FALSE(active_animated->contributes_to_drawn_render_surface());

  // Since animated has singular transform, it is not be part of render
  // surface layer list but should be rastered.
  EXPECT_TRUE(animated->raster_even_if_not_drawn());
  EXPECT_EQ(gfx::Rect(120, 120), active_animated->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, ChangeTransformOrigin) {
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform scale_matrix;
  scale_matrix.Scale(2.f, 2.f);

  root->SetBounds(gfx::Size(100, 100));
  root->SetDrawsContent(true);
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  CopyProperties(root, child);
  CreateTransformNode(child).local = scale_matrix;

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(10, 10), child->visible_layer_rect());

  SetTransformOrigin(child, gfx::Point3F(10.f, 10.f, 10.f));

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(5, 5, 5, 5), child->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, UpdateScrollChildPosition) {
  LayerImpl* root = root_layer();
  LayerImpl* scroll_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* scroll_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(50, 50));

  scroll_parent->SetBounds(gfx::Size(30, 30));
  scroll_parent->SetElementId(
      LayerIdToElementIdForTesting(scroll_parent->id()));
  scroll_parent->SetDrawsContent(true);

  scroll_child->SetBounds(gfx::Size(40, 40));
  scroll_child->SetDrawsContent(true);

  CopyProperties(root, scroll_parent);
  CreateTransformNode(scroll_parent);
  CreateScrollNode(scroll_parent, gfx::Size(50, 50));
  CopyProperties(scroll_parent, scroll_child);
  CreateTransformNode(scroll_child).local.Scale(2.f, 2.f);

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(25, 25), scroll_child->visible_layer_rect());

  SetScrollOffset(scroll_parent, gfx::PointF(0.f, 10.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(0, 5, 25, 25), scroll_child->visible_layer_rect());

  SetPostTranslation(scroll_child, gfx::Vector2dF(0, -10.f));
  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(0, 10, 25, 25), scroll_child->visible_layer_rect());
}

// Needs layer tree mode: copy request. Not using impl-side PropertyTreeBuilder.
TEST_F(DrawPropertiesTestWithLayerTree, HasCopyRequestsInTargetSubtree) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();
  scoped_refptr<Layer> grandchild = Layer::Create();
  scoped_refptr<Layer> greatgrandchild = Layer::Create();

  root->AddChild(child1);
  root->AddChild(child2);
  child1->AddChild(grandchild);
  grandchild->AddChild(greatgrandchild);
  host()->SetRootLayer(root);

  root->SetBounds(gfx::Size(1, 1));
  child1->RequestCopyOfOutput(viz::CopyOutputRequest::CreateStubForTesting());
  greatgrandchild->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());
  child2->SetOpacity(0.f);
  UpdateMainDrawProperties();

  EXPECT_TRUE(LayerSubtreeHasCopyRequest(root.get()));
  EXPECT_TRUE(LayerSubtreeHasCopyRequest(child1.get()));
  EXPECT_FALSE(LayerSubtreeHasCopyRequest(child2.get()));
  EXPECT_TRUE(LayerSubtreeHasCopyRequest(grandchild.get()));
  EXPECT_TRUE(LayerSubtreeHasCopyRequest(greatgrandchild.get()));
}

// Needs layer tree mode: hide_layer_and_subtree, etc.
// Not using impl-side PropertyTreeBuilder.
TEST_F(DrawPropertiesTestWithLayerTree, SkippingSubtreeMain) {
  FakeContentLayerClient client;

  scoped_refptr<Layer> root = Layer::Create();
  client.set_bounds(root->bounds());
  scoped_refptr<Layer> child = Layer::Create();
  child->SetIsDrawable(true);
  scoped_refptr<Layer> grandchild = Layer::Create();
  grandchild->SetIsDrawable(true);
  scoped_refptr<FakePictureLayer> greatgrandchild(
      FakePictureLayer::Create(&client));

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  grandchild->SetBounds(gfx::Size(10, 10));
  greatgrandchild->SetBounds(gfx::Size(10, 10));

  root->AddChild(child);
  child->AddChild(grandchild);
  grandchild->AddChild(greatgrandchild);
  host()->SetRootLayer(root);
  host()->SetElementIdsForTesting();

  // Check the non-skipped case.
  UpdateMainDrawProperties();
  EXPECT_TRUE(UpdateLayerListContains(grandchild->id()));

  // Now we will reset the visible rect from property trees for the grandchild,
  // and we will configure |child| in several ways that should force the subtree
  // to be skipped. The visible content rect for |grandchild| should, therefore,
  // remain empty.
  gfx::Transform singular;
  singular.set_rc(0, 0, 0);

  child->SetTransform(singular);
  UpdateMainDrawProperties();
  EXPECT_FALSE(UpdateLayerListContains(grandchild->id()));
  child->SetTransform(gfx::Transform());

  child->SetHideLayerAndSubtree(true);
  UpdateMainDrawProperties();
  EXPECT_FALSE(UpdateLayerListContains(grandchild->id()));
  child->SetHideLayerAndSubtree(false);

  gfx::Transform zero_z_scale;
  zero_z_scale.Scale3d(1, 1, 0);
  child->SetTransform(zero_z_scale);

  // Add a transform animation with a start delay. Now, even though |child| has
  // a singular transform, the subtree should still get processed.
  int keyframe_model_id = 0;
  std::unique_ptr<KeyframeModel> keyframe_model = KeyframeModel::Create(
      std::unique_ptr<gfx::AnimationCurve>(new FakeTransformTransition(1.0)),
      keyframe_model_id, 1,
      KeyframeModel::TargetPropertyId(TargetProperty::TRANSFORM));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  keyframe_model->set_time_offset(base::Milliseconds(-1000));
  AddKeyframeModelToElementWithAnimation(child->element_id(), timeline(),
                                         std::move(keyframe_model));
  UpdateMainDrawProperties();
  EXPECT_TRUE(UpdateLayerListContains(grandchild->id()));

  RemoveKeyframeModelFromElementWithExistingKeyframeEffect(
      child->element_id(), timeline(), keyframe_model_id);
  child->SetTransform(gfx::Transform());
  child->SetOpacity(0.f);
  UpdateMainDrawProperties();
  EXPECT_FALSE(UpdateLayerListContains(grandchild->id()));

  // Now, even though child has zero opacity, we will configure |grandchild| and
  // |greatgrandchild| in several ways that should force the subtree to be
  // processed anyhow.
  grandchild->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());
  UpdateMainDrawProperties();
  EXPECT_TRUE(UpdateLayerListContains(grandchild->id()));

  // Add an opacity animation with a start delay.
  keyframe_model_id = 1;
  keyframe_model = KeyframeModel::Create(
      std::unique_ptr<gfx::AnimationCurve>(
          new FakeFloatTransition(1.0, 0.f, 1.f)),
      keyframe_model_id, 1,
      KeyframeModel::TargetPropertyId(TargetProperty::OPACITY));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  keyframe_model->set_time_offset(base::Milliseconds(-1000));
  AddKeyframeModelToElementWithExistingKeyframeEffect(
      child->element_id(), timeline(), std::move(keyframe_model));
  UpdateMainDrawProperties();
  EXPECT_TRUE(UpdateLayerListContains(grandchild->id()));
}

// Needs layer tree mode: hide_layer_and_subtree, etc.
TEST_F(DrawPropertiesTestWithLayerTree, SkippingLayerImpl) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto parent = Layer::Create();
  root->AddChild(parent);
  auto child = Layer::Create();
  parent->AddChild(child);
  auto grandchild = Layer::Create();
  child->AddChild(grandchild);
  FakeContentLayerClient client;
  auto greatgrandchild = PictureLayer::Create(&client);
  grandchild->AddChild(greatgrandchild);

  root->SetBounds(gfx::Size(100, 100));
  parent->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  child->SetIsDrawable(true);
  grandchild->SetBounds(gfx::Size(10, 10));
  grandchild->SetIsDrawable(true);
  greatgrandchild->SetIsDrawable(true);

  host()->SetElementIdsForTesting();

  // Check the non-skipped case.
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(grandchild)->visible_layer_rect());

  // Now we will reset the visible rect from property trees for the grandchild,
  // and we will configure |child| in several ways that should force the subtree
  // to be skipped. The visible content rect for |grandchild| should, therefore,
  // remain empty.
  ImplOf(grandchild)->set_visible_layer_rect(gfx::Rect());

  gfx::Transform singular;
  singular.set_rc(0, 0, 0);
  // This line is used to make the results of skipping and not skipping layers
  // different.
  singular.set_rc(0, 1, 1);

  gfx::Transform rotate;
  rotate.Rotate(90);

  gfx::Transform rotate_back_and_translate;
  rotate_back_and_translate.RotateAboutYAxis(180);
  rotate_back_and_translate.Translate(-10, 0);

  child->SetTransform(singular);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(0, 0), ImplOf(grandchild)->visible_layer_rect());
  child->SetTransform(gfx::Transform());

  child->SetHideLayerAndSubtree(true);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(0, 0), ImplOf(grandchild)->visible_layer_rect());
  child->SetHideLayerAndSubtree(false);

  child->SetOpacity(0.f);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(0, 0), ImplOf(grandchild)->visible_layer_rect());
  child->SetOpacity(1.f);

  parent->SetTransform(singular);
  // Force transform tree to have a node for child, so that ancestor's
  // invertible transform can be tested.
  child->SetTransform(rotate);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(0, 0), ImplOf(grandchild)->visible_layer_rect());
  parent->SetTransform(gfx::Transform());
  child->SetTransform(gfx::Transform());

  parent->SetOpacity(0.f);
  child->SetOpacity(0.7f);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(0, 0), ImplOf(grandchild)->visible_layer_rect());
  parent->SetOpacity(1.f);

  child->SetOpacity(0.f);
  // Now, even though child has zero opacity, we will configure |grandchild| and
  // |greatgrandchild| in several ways that should force the subtree to be
  // processed anyhow.
  grandchild->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(grandchild)->visible_layer_rect());

  GetPropertyTrees(root.get())->effect_tree_mutable().ClearCopyRequests();
  child->SetOpacity(1.f);

  // A double sided render surface with backface visible should not be skipped
  ImplOf(grandchild)->set_visible_layer_rect(gfx::Rect());
  child->SetForceRenderSurfaceForTesting(true);
  child->SetTransform(rotate_back_and_translate);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(grandchild)->visible_layer_rect());
  child->SetTransform(gfx::Transform());

  std::unique_ptr<gfx::KeyframedTransformAnimationCurve> curve(
      gfx::KeyframedTransformAnimationCurve::Create());
  gfx::TransformOperations start;
  start.AppendTranslate(1.f, 2.f, 3.f);
  gfx::Transform transform;
  transform.Scale3d(1.0, 2.0, 3.0);
  gfx::TransformOperations operation;
  operation.AppendMatrix(transform);
  curve->AddKeyframe(
      gfx::TransformKeyframe::Create(base::TimeDelta(), start, nullptr));
  curve->AddKeyframe(
      gfx::TransformKeyframe::Create(base::Seconds(1.0), operation, nullptr));
  std::unique_ptr<KeyframeModel> transform_animation(KeyframeModel::Create(
      std::move(curve), 3, 3,
      KeyframeModel::TargetPropertyId(TargetProperty::TRANSFORM)));
  scoped_refptr<Animation> animation(Animation::Create(1));
  timeline()->AttachAnimation(animation);
  animation->AttachElement(parent->element_id());
  animation->AddKeyframeModel(std::move(transform_animation));
  ImplOf(grandchild)->set_visible_layer_rect(gfx::Rect());
  parent->SetTransform(singular);
  child->SetTransform(singular);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(0, 0), ImplOf(grandchild)->visible_layer_rect());
}

// This tests for correctness of an optimization.  If a node in the tree
// maps all possible spaces to a single point (ie has a singular transform)
// we can ignore the size of all its children.  We need to make sure that
// we don't do this if an animation can replace this transform in the
// compositor without recomputing the trees.
TEST_F(DrawPropertiesTest, LayerSkippingInSubtreeOfSingularTransform) {
  // Set up a transform animation
  std::unique_ptr<gfx::KeyframedTransformAnimationCurve> curve(
      gfx::KeyframedTransformAnimationCurve::Create());
  gfx::TransformOperations start;
  start.AppendTranslate(1.f, 2.f, 3.f);
  gfx::Transform transform;
  transform.Scale3d(1.0, 2.0, 3.0);
  gfx::TransformOperations operation;
  operation.AppendMatrix(transform);
  curve->AddKeyframe(
      gfx::TransformKeyframe::Create(base::TimeDelta(), start, nullptr));
  curve->AddKeyframe(
      gfx::TransformKeyframe::Create(base::Seconds(1.0), operation, nullptr));
  std::unique_ptr<KeyframeModel> transform_animation(KeyframeModel::Create(
      std::move(curve), 3, 3,
      KeyframeModel::TargetPropertyId(TargetProperty::TRANSFORM)));
  transform_animation->set_affects_pending_elements(false);
  scoped_refptr<Animation> animation(Animation::Create(1));
  timeline_impl()->AttachAnimation(animation);
  animation->AddKeyframeModel(std::move(transform_animation));

  // Set up some layers to have a tree.
  LayerImpl* root = root_layer();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();

  SetElementIdsForTesting();

  // If these are not on the same host we are doomed to fail.
  ASSERT_EQ(timeline_impl()->animation_host(),
            child->layer_tree_impl()->mutator_host());

  // A non-invertible matrix for use later.
  gfx::Transform singular;
  singular.set_rc(0, 0, 0);
  singular.set_rc(0, 1, 1);

  root->SetBounds(gfx::Size(10, 10));
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  grand_child->SetBounds(gfx::Size(10, 10));
  grand_child->SetDrawsContent(true);

  // Check that we set the visible sizes as expected in CalculateDrawProperties
  grand_child->set_visible_layer_rect(gfx::Rect());
  child->set_visible_layer_rect(gfx::Rect());

  CopyProperties(root, child);
  CreateTransformNode(child);
  CopyProperties(child, grand_child);

  UpdateActiveTreeDrawProperties();
  ASSERT_EQ(gfx::Rect(10, 10), grand_child->visible_layer_rect());
  ASSERT_EQ(gfx::Rect(10, 10), child->visible_layer_rect());

  // See if we optimize out irrelevant pieces of work.
  SetTransform(child, singular);
  grand_child->set_visible_layer_rect(gfx::Rect());
  child->set_visible_layer_rect(gfx::Rect());
  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(), grand_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(), child->visible_layer_rect());

  // Check that undoing the transform is still valid (memoryless enough)
  SetTransform(child, gfx::Transform());
  grand_child->set_visible_layer_rect(gfx::Rect());
  child->set_visible_layer_rect(gfx::Rect());
  root->layer_tree_impl()->property_trees()->set_needs_rebuild(true);
  UpdateActiveTreeDrawProperties();
  ASSERT_EQ(gfx::Rect(10, 10), grand_child->visible_layer_rect());
  ASSERT_EQ(gfx::Rect(10, 10), child->visible_layer_rect());

  // If the transform is singular, but there is an animation on it, we
  // should not skip the subtree.  Note that the animation has not started or
  // ticked, there is also code along that path.  This is not its test.
  animation->AttachElement(child->element_id());

  SetTransform(child, singular);
  grand_child->set_visible_layer_rect(gfx::Rect(1, 1));
  child->set_visible_layer_rect(gfx::Rect(1, 1));
  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(10, 10), grand_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), child->visible_layer_rect());
}

// This tests that we skip computing the visible areas for the subtree
// rooted at nodes with constant zero opacity.
TEST_F(DrawPropertiesTestWithLayerTree, SkippingPendingLayerImpl) {
  auto root = Layer::Create();
  auto child = Layer::Create();
  auto grandchild = Layer::Create();
  FakeContentLayerClient client;
  auto greatgrandchild = PictureLayer::Create(&client);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  child->SetIsDrawable(true);
  grandchild->SetBounds(gfx::Size(10, 10));
  grandchild->SetIsDrawable(true);
  greatgrandchild->SetIsDrawable(true);

  child->AddChild(grandchild);
  root->AddChild(child);
  host()->SetRootLayer(root);
  host()->SetElementIdsForTesting();

  // Check the non-skipped case.
  root->SetOpacity(1.f);
  Commit();
  ASSERT_EQ(gfx::Rect(10, 10), PendingImplOf(grandchild)->visible_layer_rect());

  // Check the skipped case.
  root->SetOpacity(0.f);
  PendingImplOf(grandchild)->set_visible_layer_rect(gfx::Rect());
  Commit();
  EXPECT_EQ(gfx::Rect(), PendingImplOf(grandchild)->visible_layer_rect());

  // Check the animated case is not skipped.
  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> curve(
      gfx::KeyframedFloatAnimationCurve::Create());
  std::unique_ptr<gfx::TimingFunction> func =
      gfx::CubicBezierTimingFunction::CreatePreset(
          gfx::CubicBezierTimingFunction::EaseType::EASE);
  curve->AddKeyframe(
      gfx::FloatKeyframe::Create(base::TimeDelta(), 0.9f, std::move(func)));
  curve->AddKeyframe(
      gfx::FloatKeyframe::Create(base::Seconds(1.0), 0.3f, nullptr));
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), 3, 3,
      KeyframeModel::TargetPropertyId(TargetProperty::OPACITY)));
  scoped_refptr<Animation> animation(Animation::Create(1));
  timeline()->AttachAnimation(animation);
  animation->AddKeyframeModel(std::move(keyframe_model));
  animation->AttachElement(root->element_id());
  // Repeat the calculation invocation.
  PendingImplOf(grandchild)->set_visible_layer_rect(gfx::Rect());
  Commit();
  EXPECT_EQ(gfx::Rect(10, 10), PendingImplOf(grandchild)->visible_layer_rect());
}

// Needs layer tree mode: hide_layer_and_subtree.
TEST_F(DrawPropertiesTestWithLayerTree, SkippingLayer) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto child = Layer::Create();
  root->AddChild(child);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(10, 10));
  child->SetIsDrawable(true);

  CommitAndActivate();

  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(child)->visible_layer_rect());
  ImplOf(child)->set_visible_layer_rect(gfx::Rect());

  child->SetHideLayerAndSubtree(true);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(0, 0), ImplOf(child)->visible_layer_rect());
  child->SetHideLayerAndSubtree(false);

  child->SetBounds(gfx::Size());
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(0, 0), ImplOf(child)->visible_layer_rect());
  child->SetBounds(gfx::Size(10, 10));

  gfx::Transform rotate;
  rotate.RotateAboutXAxis(180.f);
  child->SetTransform(rotate);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(0, 0), ImplOf(child)->visible_layer_rect());
  child->SetTransform(gfx::Transform());

  child->SetOpacity(0.f);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(0, 0), ImplOf(child)->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, TransformOfParentClipNodeAncestorOfTarget) {
  // Ensure that when parent clip node's transform is an ancestor of current
  // clip node's target, clip is 'projected' from parent space to current
  // target space and visible rects are calculated correctly.
  LayerImpl* root = root_layer();
  LayerImpl* clip_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* target_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* test_layer = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform transform;
  transform.RotateAboutYAxis(45);

  root->SetBounds(gfx::Size(30, 30));
  clip_layer->SetBounds(gfx::Size(30, 30));
  target_layer->SetBounds(gfx::Size(30, 30));
  test_layer->SetBounds(gfx::Size(30, 30));
  test_layer->SetDrawsContent(true);

  CopyProperties(root, clip_layer);
  CreateTransformNode(clip_layer).local = transform;
  CreateClipNode(clip_layer);
  CopyProperties(clip_layer, target_layer);
  CreateTransformNode(target_layer).local = transform;
  CreateClipNode(target_layer);
  CopyProperties(target_layer, test_layer);

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::Rect(30, 30), test_layer->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, RenderSurfaceWithUnclippedDescendantsClipsSubtree) {
  // Ensure clip rect is calculated correctly when render surface has unclipped
  // descendants.
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* between_clip_parent_and_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* test_layer = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(30, 30));
  clip_parent->SetBounds(gfx::Size(30, 30));
  between_clip_parent_and_child->SetBounds(gfx::Size(30, 30));
  render_surface_layer->SetBounds(gfx::Size(30, 30));
  test_layer->SetBounds(gfx::Size(30, 30));
  test_layer->SetDrawsContent(true);

  CopyProperties(root, clip_parent);
  CreateTransformNode(clip_parent).local.Translate(2, 2);
  CreateClipNode(clip_parent);
  CopyProperties(clip_parent, between_clip_parent_and_child);
  CreateClipNode(between_clip_parent_and_child);
  CreateTransformNode(between_clip_parent_and_child).local.Translate(2, 2);
  CreateClipNode(between_clip_parent_and_child);
  CopyProperties(between_clip_parent_and_child, render_surface_layer);
  CreateEffectNode(render_surface_layer).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface_layer, test_layer);
  test_layer->SetClipTreeIndex(clip_parent->clip_tree_index());

  UpdateActiveTreeDrawProperties();

  EXPECT_TRUE(test_layer->is_clipped());
  EXPECT_EQ(gfx::Rect(-2, -2, 30, 30), test_layer->clip_rect());
  EXPECT_EQ(gfx::Rect(26, 26), test_layer->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(26, 26), test_layer->visible_drawable_content_rect());

  auto* render_surface = test_layer->render_target();
  EXPECT_TRUE(render_surface->has_contributing_layer_that_escapes_clip());
  EXPECT_EQ(between_clip_parent_and_child->clip_tree_index(),
            render_surface->ClipTreeIndex());
  if (base::FeatureList::IsEnabled(
          features::kRenderSurfaceCommonAncestorClip)) {
    EXPECT_EQ(clip_parent->clip_tree_index(),
              render_surface->common_ancestor_clip_id());
    EXPECT_TRUE(render_surface->is_clipped());
    EXPECT_EQ(gfx::Rect(2, 2, 30, 30), render_surface->clip_rect());
  } else {
    EXPECT_EQ(between_clip_parent_and_child->clip_tree_index(),
              render_surface->common_ancestor_clip_id());
    EXPECT_FALSE(render_surface->is_clipped());
  }
  EXPECT_EQ(gfx::Rect(26, 26), render_surface->content_rect());
}

TEST_F(DrawPropertiesTest,
       RenderSurfaceWithUnclippedDescendantsButDoesntApplyOwnClip) {
  // Ensure that the visible layer rect of a descendant of a surface with
  // unclipped descendants is computed correctly, when the surface doesn't apply
  // a clip.
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(30, 10));
  clip_parent->SetBounds(gfx::Size(30, 30));
  render_surface->SetBounds(gfx::Size(10, 15));
  clip_child->SetBounds(gfx::Size(10, 10));
  clip_child->SetDrawsContent(true);
  child->SetBounds(gfx::Size(40, 40));
  child->SetDrawsContent(true);

  CopyProperties(root, clip_parent);
  CopyProperties(clip_parent, render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());
  CopyProperties(clip_child, child);

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(30, 10), child->visible_layer_rect());
}

TEST_F(DrawPropertiesTest,
       RenderSurfaceClipsSubtreeAndHasUnclippedDescendants) {
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* test_layer1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* test_layer2 = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(30, 30));
  clip_parent->SetBounds(gfx::Size(30, 30));
  render_surface->SetBounds(gfx::Size(50, 50));
  render_surface->SetDrawsContent(true);
  test_layer1->SetBounds(gfx::Size(50, 50));
  test_layer1->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(50, 50));
  clip_child->SetDrawsContent(true);
  test_layer2->SetBounds(gfx::Size(50, 50));
  test_layer2->SetDrawsContent(true);

  CreateClipNode(root);
  CopyProperties(root, clip_parent);
  CopyProperties(clip_parent, render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(render_surface);
  CopyProperties(render_surface, test_layer1);
  CopyProperties(test_layer1, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());
  CopyProperties(clip_child, test_layer2);

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(30, 30), render_surface->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(30, 30), test_layer1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(30, 30), clip_child->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(30, 30), test_layer2->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, UnclippedClipParent) {
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(50, 50));
  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_parent->SetDrawsContent(true);
  render_surface->SetBounds(gfx::Size(30, 30));
  render_surface->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(50, 50));
  clip_child->SetDrawsContent(true);

  CopyProperties(root, clip_parent);
  CopyProperties(clip_parent, render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(render_surface);
  CopyProperties(render_surface, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  UpdateActiveTreeDrawProperties();

  // The clip child should inherit its clip parent's clipping state, not its
  // tree parent's clipping state.
  EXPECT_FALSE(clip_parent->is_clipped());
  EXPECT_TRUE(render_surface->is_clipped());
  EXPECT_FALSE(clip_child->is_clipped());
}

TEST_F(DrawPropertiesTest, RenderSurfaceContentRectWithMultipleSurfaces) {
  // Tests the value of render surface content rect when we have multiple types
  // of surfaces : unclipped surfaces, surfaces with unclipped surfaces and
  // clipped surfaces.
  LayerImpl* root = root_layer();
  LayerImpl* unclipped_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* unclipped_desc_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* unclipped_desc_surface2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clipped_surface = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(80, 80));

  unclipped_surface->SetBounds(gfx::Size(50, 50));
  unclipped_surface->SetDrawsContent(true);
  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_layer->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetDrawsContent(true);
  unclipped_desc_surface2->SetBounds(gfx::Size(60, 60));
  unclipped_desc_surface2->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(100, 100));
  clipped_surface->SetBounds(gfx::Size(70, 70));
  clipped_surface->SetDrawsContent(true);

  CopyProperties(root, unclipped_surface);
  CreateEffectNode(unclipped_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(unclipped_surface);
  CopyProperties(unclipped_surface, clip_parent);
  CreateClipNode(clip_parent);
  CopyProperties(clip_parent, clip_layer);
  CreateClipNode(clip_layer);
  CopyProperties(clip_layer, unclipped_desc_surface);
  CreateEffectNode(unclipped_desc_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(unclipped_desc_surface, unclipped_desc_surface2);
  CreateEffectNode(unclipped_desc_surface2).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(unclipped_desc_surface2, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());
  CopyProperties(clip_child, clipped_surface);
  CreateEffectNode(clipped_surface).render_surface_reason =
      RenderSurfaceReason::kTest;

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(50, 50),
            GetRenderSurface(unclipped_surface)->content_rect());
  EXPECT_EQ(gfx::Rect(50, 50),
            GetRenderSurface(unclipped_desc_surface)->content_rect());
  EXPECT_EQ(gfx::Rect(50, 50),
            GetRenderSurface(unclipped_desc_surface2)->content_rect());
  EXPECT_EQ(gfx::Rect(50, 50),
            GetRenderSurface(clipped_surface)->content_rect());
}

TEST_F(DrawPropertiesTest, ClipBetweenClipChildTargetAndClipParentTarget) {
  // Tests the value of render surface content rect when we have a layer that
  // clips between the clip parent's target and clip child's target.
  LayerImpl* root = root_layer();
  LayerImpl* surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* unclipped_desc_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  surface->SetBounds(gfx::Size(100, 100));
  clip_layer->SetBounds(gfx::Size(20, 20));
  clip_parent->SetBounds(gfx::Size(50, 50));
  unclipped_desc_surface->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(100, 100));
  clip_child->SetDrawsContent(true);

  CopyProperties(root, surface);
  CreateTransformNode(surface).local.RotateAboutXAxis(10);
  CreateEffectNode(surface).render_surface_reason = RenderSurfaceReason::kTest;
  CreateClipNode(surface);
  CopyProperties(surface, clip_layer);
  CreateClipNode(clip_layer);
  CopyProperties(clip_layer, clip_parent);
  CopyProperties(clip_parent, unclipped_desc_surface);
  CreateTransformNode(unclipped_desc_surface).local.Translate(10, 10);
  CreateEffectNode(unclipped_desc_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(unclipped_desc_surface, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::Rect(10, 10),
            GetRenderSurface(unclipped_desc_surface)->content_rect());
}

TEST_F(DrawPropertiesTest, VisibleRectForDescendantOfScaledSurface) {
  LayerImpl* root = root_layer();
  LayerImpl* surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* unclipped_desc_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  surface->SetBounds(gfx::Size(100, 100));
  clip_layer->SetBounds(gfx::Size(20, 20));
  clip_parent->SetBounds(gfx::Size(50, 50));
  unclipped_desc_surface->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(100, 100));
  clip_child->SetDrawsContent(true);

  CopyProperties(root, surface);
  CreateTransformNode(surface).local.Scale(2, 2);
  CreateEffectNode(surface).render_surface_reason = RenderSurfaceReason::kTest;
  CreateClipNode(surface);
  CopyProperties(surface, clip_layer);
  CreateClipNode(clip_layer);
  CopyProperties(clip_layer, clip_parent);
  CopyProperties(clip_parent, unclipped_desc_surface);
  CreateEffectNode(unclipped_desc_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(unclipped_desc_surface, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::Rect(20, 20), clip_child->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, LayerWithInputHandlerAndZeroOpacity) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* test_layer = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform translation;
  translation.Translate(10, 10);

  root->SetBounds(gfx::Size(30, 30));
  render_surface->SetBounds(gfx::Size(30, 30));
  test_layer->SetBounds(gfx::Size(20, 20));
  test_layer->SetDrawsContent(true);

  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kNone, gfx::Rect(0, 0, 20, 20));
  test_layer->SetTouchActionRegion(std::move(touch_action_region));

  CopyProperties(root, render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(render_surface);
  CopyProperties(render_surface, test_layer);
  CreateTransformNode(test_layer).local = translation;
  CreateEffectNode(test_layer).opacity = 0.f;

  UpdateActiveTreeDrawProperties();
  EXPECT_TRANSFORM_EQ(translation, test_layer->ScreenSpaceTransform());
}

TEST_F(DrawPropertiesTest, ClipParentDrawsIntoScaledRootSurface) {
  LayerImpl* root = root_layer();
  LayerImpl* clip_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_parent_child = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* unclipped_desc_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(100, 100));
  clip_layer->SetBounds(gfx::Size(20, 20));
  clip_parent->SetBounds(gfx::Size(50, 50));
  clip_parent_child->SetBounds(gfx::Size(20, 20));
  unclipped_desc_surface->SetBounds(gfx::Size(100, 100));
  unclipped_desc_surface->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(100, 100));
  clip_child->SetDrawsContent(true);

  CopyProperties(root, clip_layer);
  CreateClipNode(clip_layer);
  CopyProperties(clip_layer, clip_parent);
  CopyProperties(clip_parent, clip_parent_child);
  CreateClipNode(clip_parent_child);
  CopyProperties(clip_parent_child, unclipped_desc_surface);
  CreateTransformNode(unclipped_desc_surface).local.Translate(10, 10);
  CreateEffectNode(unclipped_desc_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(unclipped_desc_surface, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  float device_scale_factor = 1.f;
  UpdateActiveTreeDrawProperties(device_scale_factor);
  EXPECT_EQ(gfx::Rect(-10, -10, 20, 20), clip_child->clip_rect());
  EXPECT_EQ(gfx::Rect(10, 10), clip_child->visible_layer_rect());

  device_scale_factor = 2.f;
  UpdateActiveTreeDrawProperties(device_scale_factor);
  EXPECT_EQ(gfx::Rect(-20, -20, 40, 40), clip_child->clip_rect());
  EXPECT_EQ(gfx::Rect(10, 10), clip_child->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, ClipChildVisibleRect) {
  LayerImpl* root = root_layer();
  LayerImpl* clip_parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(30, 30));
  clip_parent->SetBounds(gfx::Size(40, 40));
  render_surface->SetBounds(gfx::Size(50, 50));
  render_surface->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(50, 50));
  clip_child->SetDrawsContent(true);

  CopyProperties(root, clip_parent);
  CreateClipNode(clip_parent);
  CopyProperties(clip_parent, render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(render_surface);
  CopyProperties(render_surface, clip_child);
  clip_child->SetClipTreeIndex(clip_parent->clip_tree_index());

  UpdateActiveTreeDrawProperties();
  EXPECT_EQ(gfx::Rect(30, 30), clip_child->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, LayerClipRectLargerThanClippingRenderSurfaceRect) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* test_layer = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(30, 30));
  root->SetDrawsContent(true);
  render_surface->SetBounds(gfx::Size(50, 50));
  render_surface->SetDrawsContent(true);

  test_layer->SetBounds(gfx::Size(50, 50));
  test_layer->SetDrawsContent(true);

  CreateClipNode(root);
  CopyProperties(root, render_surface);
  CreateTransformNode(render_surface);
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CreateClipNode(render_surface);
  CopyProperties(render_surface, test_layer);
  CreateClipNode(test_layer);

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::Rect(30, 30), root->clip_rect());
  EXPECT_EQ(gfx::Rect(50, 50), render_surface->clip_rect());
  EXPECT_EQ(gfx::Rect(50, 50), test_layer->clip_rect());
}

// Needs layer tree mode: hide_layer_and_subtree.
TEST_F(DrawPropertiesTestWithLayerTree, SubtreeIsHiddenTest) {
  // Tests that subtree is hidden is updated.
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto hidden = Layer::Create();
  root->AddChild(hidden);
  auto test = Layer::Create();
  hidden->AddChild(test);

  root->SetBounds(gfx::Size(30, 30));
  hidden->SetBounds(gfx::Size(30, 30));
  hidden->SetForceRenderSurfaceForTesting(true);
  hidden->SetHideLayerAndSubtree(true);
  test->SetBounds(gfx::Size(30, 30));
  test->SetForceRenderSurfaceForTesting(true);

  CommitAndActivate();

  EXPECT_EQ(
      0.f,
      GetRenderSurfaceImpl(test)->OwningEffectNode()->screen_space_opacity);

  hidden->SetHideLayerAndSubtree(false);
  CommitAndActivate();
  EXPECT_EQ(
      1.f,
      GetRenderSurfaceImpl(test)->OwningEffectNode()->screen_space_opacity);
}

TEST_F(DrawPropertiesTest, TwoUnclippedRenderSurfaces) {
  LayerImpl* root = root_layer();
  LayerImpl* clip_layer = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* clip_child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(30, 30));
  clip_layer->SetBounds(gfx::Size(30, 30));
  render_surface1->SetBounds(gfx::Size(30, 30));
  render_surface1->SetDrawsContent(true);
  render_surface2->SetBounds(gfx::Size(30, 30));
  render_surface2->SetDrawsContent(true);
  clip_child->SetBounds(gfx::Size(30, 30));
  clip_child->SetDrawsContent(true);

  CreateClipNode(root);
  CopyProperties(root, clip_layer);
  CreateClipNode(clip_layer);
  CopyProperties(clip_layer, render_surface1);
  CreateTransformNode(render_surface1).post_translation =
      gfx::Vector2dF(10, 10);
  CreateEffectNode(render_surface1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface1, render_surface2);
  CreateEffectNode(render_surface2).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface2, clip_child);
  clip_child->SetClipTreeIndex(root->clip_tree_index());

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::Rect(-10, -10, 30, 30), render_surface2->clip_rect());
}

// Needs layer tree mode: mask layer.
TEST_F(DrawPropertiesTestWithLayerTree, MaskLayerDrawProperties) {
  // Tests that a mask layer's draw properties are computed correctly.
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto child = Layer::Create();
  root->AddChild(child);
  FakeContentLayerClient client;
  auto mask = PictureLayer::Create(&client);
  child->SetMaskLayer(mask);

  gfx::Transform transform;
  transform.Translate(10, 10);

  root->SetBounds(gfx::Size(40, 40));
  root->SetIsDrawable(true);
  child->SetTransform(transform);
  child->SetBounds(gfx::Size(30, 30));
  child->SetIsDrawable(true);
  child->SetOpacity(0.f);
  mask->SetBounds(gfx::Size(30, 30));

  CommitAndActivate();

  // The render surface created for the mask has no contributing content, so the
  // mask doesn't contribute to a drawn render surface. This means it has an
  // empty visible rect, but its screen space transform can still be computed
  // correctly on-demand.
  EXPECT_FALSE(ImplOf(child)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(mask)->contributes_to_drawn_render_surface());
  EXPECT_EQ(gfx::Rect(), ImplOf(mask)->visible_layer_rect());
  EXPECT_TRANSFORM_EQ(transform, ImplOf(mask)->ScreenSpaceTransform());

  // Make the child's render surface have contributing content.
  child->SetOpacity(1.f);
  CommitAndActivate();
  EXPECT_TRUE(ImplOf(mask)->contributes_to_drawn_render_surface());
  EXPECT_EQ(gfx::Rect(30, 30), ImplOf(mask)->visible_layer_rect());
  EXPECT_TRANSFORM_EQ(transform, ImplOf(mask)->ScreenSpaceTransform());

  transform.Translate(10, 10);
  child->SetTransform(transform);
  CommitAndActivate();
  EXPECT_TRANSFORM_EQ(transform, ImplOf(mask)->ScreenSpaceTransform());
  EXPECT_EQ(gfx::Rect(20, 20), ImplOf(mask)->visible_layer_rect());

  // For now SetIsDrawable of masked layer doesn't affect draw properties of
  // the mask layer because it doesn't affect property trees.
  child->SetIsDrawable(false);
  CommitAndActivate();
  EXPECT_TRUE(ImplOf(mask)->contributes_to_drawn_render_surface());

  // SetIsDrawable of mask layer itself affects its draw properties.
  mask->SetIsDrawable(false);
  CommitAndActivate();
  EXPECT_FALSE(ImplOf(mask)->contributes_to_drawn_render_surface());
}

TEST_F(DrawPropertiesTest, SublayerScaleWithTransformNodeBetweenTwoTargets) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* between_targets = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* render_surface2 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* test_layer = AddLayerInActiveTree<LayerImpl>();

  gfx::Transform scale;
  scale.Scale(2.f, 2.f);

  root->SetBounds(gfx::Size(30, 30));
  render_surface1->SetBounds(gfx::Size(30, 30));
  between_targets->SetBounds(gfx::Size(30, 30));
  render_surface2->SetBounds(gfx::Size(30, 30));
  test_layer->SetBounds(gfx::Size(30, 30));
  test_layer->SetDrawsContent(true);

  // We want layer between the two targets to create a clip node and effect
  // node but it shouldn't create a render surface.
  CopyProperties(root, render_surface1);
  CreateTransformNode(render_surface1).local = scale;
  CreateEffectNode(render_surface1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface1, between_targets);
  CreateEffectNode(between_targets).opacity = 0.5f;
  CreateClipNode(between_targets);
  CopyProperties(between_targets, render_surface2);
  CreateEffectNode(render_surface2).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface2, test_layer);

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::Vector2dF(2.f, 2.f),
            GetEffectNode(render_surface1)->surface_contents_scale);
  EXPECT_EQ(gfx::Vector2dF(1.f, 1.f),
            GetEffectNode(between_targets)->surface_contents_scale);
  EXPECT_EQ(gfx::Vector2dF(2.f, 2.f),
            GetEffectNode(render_surface2)->surface_contents_scale);

  EXPECT_EQ(gfx::Rect(15, 15), test_layer->visible_layer_rect());
}

TEST_F(DrawPropertiesTest, NoisyTransform) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* parent = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  root->SetBounds(gfx::Size(30, 30));
  parent->SetBounds(gfx::Size(30, 30));
  parent->SetDrawsContent(true);
  child->SetBounds(gfx::Size(30, 30));
  child->SetDrawsContent(true);

  // A noisy transform that's invertible.
  gfx::Transform transform;
  transform.set_rc(0, 0, 6.12323e-17);
  transform.set_rc(0, 2, 1);
  transform.set_rc(2, 2, 6.12323e-17);
  transform.set_rc(2, 0, -1);

  CopyProperties(root, render_surface);
  CreateTransformNode(render_surface).local = transform;
  CreateEffectNode(render_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface, parent);
  CopyProperties(parent, child);
  CreateTransformNode(child).local = transform;

  UpdateActiveTreeDrawProperties();

  gfx::Transform expected;
  expected.set_rc(0, 0, 3.749395e-33);
  expected.set_rc(0, 2, 6.12323e-17);
  expected.set_rc(2, 0, -1);
  expected.set_rc(2, 2, 6.12323e-17);
  EXPECT_TRANSFORM_EQ(expected, child->ScreenSpaceTransform());
}

TEST_F(DrawPropertiesTest, LargeTransformTest) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface1 = AddLayerInActiveTree<LayerImpl>();
  LayerImpl* child = AddLayerInActiveTree<LayerImpl>();

  child->SetDrawsContent(true);

  gfx::Transform large_transform;
  large_transform.Scale(99999999999999999999.f, 99999999999999999999.f);
  large_transform.Scale(9999999999999999999.f, 9999999999999999999.f);
  EXPECT_TRUE(std::isinf(large_transform.rc(0, 0)));
  EXPECT_TRUE(std::isinf(large_transform.rc(1, 1)));

  root->SetBounds(gfx::Size(30, 30));
  render_surface1->SetBounds(gfx::Size(30, 30));

  // TODO(sunxd): we make child have no render surface, because if the
  // child has one, the large transform applied to child will result in NaNs in
  // the draw_transform of the render_surface, thus make draw property updates
  // skip the child layer. We need further investigation into this to know
  // what exactly happens here.
  child->SetBounds(gfx::Size(30, 30));

  CopyProperties(root, render_surface1);
  CreateEffectNode(render_surface1).render_surface_reason =
      RenderSurfaceReason::kTest;
  CopyProperties(render_surface1, child);
  CreateTransformNode(child).local = large_transform;
  CreateClipNode(child);

  UpdateActiveTreeDrawProperties();

  EXPECT_EQ(gfx::RectF(),
            GetRenderSurface(render_surface1)->DrawableContentRect());

  bool is_inf_or_nan = std::isinf(child->DrawTransform().rc(0, 0)) ||
                       std::isnan(child->DrawTransform().rc(0, 0));
  EXPECT_TRUE(is_inf_or_nan);

  is_inf_or_nan = std::isinf(child->DrawTransform().rc(1, 1)) ||
                  std::isnan(child->DrawTransform().rc(1, 1));
  EXPECT_TRUE(is_inf_or_nan);

  // The root layer should be in the RenderSurfaceList.
  EXPECT_TRUE(base::Contains(GetRenderSurfaceList(), GetRenderSurface(root)));
}

#if DCHECK_IS_ON()
class DrawPropertiesTestDoubleBlurCheck : public DrawPropertiesTestBase,
                                          public testing::Test {
 public:
  DrawPropertiesTestDoubleBlurCheck()
      : DrawPropertiesTestBase(GetTestLayerTreeSettings()) {}

 private:
  static LayerTreeSettings GetTestLayerTreeSettings() {
    LayerTreeSettings s = CommitToPendingTreeLayerTreeSettings();
    s.log_on_ui_double_background_blur = true;
    return s;
  }
};

TEST_F(DrawPropertiesTestDoubleBlurCheck, CheckForNoDoubleBlurTest) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto child_1 = Layer::Create();
  auto child_2 = Layer::Create();
  root->AddChild(child_1);
  root->AddChild(child_2);

  child_1->SetIsDrawable(true);
  child_2->SetIsDrawable(true);
  root->SetBounds(gfx::Size(100, 100));
  child_1->SetBounds(gfx::Size(10, 20));
  child_2->SetBounds(gfx::Size(30, 30));

  FilterOperations blur_filter;
  blur_filter.Append(FilterOperation::CreateBlurFilter(2.0));
  child_1->SetBackdropFilters(blur_filter);
  child_2->SetBackdropFilters(blur_filter);

  ASSERT_DEATH_IF_SUPPORTED(CommitAndActivate(), "");

  gfx::Transform transform;
  transform.Translate(gfx::Vector2dF(10.0f, 20.0f));
  child_2->SetTransform(transform);

  // There should be no crash here.
  CommitAndActivate();

  auto grandchild = Layer::Create();
  grandchild->SetIsDrawable(true);
  grandchild->SetBounds(gfx::Size(20, 10));
  grandchild->SetBackdropFilters(blur_filter);
  child_1->AddChild(grandchild);
  grandchild->SetTransform(transform);

  ASSERT_DEATH_IF_SUPPORTED(CommitAndActivate(), "");
}
#endif

// In layer tree mode, not using impl-side PropertyTreeBuilder.
TEST_F(DrawPropertiesTestWithLayerTree, OpacityAnimationsTrackingTest) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> animated = Layer::Create();
  animated->SetIsDrawable(true);
  root->AddChild(animated);
  host()->SetRootLayer(root);
  host()->SetElementIdsForTesting();

  root->SetBounds(gfx::Size(100, 100));
  root->SetForceRenderSurfaceForTesting(true);
  animated->SetBounds(gfx::Size(20, 20));
  animated->SetOpacity(0.f);

  scoped_refptr<Animation> animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline()->AttachAnimation(animation);

  animation->AttachElement(animated->element_id());

  int keyframe_model_id = 0;
  std::unique_ptr<KeyframeModel> keyframe_model = KeyframeModel::Create(
      std::unique_ptr<gfx::AnimationCurve>(
          new FakeFloatTransition(1.0, 0.f, 1.f)),
      keyframe_model_id, 1,
      KeyframeModel::TargetPropertyId(TargetProperty::OPACITY));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  keyframe_model->set_time_offset(base::Milliseconds(-1000));
  KeyframeModel* keyframe_model_ptr = keyframe_model.get();
  AddKeyframeModelToElementWithExistingKeyframeEffect(
      animated->element_id(), timeline(), std::move(keyframe_model));

  UpdateMainDrawProperties();

  EffectNode* node = GetEffectNode(animated.get());
  EXPECT_FALSE(node->is_currently_animating_opacity);
  EXPECT_TRUE(node->has_potential_opacity_animation);

  keyframe_model_ptr->set_time_offset(base::Milliseconds(0));
  host()->AnimateLayers(base::TimeTicks::Max());
  node = GetEffectNode(animated.get());
  EXPECT_TRUE(node->is_currently_animating_opacity);
  EXPECT_TRUE(node->has_potential_opacity_animation);

  animation->AbortKeyframeModelsWithProperty(TargetProperty::OPACITY,
                                             false /*needs_completion*/);
  node = GetEffectNode(animated.get());
  EXPECT_FALSE(node->is_currently_animating_opacity);
  EXPECT_FALSE(node->has_potential_opacity_animation);
}

// In layer tree mode, not using impl-side PropertyTreeBuilder.
TEST_F(DrawPropertiesTestWithLayerTree, TransformAnimationsTrackingTest) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> animated = Layer::Create();
  animated->SetIsDrawable(true);
  root->AddChild(animated);
  host()->SetRootLayer(root);
  host()->SetElementIdsForTesting();

  root->SetBounds(gfx::Size(100, 100));
  root->SetForceRenderSurfaceForTesting(true);
  animated->SetBounds(gfx::Size(20, 20));

  scoped_refptr<Animation> animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline()->AttachAnimation(animation);
  animation->AttachElement(animated->element_id());

  std::unique_ptr<gfx::KeyframedTransformAnimationCurve> curve(
      gfx::KeyframedTransformAnimationCurve::Create());
  gfx::TransformOperations start;
  start.AppendTranslate(1.f, 2.f, 3.f);
  gfx::Transform transform;
  transform.Scale3d(1.0, 2.0, 3.0);
  gfx::TransformOperations operation;
  operation.AppendMatrix(transform);
  curve->AddKeyframe(
      gfx::TransformKeyframe::Create(base::TimeDelta(), start, nullptr));
  curve->AddKeyframe(
      gfx::TransformKeyframe::Create(base::Seconds(1.0), operation, nullptr));
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), 3, 3,
      KeyframeModel::TargetPropertyId(TargetProperty::TRANSFORM)));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  keyframe_model->set_time_offset(base::Milliseconds(-1000));
  KeyframeModel* keyframe_model_ptr = keyframe_model.get();
  AddKeyframeModelToElementWithExistingKeyframeEffect(
      animated->element_id(), timeline(), std::move(keyframe_model));

  UpdateMainDrawProperties();

  TransformNode* node = GetTransformNode(animated.get());
  EXPECT_FALSE(node->is_currently_animating);
  EXPECT_TRUE(node->has_potential_animation);

  keyframe_model_ptr->set_time_offset(base::Milliseconds(0));
  host()->AnimateLayers(base::TimeTicks::Max());
  node = GetTransformNode(animated.get());
  EXPECT_TRUE(node->is_currently_animating);
  EXPECT_TRUE(node->has_potential_animation);

  animation->AbortKeyframeModelsWithProperty(TargetProperty::TRANSFORM,
                                             false /*needs_completion*/);
  node = GetTransformNode(animated.get());
  EXPECT_FALSE(node->is_currently_animating);
  EXPECT_FALSE(node->has_potential_animation);
}

// Needs layer tree mode: copy request.
TEST_F(DrawPropertiesTestWithLayerTree, CopyRequestScalingTest) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto scale_layer = Layer::Create();
  root->AddChild(scale_layer);
  auto copy_layer = Layer::Create();
  scale_layer->AddChild(copy_layer);
  auto clip_layer = Layer::Create();
  copy_layer->AddChild(clip_layer);
  auto test_layer = Layer::Create();
  clip_layer->AddChild(test_layer);

  root->SetBounds(gfx::Size(150, 150));

  scale_layer->SetBounds(gfx::Size(30, 30));
  gfx::Transform transform;
  transform.Scale(5.f, 5.f);
  scale_layer->SetTransform(transform);

  // Need to persist the render surface after copy request is cleared.
  copy_layer->SetForceRenderSurfaceForTesting(true);
  copy_layer->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());

  clip_layer->SetIsDrawable(true);
  clip_layer->SetMasksToBounds(true);
  clip_layer->SetBounds(gfx::Size(10, 10));

  test_layer->SetIsDrawable(true);
  test_layer->SetMasksToBounds(true);
  test_layer->SetBounds(gfx::Size(20, 20));

  CommitAndActivate();

  // Check surface with copy request draw properties.
  EXPECT_EQ(gfx::Rect(50, 50),
            GetRenderSurfaceImpl(copy_layer)->content_rect());
  EXPECT_EQ(gfx::Transform(),
            GetRenderSurfaceImpl(copy_layer)->draw_transform());
  EXPECT_EQ(gfx::RectF(50.0f, 50.0f),
            GetRenderSurfaceImpl(copy_layer)->DrawableContentRect());

  // Check test layer draw properties.
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(test_layer)->visible_layer_rect());
  EXPECT_EQ(transform, ImplOf(test_layer)->DrawTransform());
  EXPECT_EQ(gfx::Rect(50, 50), ImplOf(test_layer)->clip_rect());
  EXPECT_EQ(gfx::Rect(50, 50),
            ImplOf(test_layer)->visible_drawable_content_rect());

  // Clear the copy request and call UpdateSurfaceContentsScale.
  GetPropertyTrees(root.get())->effect_tree_mutable().ClearCopyRequests();
  CommitAndActivate();
}

// Needs layer tree mode: hide_layer_and_subtree, etc.
TEST_F(DrawPropertiesTestWithLayerTree, SubtreeHiddenWithCacheRenderSurface) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  root->SetBounds(gfx::Size(50, 50));
  root->SetIsDrawable(true);

  auto cache_grand_parent_sibling_before = Layer::Create();
  root->AddChild(cache_grand_parent_sibling_before);
  cache_grand_parent_sibling_before->SetBounds(gfx::Size(40, 40));
  cache_grand_parent_sibling_before->SetIsDrawable(true);

  auto cache_grand_parent = Layer::Create();
  root->AddChild(cache_grand_parent);
  cache_grand_parent->SetBounds(gfx::Size(40, 40));
  cache_grand_parent->SetIsDrawable(true);

  auto cache_parent = Layer::Create();
  cache_grand_parent->AddChild(cache_parent);
  cache_parent->SetBounds(gfx::Size(30, 30));
  cache_parent->SetIsDrawable(true);
  cache_parent->SetForceRenderSurfaceForTesting(true);

  auto cache_render_surface = Layer::Create();
  cache_parent->AddChild(cache_render_surface);
  cache_render_surface->SetBounds(gfx::Size(20, 20));
  cache_render_surface->SetIsDrawable(true);
  cache_render_surface->SetCacheRenderSurface(true);

  auto cache_child = Layer::Create();
  cache_render_surface->AddChild(cache_child);
  cache_child->SetBounds(gfx::Size(20, 20));
  cache_child->SetIsDrawable(true);

  auto cache_grand_child = Layer::Create();
  cache_child->AddChild(cache_grand_child);
  cache_grand_child->SetBounds(gfx::Size(20, 20));
  cache_grand_child->SetIsDrawable(true);

  auto cache_grand_parent_sibling_after = Layer::Create();
  root->AddChild(cache_grand_parent_sibling_after);
  cache_grand_parent_sibling_after->SetBounds(gfx::Size(40, 40));
  cache_grand_parent_sibling_after->SetIsDrawable(true);

  // Hide the cache_grand_parent and its subtree. But cache a render surface in
  // that hidden subtree on cache_layer. Also hide the cache grand child and its
  // subtree.
  cache_grand_parent->SetHideLayerAndSubtree(true);
  cache_grand_parent_sibling_before->SetHideLayerAndSubtree(true);
  cache_grand_parent_sibling_after->SetHideLayerAndSubtree(true);
  cache_grand_child->SetHideLayerAndSubtree(true);

  host()->SetElementIdsForTesting();
  CommitAndActivate();

  // We should have four render surfaces, one for the root, one for the grand
  // parent since it has opacity and two drawing descendants, one for the parent
  // since it owns a surface, and one for the cache.
  ASSERT_EQ(4u, GetRenderSurfaceList().size());
  EXPECT_EQ(root->element_id(), GetRenderSurfaceList().at(0)->id());
  EXPECT_EQ(cache_grand_parent->element_id(),
            GetRenderSurfaceList().at(1)->id());
  EXPECT_EQ(cache_parent->element_id(), GetRenderSurfaceList().at(2)->id());
  EXPECT_EQ(cache_render_surface->element_id(),
            GetRenderSurfaceList().at(3)->id());

  // The root render surface should have 2 contributing layers.
  EXPECT_EQ(2, GetRenderSurfaceImpl(root)->num_contributors());
  EXPECT_TRUE(ImplOf(root)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(
      ImplOf(cache_grand_parent)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(cache_grand_parent_sibling_before)
                   ->contributes_to_drawn_render_surface());
  EXPECT_FALSE(ImplOf(cache_grand_parent_sibling_after)
                   ->contributes_to_drawn_render_surface());

  // Nothing actually draws into the cache parent, so only the cache will
  // appear in its list, since it needs to be drawn for the cache render
  // surface.
  ASSERT_EQ(1, GetRenderSurfaceImpl(cache_parent)->num_contributors());
  EXPECT_FALSE(ImplOf(cache_parent)->contributes_to_drawn_render_surface());

  // The cache layer's render surface should have 2 contributing layers.
  ASSERT_EQ(2, GetRenderSurfaceImpl(cache_render_surface)->num_contributors());
  EXPECT_TRUE(
      ImplOf(cache_render_surface)->contributes_to_drawn_render_surface());
  EXPECT_TRUE(ImplOf(cache_child)->contributes_to_drawn_render_surface());
  EXPECT_FALSE(
      ImplOf(cache_grand_child)->contributes_to_drawn_render_surface());

  // cache_grand_parent, cache_parent shouldn't be drawn because they are
  // hidden, but the cache and cache_child should be drawn for the cache
  // render surface. cache grand child should not be drawn as its hidden even in
  // the cache render surface.
  EXPECT_FALSE(GetEffectNode(ImplOf(cache_grand_parent))->is_drawn);
  EXPECT_FALSE(GetEffectNode(ImplOf(cache_parent))->is_drawn);
  EXPECT_TRUE(GetEffectNode(ImplOf(cache_render_surface))->is_drawn);
  EXPECT_TRUE(GetEffectNode(ImplOf(cache_child))->is_drawn);
  EXPECT_FALSE(GetEffectNode(ImplOf(cache_grand_child))->is_drawn);

  // Though cache is drawn, it shouldn't contribute to drawn surface as
  // its actually hidden.
  EXPECT_FALSE(GetRenderSurfaceImpl(cache_render_surface)
                   ->contributes_to_drawn_surface());
}

// Needs layer tree mode: copy request.
TEST_F(DrawPropertiesTestWithLayerTree,
       VisibleRectInNonRootCacheRenderSurface) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  root->SetBounds(gfx::Size(50, 50));
  root->SetIsDrawable(true);
  root->SetMasksToBounds(true);

  auto cache_render_surface_layer = Layer::Create();
  root->AddChild(cache_render_surface_layer);
  cache_render_surface_layer->SetBounds(gfx::Size(120, 120));
  cache_render_surface_layer->SetIsDrawable(true);
  cache_render_surface_layer->SetCacheRenderSurface(true);

  auto copy_layer = Layer::Create();
  cache_render_surface_layer->AddChild(copy_layer);
  copy_layer->SetBounds(gfx::Size(100, 100));
  copy_layer->SetIsDrawable(true);
  copy_layer->SetForceRenderSurfaceForTesting(true);

  auto copy_child = Layer::Create();
  copy_layer->AddChild(copy_child);
  copy_child->SetPosition(gfx::PointF(40.f, 40.f));
  copy_child->SetBounds(gfx::Size(20, 20));
  copy_child->SetIsDrawable(true);

  auto copy_clip = Layer::Create();
  copy_layer->AddChild(copy_clip);
  copy_clip->SetBounds(gfx::Size(55, 55));
  copy_clip->SetMasksToBounds(true);

  auto copy_clipped_child = Layer::Create();
  copy_clip->AddChild(copy_clipped_child);
  copy_clipped_child->SetPosition(gfx::PointF(40.f, 40.f));
  copy_clipped_child->SetBounds(gfx::Size(20, 20));
  copy_clipped_child->SetIsDrawable(true);

  auto cache_surface = Layer::Create();
  copy_clip->AddChild(cache_surface);
  cache_surface->SetPosition(gfx::PointF(45.f, 45.f));
  cache_surface->SetBounds(gfx::Size(20, 20));
  cache_surface->SetIsDrawable(true);

  CommitAndActivate();

  EXPECT_EQ(gfx::Rect(120, 120),
            ImplOf(cache_render_surface_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(100, 100), ImplOf(copy_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(20, 20), ImplOf(copy_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(15, 15),
            ImplOf(copy_clipped_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(cache_surface)->visible_layer_rect());

  // Case 2: When the non root cache render surface layer is clipped.
  cache_render_surface_layer->SetBounds(gfx::Size(50, 50));
  cache_render_surface_layer->SetMasksToBounds(true);
  CommitAndActivate();
  EXPECT_EQ(gfx::Rect(50, 50),
            ImplOf(cache_render_surface_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(50, 50), ImplOf(copy_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(copy_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10),
            ImplOf(copy_clipped_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(5, 5), ImplOf(cache_surface)->visible_layer_rect());

  // Case 3: When there is device scale factor.
  float device_scale_factor = 2.f;
  CommitAndActivate(device_scale_factor);
  EXPECT_EQ(gfx::Rect(50, 50),
            ImplOf(cache_render_surface_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(50, 50), ImplOf(copy_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(copy_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10),
            ImplOf(copy_clipped_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(5, 5), ImplOf(cache_surface)->visible_layer_rect());

  // Case 4: When the non root cache render surface layer is clipped and there
  // is a copy request layer beneath it.
  copy_layer->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());
  ASSERT_TRUE(copy_layer->HasCopyRequest());
  CommitAndActivate();
  ASSERT_FALSE(copy_layer->HasCopyRequest());
  EXPECT_EQ(gfx::Rect(50, 50),
            ImplOf(cache_render_surface_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(100, 100), ImplOf(copy_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(20, 20), ImplOf(copy_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(15, 15),
            ImplOf(copy_clipped_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), ImplOf(cache_surface)->visible_layer_rect());

  // Case 5: When there is another cache render surface layer under the copy
  // request layer.
  cache_surface->SetCacheRenderSurface(true);
  copy_layer->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());
  ASSERT_TRUE(copy_layer->HasCopyRequest());
  CommitAndActivate();
  ASSERT_FALSE(copy_layer->HasCopyRequest());
  EXPECT_EQ(gfx::Rect(50, 50),
            ImplOf(cache_render_surface_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(100, 100), ImplOf(copy_layer)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(20, 20), ImplOf(copy_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(15, 15),
            ImplOf(copy_clipped_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(20, 20), ImplOf(cache_surface)->visible_layer_rect());
}

// In layer tree mode, not using impl-side PropertyTreeBuilder.
TEST_F(DrawPropertiesTestWithLayerTree, CustomLayerClipBounds) {
  // The custom clip API should have the same effect as if an intermediate
  // clip layer has been added to the layer tree. To check this the test creates
  // 2 subtree for a root layer. One of the subtree uses the clip API to clip
  // its subtree while the other uses an intermediate layer. The resulting clip
  // in draw properties are expected to be the same.
  // -Root
  //   - Parent [Clip set to |kClipBounds| using API]
  //     - Child
  //   - Clip Layer [Masks to bounds = true] [Layer bounds set to |kClipBounds|]
  //     - Expected Parent
  //       - Expected Child
  constexpr float kDeviceScale = 1.f;

  const gfx::Rect kRootLayerBounds(0, 0, 100, 100);
  const gfx::Rect kParentLayerBounds(0, 0, 50, 100);
  const gfx::Rect kChildLayerBounds(20, 20, 30, 60);

  constexpr gfx::Rect kClipBounds(10, 10, 50, 50);

  // The position of |Expected Parent| on screen should be same as |Parent|.
  const gfx::Rect kExpectedParentLayerBounds(
      gfx::Point(0, 0) - kClipBounds.OffsetFromOrigin(), gfx::Size(50, 100));

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();

  scoped_refptr<Layer> clip_layer = Layer::Create();
  scoped_refptr<Layer> expected_parent = Layer::Create();
  scoped_refptr<Layer> expected_child = Layer::Create();

  root->AddChild(parent);
  parent->AddChild(child);

  root->AddChild(clip_layer);
  clip_layer->AddChild(expected_parent);
  expected_parent->AddChild(expected_child);

  host()->SetRootLayer(root);

  root->SetIsDrawable(true);
  parent->SetIsDrawable(true);
  child->SetIsDrawable(true);
  expected_parent->SetIsDrawable(true);
  expected_child->SetIsDrawable(true);

  // Set layer positions.
  root->SetPosition(gfx::PointF(kRootLayerBounds.origin()));
  parent->SetPosition(gfx::PointF(kParentLayerBounds.origin()));
  child->SetPosition(gfx::PointF(kChildLayerBounds.origin()));

  clip_layer->SetPosition(gfx::PointF(kClipBounds.origin()));
  expected_parent->SetPosition(
      gfx::PointF(kExpectedParentLayerBounds.origin()));
  expected_child->SetPosition(gfx::PointF(kChildLayerBounds.origin()));

  root->SetBounds(kRootLayerBounds.size());
  parent->SetBounds(kParentLayerBounds.size());
  child->SetBounds(kChildLayerBounds.size());

  clip_layer->SetBounds(kClipBounds.size());
  expected_parent->SetBounds(kExpectedParentLayerBounds.size());
  expected_child->SetBounds(kChildLayerBounds.size());

  parent->SetClipRect(kClipBounds);
  clip_layer->SetMasksToBounds(true);

  UpdateMainDrawProperties(kDeviceScale);

  EXPECT_EQ(GetClipNode(parent.get())->clip, gfx::RectF(kClipBounds));
  EXPECT_TRUE(!parent->clip_rect().IsEmpty());

  EXPECT_EQ(GetClipNode(child.get())->clip, gfx::RectF(kClipBounds));

  CommitAndActivate(kDeviceScale);
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  // Get the layer impl for each Layer.
  LayerImpl* parent_impl = layer_tree_impl->LayerById(parent->id());
  LayerImpl* child_impl = layer_tree_impl->LayerById(child->id());
  LayerImpl* expected_parent_impl =
      layer_tree_impl->LayerById(expected_parent->id());
  LayerImpl* expected_child_impl =
      layer_tree_impl->LayerById(expected_child->id());

  EXPECT_EQ(kDeviceScale, layer_tree_impl->device_scale_factor());

  EXPECT_TRUE(parent_impl->is_clipped());
  EXPECT_TRUE(child_impl->is_clipped());
  ASSERT_TRUE(expected_parent_impl->is_clipped());
  ASSERT_TRUE(expected_child_impl->is_clipped());

  EXPECT_EQ(parent_impl->clip_rect(), expected_parent_impl->clip_rect());
  EXPECT_EQ(child_impl->clip_rect(), expected_child_impl->clip_rect());
}

// In layer tree mode, not using impl-side PropertyTreeBuilder.
TEST_F(DrawPropertiesTestWithLayerTree, CustomLayerClipBoundsWithMaskToBounds) {
  // The custom clip API should have the same effect as if an intermediate
  // clip layer has been added to the layer tree. To check this the test creates
  // 2 subtree for a root layer. One of the subtree uses the clip API to clip
  // its subtree while the other uses an intermediate layer. The resulting clip
  // in draw properties are expected to be the same. In this test, the subtree
  // roots also have their masks to bounds property set.
  // -Root
  //   - Parent [Clip set to |kClipBounds| using API]
  //     - Child
  //   - Clip Layer [Masks to bounds = true] [Layer bounds set to |kClipBounds|]
  //     - Expected Parent [Masks to bounds = true]
  //       - Expected Child
  constexpr float kDeviceScale = 1.f;

  const gfx::Rect kRootLayerBounds(0, 0, 100, 100);
  const gfx::Rect kParentLayerBounds(0, 0, 50, 100);
  const gfx::Rect kChildLayerBounds(20, 20, 30, 60);

  constexpr gfx::Rect kClipBounds(10, 10, 50, 50);

  // The position of |Expected Parent| on screen should be same as |Parent|.
  const gfx::Rect kExpectedParentLayerBounds(
      gfx::Point(0, 0) - kClipBounds.OffsetFromOrigin(), gfx::Size(50, 100));

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();

  scoped_refptr<Layer> clip_layer = Layer::Create();
  scoped_refptr<Layer> expected_parent = Layer::Create();
  scoped_refptr<Layer> expected_child = Layer::Create();

  root->AddChild(parent);
  parent->AddChild(child);

  root->AddChild(clip_layer);
  clip_layer->AddChild(expected_parent);
  expected_parent->AddChild(expected_child);

  host()->SetRootLayer(root);

  root->SetIsDrawable(true);
  parent->SetIsDrawable(true);
  child->SetIsDrawable(true);
  expected_parent->SetIsDrawable(true);
  expected_child->SetIsDrawable(true);

  // Set layer positions.
  root->SetPosition(gfx::PointF(kRootLayerBounds.origin()));
  parent->SetPosition(gfx::PointF(kParentLayerBounds.origin()));
  child->SetPosition(gfx::PointF(kChildLayerBounds.origin()));

  clip_layer->SetPosition(gfx::PointF(kClipBounds.origin()));
  expected_parent->SetPosition(
      gfx::PointF(kExpectedParentLayerBounds.origin()));
  expected_child->SetPosition(gfx::PointF(kChildLayerBounds.origin()));

  root->SetBounds(kRootLayerBounds.size());
  parent->SetBounds(kParentLayerBounds.size());
  child->SetBounds(kChildLayerBounds.size());

  clip_layer->SetBounds(kClipBounds.size());
  expected_parent->SetBounds(kExpectedParentLayerBounds.size());
  expected_child->SetBounds(kChildLayerBounds.size());

  parent->SetClipRect(kClipBounds);
  parent->SetMasksToBounds(true);

  clip_layer->SetMasksToBounds(true);
  expected_parent->SetMasksToBounds(true);

  UpdateMainDrawProperties(kDeviceScale);

  const gfx::RectF expected_clip_bounds = gfx::IntersectRects(
      gfx::RectF(kClipBounds), gfx::RectF(kParentLayerBounds));
  EXPECT_EQ(GetClipNode(parent.get())->clip, expected_clip_bounds);
  EXPECT_TRUE(!parent->clip_rect().IsEmpty());

  EXPECT_EQ(GetClipNode(child.get())->clip, expected_clip_bounds);

  CommitAndActivate(kDeviceScale);
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  // Get the layer impl for each Layer.
  LayerImpl* parent_impl = layer_tree_impl->LayerById(parent->id());
  LayerImpl* child_impl = layer_tree_impl->LayerById(child->id());
  LayerImpl* expected_parent_impl =
      layer_tree_impl->LayerById(expected_parent->id());
  LayerImpl* expected_child_impl =
      layer_tree_impl->LayerById(expected_child->id());

  EXPECT_EQ(kDeviceScale, layer_tree_impl->device_scale_factor());

  EXPECT_TRUE(parent_impl->is_clipped());
  EXPECT_TRUE(child_impl->is_clipped());
  ASSERT_TRUE(expected_parent_impl->is_clipped());
  ASSERT_TRUE(expected_child_impl->is_clipped());

  EXPECT_EQ(parent_impl->clip_rect(), expected_parent_impl->clip_rect());
  EXPECT_EQ(child_impl->clip_rect(), expected_child_impl->clip_rect());
}

struct MaskFilterTestCase {
  std::string test_name;
  gfx::RoundedCornersF rounded_corners;
  gfx::LinearGradient gradient_mask;
};

class DrawPropertiesWithLayerTreeTest :
    public DrawPropertiesTestWithLayerTree,
    public testing::WithParamInterface<MaskFilterTestCase> {
};

// In layer tree mode, not using impl-side PropertyTreeBuilder.
TEST_P(DrawPropertiesWithLayerTreeTest, MaskFilterOnRenderSurface) {
  // -Root
  //   - Parent 1
  //     - [Render Surface] Child 1 with rounded corner and/or gradient mask
  //   - [Render Surface] Parent 2 with rounded corner and/or gradient mask
  //     - [Render Surface] Child 2
  //   - Parent 3 with rounded corner and/or gradient mask
  //     - [Render Surface] Child 3

  const MaskFilterTestCase test_case = GetParam();
  gfx::LinearGradient gradient_mask = test_case.gradient_mask;
  if (!gradient_mask.IsEmpty())
    gradient_mask.AddStep(50, 0x50);

  scoped_refptr<Layer> root = Layer::Create();
  host()->SetRootLayer(root);
  root->SetBounds(gfx::Size(250, 250));
  root->SetIsDrawable(true);

  scoped_refptr<Layer> parent_1 = Layer::Create();
  root->AddChild(parent_1);
  parent_1->SetBounds(gfx::Size(80, 80));
  parent_1->SetPosition(gfx::PointF(0, 0));
  parent_1->SetIsDrawable(true);

  scoped_refptr<Layer> parent_2 = Layer::Create();
  root->AddChild(parent_2);
  parent_2->SetBounds(gfx::Size(80, 80));
  parent_1->SetPosition(gfx::PointF(80, 80));
  parent_2->SetIsDrawable(true);
  parent_2->SetForceRenderSurfaceForTesting(true);
  parent_2->SetRoundedCorner(test_case.rounded_corners);
  parent_2->SetGradientMask(gradient_mask);
  parent_2->SetIsFastRoundedCorner(true);

  scoped_refptr<Layer> parent_3 = Layer::Create();
  root->AddChild(parent_3);
  parent_3->SetBounds(gfx::Size(80, 80));
  parent_1->SetPosition(gfx::PointF(160, 160));
  parent_3->SetIsDrawable(true);
  parent_3->SetRoundedCorner(test_case.rounded_corners);
  parent_3->SetGradientMask(gradient_mask);
  parent_3->SetIsFastRoundedCorner(true);

  scoped_refptr<Layer> child_1 = Layer::Create();
  parent_1->AddChild(child_1);
  child_1->SetBounds(gfx::Size(80, 80));
  child_1->SetIsDrawable(true);
  child_1->SetForceRenderSurfaceForTesting(true);
  child_1->SetRoundedCorner(test_case.rounded_corners);
  child_1->SetGradientMask(gradient_mask);
  child_1->SetIsFastRoundedCorner(true);

  scoped_refptr<Layer> child_2 = Layer::Create();
  parent_2->AddChild(child_2);
  child_2->SetBounds(gfx::Size(80, 80));
  child_2->SetIsDrawable(true);
  child_2->SetForceRenderSurfaceForTesting(true);

  scoped_refptr<Layer> child_3 = Layer::Create();
  parent_3->AddChild(child_3);
  child_3->SetBounds(gfx::Size(80, 80));
  child_3->SetIsDrawable(true);
  child_3->SetForceRenderSurfaceForTesting(true);

  UpdateMainDrawProperties();
  CommitAndActivate();

  EXPECT_NE(test_case.rounded_corners.IsEmpty(),
      GetRenderSurfaceImpl(child_1)->mask_filter_info().HasRoundedCorners());
  EXPECT_NE(test_case.rounded_corners.IsEmpty(),
      GetRenderSurfaceImpl(child_2)->mask_filter_info().HasRoundedCorners());
  EXPECT_NE(test_case.rounded_corners.IsEmpty(),
      GetRenderSurfaceImpl(child_3)->mask_filter_info().HasRoundedCorners());

  EXPECT_NE(test_case.gradient_mask.IsEmpty(),
      GetRenderSurfaceImpl(child_1)->mask_filter_info().HasGradientMask());
  EXPECT_NE(test_case.gradient_mask.IsEmpty(),
      GetRenderSurfaceImpl(child_2)->mask_filter_info().HasGradientMask());
  EXPECT_NE(test_case.gradient_mask.IsEmpty(),
      GetRenderSurfaceImpl(child_3)->mask_filter_info().HasGradientMask());
  }


INSTANTIATE_TEST_SUITE_P(
    DrawPropertiesWithLayerTreeTests,
    DrawPropertiesWithLayerTreeTest,
    testing::ValuesIn<MaskFilterTestCase>({
        {"WithRoundedCorners", gfx::RoundedCornersF(10.f),
         gfx::LinearGradient::GetEmpty()},
        {"WithGradientMask", gfx::RoundedCornersF(0.f), gfx::LinearGradient(45)},
        {"WithRoundedCornersAndGradientMask", gfx::RoundedCornersF(10.f),
         gfx::LinearGradient(45)},
    }),
    [](const testing::TestParamInfo<DrawPropertiesWithLayerTreeTest::ParamType>&
           info) { return info.param.test_name; });
}  // namespace
}  // namespace cc
