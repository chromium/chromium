// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_builder.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/texture_layer.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class PropertyTreeBuilderTest : public LayerTreeImplTestBase,
                                public testing::Test {
 public:
  PropertyTreeBuilderTest() : LayerTreeImplTestBase(LayerTreeSettings()) {}

  void UpdateMainDrawProperties(float device_scale_factor = 1.0f) {
    SetDeviceScaleAndUpdateViewportRect(host(), device_scale_factor);
    UpdateDrawProperties(host());
  }

  LayerImpl* ImplOf(const scoped_refptr<Layer>& layer) {
    return layer ? host_impl()->active_tree()->LayerById(layer->id()) : nullptr;
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
    host()->CommitAndCreatePendingTree();
    // TODO(https://crbug.com/939968) This call should be handled by
    // FakeLayerTreeHost instead of manually pushing the properties from the
    // layer tree host to the pending tree.
    host()->PushLayerTreePropertiesTo(host_impl()->pending_tree());

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

  const RenderSurfaceList& GetRenderSurfaceList() {
    return host_impl()->active_tree()->GetRenderSurfaceList();
  }
};

TEST_F(PropertyTreeBuilderTest, EffectTreeTransformIdTest) {
  // Tests that effect tree node gets a valid transform id when a layer
  // has opacity but doesn't create a render surface.
  auto parent = Layer::Create();
  host()->SetRootLayer(parent);
  auto child = Layer::Create();
  parent->AddChild(child);
  child->SetIsDrawable(true);

  parent->SetBounds(gfx::Size(100, 100));
  child->SetPosition(gfx::PointF(10, 10));
  child->SetBounds(gfx::Size(100, 100));
  child->SetOpacity(0.f);
  UpdateMainDrawProperties();
  EffectNode* node = GetEffectNode(child.get());
  const int transform_tree_size =
      GetPropertyTrees(parent.get())->transform_tree.next_available_id();
  EXPECT_LT(node->transform_id, transform_tree_size);
}

TEST_F(PropertyTreeBuilderTest, RenderSurfaceForNonAxisAlignedClipping) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto rotated_and_transparent = Layer::Create();
  root->AddChild(rotated_and_transparent);
  auto clips_subtree = Layer::Create();
  rotated_and_transparent->AddChild(clips_subtree);
  auto draws_content = Layer::Create();
  clips_subtree->AddChild(draws_content);

  root->SetBounds(gfx::Size(10, 10));
  rotated_and_transparent->SetBounds(gfx::Size(10, 10));
  rotated_and_transparent->SetOpacity(0.5f);
  gfx::Transform rotate;
  rotate.Rotate(2);
  rotated_and_transparent->SetTransform(rotate);
  clips_subtree->SetBounds(gfx::Size(10, 10));
  clips_subtree->SetMasksToBounds(true);
  draws_content->SetBounds(gfx::Size(10, 10));
  draws_content->SetIsDrawable(true);

  UpdateMainDrawProperties();
  EXPECT_TRUE(GetEffectNode(clips_subtree.get())->HasRenderSurface());
}

TEST_F(PropertyTreeBuilderTest, EffectNodesForNonAxisAlignedClips) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto rotate_and_clip = Layer::Create();
  root->AddChild(rotate_and_clip);
  auto only_clip = Layer::Create();
  rotate_and_clip->AddChild(only_clip);
  auto rotate_and_clip2 = Layer::Create();
  only_clip->AddChild(rotate_and_clip2);

  gfx::Transform rotate;
  rotate.Rotate(2);
  root->SetBounds(gfx::Size(10, 10));
  rotate_and_clip->SetBounds(gfx::Size(10, 10));
  rotate_and_clip->SetTransform(rotate);
  rotate_and_clip->SetMasksToBounds(true);
  only_clip->SetBounds(gfx::Size(10, 10));
  only_clip->SetMasksToBounds(true);
  rotate_and_clip2->SetBounds(gfx::Size(10, 10));
  rotate_and_clip2->SetTransform(rotate);
  rotate_and_clip2->SetMasksToBounds(true);

  UpdateMainDrawProperties();
  // non-axis aligned clip should create an effect node
  EXPECT_NE(root->effect_tree_index(), rotate_and_clip->effect_tree_index());
  // Since only_clip's clip is in the same non-axis aligned space as
  // rotate_and_clip's clip, no new effect node should be created.
  EXPECT_EQ(rotate_and_clip->effect_tree_index(),
            only_clip->effect_tree_index());
  // rotate_and_clip2's clip and only_clip's clip are in different non-axis
  // aligned spaces. So, new effect node should be created.
  EXPECT_NE(rotate_and_clip2->effect_tree_index(),
            only_clip->effect_tree_index());
}

TEST_F(PropertyTreeBuilderTest,
       RenderSurfaceListForRenderSurfaceWithClippedLayer) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto render_surface1 = Layer::Create();
  root->AddChild(render_surface1);
  auto child = Layer::Create();
  render_surface1->AddChild(child);

  root->SetBounds(gfx::Size(10, 10));
  root->SetMasksToBounds(true);
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->SetForceRenderSurfaceForTesting(true);
  child->SetIsDrawable(true);
  child->SetPosition(gfx::PointF(30.f, 30.f));
  child->SetBounds(gfx::Size(10, 10));

  CommitAndActivate();

  // The child layer's content is entirely outside the root's clip rect, so
  // the intermediate render surface should not be listed here, even if it was
  // forced to be created. Render surfaces without children or visible content
  // are unexpected at draw time (e.g. we might try to create a content texture
  // of size 0).
  ASSERT_TRUE(GetRenderSurfaceImpl(root));
  EXPECT_EQ(1U, GetRenderSurfaceList().size());
}

TEST_F(PropertyTreeBuilderTest, RenderSurfaceListForTransparentChild) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto render_surface1 = Layer::Create();
  root->AddChild(render_surface1);
  auto child = Layer::Create();
  render_surface1->AddChild(child);

  root->SetBounds(gfx::Size(10, 10));
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->SetForceRenderSurfaceForTesting(true);
  render_surface1->SetOpacity(0.f);
  child->SetBounds(gfx::Size(10, 10));
  child->SetIsDrawable(true);

  CommitAndActivate();

  // Since the layer is transparent, render_surface1_impl->GetRenderSurface()
  // should not have gotten added anywhere.  Also, the drawable content rect
  // should not have been extended by the children.
  ASSERT_TRUE(GetRenderSurfaceImpl(root));
  EXPECT_EQ(0, GetRenderSurfaceImpl(root)->num_contributors());
  EXPECT_EQ(1U, GetRenderSurfaceList().size());
  EXPECT_EQ(static_cast<viz::RenderPassId>(root->id()),
            GetRenderSurfaceList().at(0)->id());
  EXPECT_EQ(gfx::Rect(), ImplOf(root)->drawable_content_rect());
}

TEST_F(PropertyTreeBuilderTest,
       RenderSurfaceListForTransparentChildWithBackdropFilter) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto render_surface1 = Layer::Create();
  root->AddChild(render_surface1);
  auto child = Layer::Create();
  render_surface1->AddChild(child);

  root->SetBounds(gfx::Size(10, 10));
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->SetForceRenderSurfaceForTesting(true);
  render_surface1->SetOpacity(0.f);
  render_surface1->SetIsDrawable(true);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(1.5f));
  render_surface1->SetBackdropFilters(filters);
  child->SetBounds(gfx::Size(10, 10));
  child->SetIsDrawable(true);
  host()->SetElementIdsForTesting();

  CommitAndActivate();
  EXPECT_EQ(2U, GetRenderSurfaceList().size());

  // The layer is fully transparent, but has a backdrop filter, so it
  // shouldn't be skipped and should be drawn.
  ASSERT_TRUE(GetRenderSurfaceImpl(root));
  EXPECT_EQ(1, GetRenderSurfaceImpl(root)->num_contributors());
  EXPECT_EQ(gfx::RectF(0, 0, 10, 10),
            GetRenderSurfaceImpl(root)->DrawableContentRect());
  EXPECT_TRUE(GetEffectNode(ImplOf(render_surface1))->is_drawn);

  // When root is transparent, the layer should not be drawn.
  host_impl()->active_tree()->SetOpacityMutated(root->element_id(), 0.f);
  host_impl()->active_tree()->SetOpacityMutated(render_surface1->element_id(),
                                                1.f);
  ImplOf(render_surface1)->set_visible_layer_rect(gfx::Rect());
  UpdateActiveTreeDrawProperties();

  EXPECT_FALSE(GetEffectNode(ImplOf(render_surface1))->is_drawn);
  EXPECT_EQ(gfx::Rect(), ImplOf(render_surface1)->visible_layer_rect());
}

TEST_F(PropertyTreeBuilderTest, RenderSurfaceListForFilter) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto parent = Layer::Create();
  root->AddChild(parent);
  auto child1 = Layer::Create();
  parent->AddChild(child1);
  auto child2 = Layer::Create();
  parent->AddChild(child2);

  gfx::Transform scale_matrix;
  scale_matrix.Scale(2.0f, 2.0f);

  root->SetBounds(gfx::Size(100, 100));
  parent->SetTransform(scale_matrix);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(10.0f));
  parent->SetFilters(filters);
  parent->SetForceRenderSurfaceForTesting(true);
  child1->SetBounds(gfx::Size(25, 25));
  child1->SetIsDrawable(true);
  child1->SetForceRenderSurfaceForTesting(true);
  child2->SetPosition(gfx::PointF(25, 25));
  child2->SetBounds(gfx::Size(25, 25));
  child2->SetIsDrawable(true);
  child2->SetForceRenderSurfaceForTesting(true);

  CommitAndActivate();

  ASSERT_TRUE(GetRenderSurfaceImpl(parent));
  EXPECT_EQ(2, GetRenderSurfaceImpl(parent)->num_contributors());
  EXPECT_EQ(4U, GetRenderSurfaceList().size());

  // The rectangle enclosing child1 and child2 (0,0 50x50), expanded for the
  // blur (-30,-30 110x110), and then scaled by the scale matrix
  // (-60,-60 220x220).
  EXPECT_EQ(gfx::RectF(-60, -60, 220, 220),
            GetRenderSurfaceImpl(parent)->DrawableContentRect());
}

TEST_F(PropertyTreeBuilderTest, ForceRenderSurface) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto render_surface1 = Layer::Create();
  root->AddChild(render_surface1);
  auto child = Layer::Create();
  render_surface1->AddChild(child);

  root->SetBounds(gfx::Size(10, 10));
  render_surface1->SetBounds(gfx::Size(10, 10));
  render_surface1->SetForceRenderSurfaceForTesting(true);
  child->SetBounds(gfx::Size(10, 10));
  child->SetIsDrawable(true);

  CommitAndActivate();

  // The root layer always creates a render surface
  EXPECT_TRUE(GetRenderSurfaceImpl(root));
  EXPECT_NE(GetRenderSurfaceImpl(render_surface1), GetRenderSurfaceImpl(root));

  render_surface1->SetForceRenderSurfaceForTesting(false);
  CommitAndActivate();

  EXPECT_TRUE(GetRenderSurfaceImpl(root));
  EXPECT_EQ(GetRenderSurfaceImpl(render_surface1), GetRenderSurfaceImpl(root));
}

TEST_F(PropertyTreeBuilderTest, VisibleRectWithClippingAndFilters) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto clip = Layer::Create();
  root->AddChild(clip);
  auto filter = Layer::Create();
  clip->AddChild(filter);
  auto filter_child = Layer::Create();
  filter->AddChild(filter_child);

  root->SetBounds(gfx::Size(100, 100));
  clip->SetBounds(gfx::Size(10, 10));
  filter->SetForceRenderSurfaceForTesting(true);
  filter_child->SetBounds(gfx::Size(2000, 2000));
  filter_child->SetPosition(gfx::PointF(-50, -50));
  filter_child->SetIsDrawable(true);

  clip->SetMasksToBounds(true);

  CommitAndActivate();

  EXPECT_EQ(gfx::Rect(50, 50, 10, 10),
            ImplOf(filter_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(10, 10), GetRenderSurfaceImpl(filter)->content_rect());

  FilterOperations blur_filter;
  blur_filter.Append(FilterOperation::CreateBlurFilter(4.0f));
  filter->SetFilters(blur_filter);

  CommitAndActivate();

  EXPECT_EQ(gfx::Rect(38, 38, 34, 34),
            ImplOf(filter_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(-12, -12, 34, 34),
            GetRenderSurfaceImpl(filter)->content_rect());

  gfx::Transform vertical_flip;
  vertical_flip.Scale(1, -1);
  sk_sp<PaintFilter> flip_filter = sk_make_sp<MatrixPaintFilter>(
      vertical_flip.matrix(), kLow_SkFilterQuality, nullptr);
  FilterOperations reflection_filter;
  reflection_filter.Append(
      FilterOperation::CreateReferenceFilter(sk_make_sp<XfermodePaintFilter>(
          SkBlendMode::kSrcOver, std::move(flip_filter), nullptr)));
  filter->SetFilters(reflection_filter);

  CommitAndActivate();

  EXPECT_EQ(gfx::Rect(49, 39, 12, 21),
            ImplOf(filter_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(-1, -11, 12, 21),
            GetRenderSurfaceImpl(filter)->content_rect());
}

TEST_F(PropertyTreeBuilderTest, VisibleRectWithScalingClippingAndFilters) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto scale = Layer::Create();
  root->AddChild(scale);
  auto clip = Layer::Create();
  scale->AddChild(clip);
  auto filter = Layer::Create();
  clip->AddChild(filter);
  auto filter_child = Layer::Create();
  filter->AddChild(filter_child);

  root->SetBounds(gfx::Size(100, 100));
  clip->SetBounds(gfx::Size(10, 10));
  filter->SetForceRenderSurfaceForTesting(true);
  filter_child->SetBounds(gfx::Size(2000, 2000));
  filter_child->SetPosition(gfx::PointF(-50, -50));
  filter_child->SetIsDrawable(true);

  clip->SetMasksToBounds(true);

  gfx::Transform scale_transform;
  scale_transform.Scale(3, 3);
  scale->SetTransform(scale_transform);

  CommitAndActivate();

  EXPECT_EQ(gfx::Rect(50, 50, 10, 10),
            ImplOf(filter_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(30, 30), GetRenderSurfaceImpl(filter)->content_rect());

  FilterOperations blur_filter;
  blur_filter.Append(FilterOperation::CreateBlurFilter(4.0f));
  filter->SetFilters(blur_filter);

  CommitAndActivate();

  EXPECT_EQ(gfx::Rect(38, 38, 34, 34),
            ImplOf(filter_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(-36, -36, 102, 102),
            GetRenderSurfaceImpl(filter)->content_rect());

  gfx::Transform vertical_flip;
  vertical_flip.Scale(1, -1);
  sk_sp<PaintFilter> flip_filter = sk_make_sp<MatrixPaintFilter>(
      vertical_flip.matrix(), kLow_SkFilterQuality, nullptr);
  FilterOperations reflection_filter;
  reflection_filter.Append(
      FilterOperation::CreateReferenceFilter(sk_make_sp<XfermodePaintFilter>(
          SkBlendMode::kSrcOver, std::move(flip_filter), nullptr)));
  filter->SetFilters(reflection_filter);

  CommitAndActivate();

  EXPECT_EQ(gfx::Rect(49, 39, 12, 21),
            ImplOf(filter_child)->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(-1, -31, 32, 61),
            GetRenderSurfaceImpl(filter)->content_rect());
}

TEST_F(PropertyTreeBuilderTest, TextureLayerSnapping) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto child = TextureLayer::CreateForMailbox(nullptr);
  root->AddChild(child);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(100, 100));
  child->SetIsDrawable(true);
  gfx::Transform fractional_translate;
  fractional_translate.Translate(10.5f, 20.3f);
  child->SetTransform(fractional_translate);

  CommitAndActivate();

  auto child_screen_space_transform = ImplOf(child)->ScreenSpaceTransform();
  EXPECT_NE(child_screen_space_transform, fractional_translate);
  fractional_translate.RoundTranslationComponents();
  EXPECT_TRANSFORMATION_MATRIX_EQ(child_screen_space_transform,
                                  fractional_translate);
  gfx::RectF layer_bounds_in_screen_space = MathUtil::MapClippedRect(
      child_screen_space_transform, gfx::RectF(gfx::SizeF(child->bounds())));
  EXPECT_EQ(layer_bounds_in_screen_space, gfx::RectF(11.f, 20.f, 100.f, 100.f));
}

// Verify the behavior of back-face culling when there are no preserve-3d
// layers. Note that 3d transforms still apply in this case, but they are
// "flattened" to each parent layer according to current W3C spec.
TEST_F(PropertyTreeBuilderTest, BackFaceCullingWithoutPreserves3d) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto front_facing_child = Layer::Create();
  root->AddChild(front_facing_child);
  auto back_facing_child = Layer::Create();
  root->AddChild(back_facing_child);
  auto front_facing_surface = Layer::Create();
  root->AddChild(front_facing_surface);
  auto back_facing_surface = Layer::Create();
  root->AddChild(back_facing_surface);
  auto front_facing_child_of_front_facing_surface = Layer::Create();
  front_facing_surface->AddChild(front_facing_child_of_front_facing_surface);
  auto back_facing_child_of_front_facing_surface = Layer::Create();
  front_facing_surface->AddChild(back_facing_child_of_front_facing_surface);
  auto front_facing_child_of_back_facing_surface = Layer::Create();
  back_facing_surface->AddChild(front_facing_child_of_back_facing_surface);
  auto back_facing_child_of_back_facing_surface = Layer::Create();
  back_facing_surface->AddChild(back_facing_child_of_back_facing_surface);

  // Nothing is double-sided
  front_facing_child->SetDoubleSided(false);
  back_facing_child->SetDoubleSided(false);
  front_facing_surface->SetDoubleSided(false);
  back_facing_surface->SetDoubleSided(false);
  front_facing_child_of_front_facing_surface->SetDoubleSided(false);
  back_facing_child_of_front_facing_surface->SetDoubleSided(false);
  front_facing_child_of_back_facing_surface->SetDoubleSided(false);
  back_facing_child_of_back_facing_surface->SetDoubleSided(false);

  // Everything draws content.
  front_facing_child->SetIsDrawable(true);
  back_facing_child->SetIsDrawable(true);
  front_facing_surface->SetIsDrawable(true);
  back_facing_surface->SetIsDrawable(true);
  front_facing_child_of_front_facing_surface->SetIsDrawable(true);
  back_facing_child_of_front_facing_surface->SetIsDrawable(true);
  front_facing_child_of_back_facing_surface->SetIsDrawable(true);
  back_facing_child_of_back_facing_surface->SetIsDrawable(true);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  root->SetBounds(gfx::Size(100, 100));
  front_facing_child->SetBounds(gfx::Size(100, 100));
  back_facing_child->SetBounds(gfx::Size(100, 100));
  front_facing_surface->SetBounds(gfx::Size(100, 100));
  back_facing_surface->SetBounds(gfx::Size(100, 100));
  front_facing_child_of_front_facing_surface->SetBounds(gfx::Size(100, 100));
  back_facing_child_of_front_facing_surface->SetBounds(gfx::Size(100, 100));
  front_facing_child_of_back_facing_surface->SetBounds(gfx::Size(100, 100));
  back_facing_child_of_back_facing_surface->SetBounds(gfx::Size(100, 100));

  front_facing_surface->SetForceRenderSurfaceForTesting(true);
  back_facing_surface->SetForceRenderSurfaceForTesting(true);

  back_facing_child->SetTransform(backface_matrix);
  back_facing_surface->SetTransform(backface_matrix);
  back_facing_child_of_front_facing_surface->SetTransform(backface_matrix);
  back_facing_child_of_back_facing_surface->SetTransform(backface_matrix);

  // Note: No layers preserve 3d. According to current W3C CSS gfx::Transforms
  // spec, these layers should blindly use their own local transforms to
  // determine back-face culling.
  CommitAndActivate();

  // Verify which render surfaces were created.
  EXPECT_EQ(GetRenderSurfaceImpl(front_facing_child),
            GetRenderSurfaceImpl(root));
  EXPECT_EQ(GetRenderSurfaceImpl(back_facing_child),
            GetRenderSurfaceImpl(root));
  EXPECT_NE(GetRenderSurfaceImpl(front_facing_surface),
            GetRenderSurfaceImpl(root));
  EXPECT_NE(GetRenderSurfaceImpl(back_facing_surface),
            GetRenderSurfaceImpl(root));
  EXPECT_NE(GetRenderSurfaceImpl(back_facing_surface),
            GetRenderSurfaceImpl(front_facing_surface));
  EXPECT_EQ(GetRenderSurfaceImpl(front_facing_child_of_front_facing_surface),
            GetRenderSurfaceImpl(front_facing_surface));
  EXPECT_EQ(GetRenderSurfaceImpl(back_facing_child_of_front_facing_surface),
            GetRenderSurfaceImpl(front_facing_surface));
  EXPECT_EQ(GetRenderSurfaceImpl(front_facing_child_of_back_facing_surface),
            GetRenderSurfaceImpl(back_facing_surface));
  EXPECT_EQ(GetRenderSurfaceImpl(back_facing_child_of_back_facing_surface),
            GetRenderSurfaceImpl(back_facing_surface));

  EXPECT_EQ(3u, update_layer_impl_list().size());
  EXPECT_TRUE(UpdateLayerImplListContains(front_facing_child->id()));
  EXPECT_TRUE(UpdateLayerImplListContains(front_facing_surface->id()));
  EXPECT_TRUE(UpdateLayerImplListContains(
      front_facing_child_of_front_facing_surface->id()));
}

// Verify that layers are appropriately culled when their back face is showing
// and they are not double sided, while animations are going on.
// Even layers that are animating get culled if their back face is showing and
// they are not double sided.
TEST_F(PropertyTreeBuilderTest, BackFaceCullingWithAnimatingTransforms) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto child = Layer::Create();
  root->AddChild(child);
  auto animating_surface = Layer::Create();
  root->AddChild(animating_surface);
  auto child_of_animating_surface = Layer::Create();
  animating_surface->AddChild(child_of_animating_surface);
  auto animating_child = Layer::Create();
  root->AddChild(animating_child);
  auto child2 = Layer::Create();
  root->AddChild(child2);

  // Nothing is double-sided
  child->SetDoubleSided(false);
  child2->SetDoubleSided(false);
  animating_surface->SetDoubleSided(false);
  child_of_animating_surface->SetDoubleSided(false);
  animating_child->SetDoubleSided(false);

  // Everything draws content.
  child->SetIsDrawable(true);
  child2->SetIsDrawable(true);
  animating_surface->SetIsDrawable(true);
  child_of_animating_surface->SetIsDrawable(true);
  animating_child->SetIsDrawable(true);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  host()->SetElementIdsForTesting();

  // Animate the transform on the render surface.
  AddAnimatedTransformToElementWithAnimation(animating_surface->element_id(),
                                             timeline(), 10.0, 30, 0);
  // This is just an animating layer, not a surface.
  AddAnimatedTransformToElementWithAnimation(animating_child->element_id(),
                                             timeline(), 10.0, 30, 0);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(100, 100));
  child->SetTransform(backface_matrix);
  animating_surface->SetBounds(gfx::Size(100, 100));
  animating_surface->SetTransform(backface_matrix);
  animating_surface->SetForceRenderSurfaceForTesting(true);
  child_of_animating_surface->SetBounds(gfx::Size(100, 100));
  child_of_animating_surface->SetTransform(backface_matrix);
  animating_child->SetBounds(gfx::Size(100, 100));
  animating_child->SetTransform(backface_matrix);
  child2->SetBounds(gfx::Size(100, 100));

  CommitAndActivate();

  EXPECT_EQ(GetRenderSurfaceImpl(child), GetRenderSurfaceImpl(root));
  EXPECT_TRUE(GetRenderSurfaceImpl(animating_surface));
  EXPECT_EQ(GetRenderSurfaceImpl(child_of_animating_surface),
            GetRenderSurfaceImpl(animating_surface));
  EXPECT_EQ(GetRenderSurfaceImpl(animating_child), GetRenderSurfaceImpl(root));
  EXPECT_EQ(GetRenderSurfaceImpl(child2), GetRenderSurfaceImpl(root));

  EXPECT_EQ(1u, update_layer_impl_list().size());

  // The back facing layers are culled from the layer list, and have an empty
  // visible rect.
  EXPECT_TRUE(UpdateLayerImplListContains(child2->id()));
  EXPECT_TRUE(ImplOf(child)->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(ImplOf(animating_surface)->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(
      ImplOf(child_of_animating_surface)->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(ImplOf(animating_child)->visible_layer_rect().IsEmpty());

  EXPECT_EQ(gfx::Rect(100, 100), ImplOf(child2)->visible_layer_rect());
}

// Verify that having animated opacity but current opacity 1 still creates
// a render surface.
TEST_F(PropertyTreeBuilderTest, AnimatedOpacityCreatesRenderSurface) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto child = Layer::Create();
  root->AddChild(child);
  auto grandchild = Layer::Create();
  child->AddChild(grandchild);

  root->SetBounds(gfx::Size(50, 50));
  child->SetBounds(gfx::Size(50, 50));
  child->SetIsDrawable(true);
  grandchild->SetBounds(gfx::Size(50, 50));
  grandchild->SetIsDrawable(true);

  host()->SetElementIdsForTesting();
  AddOpacityTransitionToElementWithAnimation(child->element_id(), timeline(),
                                             10.0, 1.f, 0.2f, false);
  CommitAndActivate();

  EXPECT_EQ(1.f, ImplOf(child)->Opacity());
  EXPECT_TRUE(GetRenderSurfaceImpl(root));
  EXPECT_NE(GetRenderSurfaceImpl(child), GetRenderSurfaceImpl(root));
  EXPECT_EQ(GetRenderSurfaceImpl(grandchild), GetRenderSurfaceImpl(child));
}

static bool FilterIsAnimating(LayerImpl* layer) {
  MutatorHost* host = layer->layer_tree_impl()->mutator_host();
  return host->IsAnimatingFilterProperty(layer->element_id(),
                                         layer->GetElementTypeForAnimation());
}

// Verify that having an animated filter (but no current filter, as these
// are mutually exclusive) correctly creates a render surface.
TEST_F(PropertyTreeBuilderTest, AnimatedFilterCreatesRenderSurface) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto child = Layer::Create();
  root->AddChild(child);
  auto grandchild = Layer::Create();
  child->AddChild(grandchild);

  root->SetBounds(gfx::Size(50, 50));
  child->SetBounds(gfx::Size(50, 50));
  grandchild->SetBounds(gfx::Size(50, 50));

  host()->SetElementIdsForTesting();
  AddAnimatedFilterToElementWithAnimation(child->element_id(), timeline(), 10.0,
                                          0.1f, 0.2f);
  CommitAndActivate();

  EXPECT_TRUE(GetRenderSurfaceImpl(root));
  EXPECT_NE(GetRenderSurfaceImpl(child), GetRenderSurfaceImpl(root));
  EXPECT_EQ(GetRenderSurfaceImpl(grandchild), GetRenderSurfaceImpl(child));

  EXPECT_TRUE(GetRenderSurfaceImpl(root)->Filters().IsEmpty());
  EXPECT_TRUE(GetRenderSurfaceImpl(child)->Filters().IsEmpty());

  EXPECT_FALSE(FilterIsAnimating(ImplOf(root)));
  EXPECT_TRUE(FilterIsAnimating(ImplOf(child)));
  EXPECT_FALSE(FilterIsAnimating(ImplOf(grandchild)));
}

bool HasPotentiallyRunningFilterAnimation(const LayerImpl& layer) {
  MutatorHost* host = layer.layer_tree_impl()->mutator_host();
  return host->HasPotentiallyRunningFilterAnimation(
      layer.element_id(), layer.GetElementTypeForAnimation());
}

// Verify that having a filter animation with a delayed start time creates a
// render surface.
TEST_F(PropertyTreeBuilderTest, DelayedFilterAnimationCreatesRenderSurface) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto child = Layer::Create();
  root->AddChild(child);
  auto grandchild = Layer::Create();
  child->AddChild(grandchild);

  root->SetBounds(gfx::Size(50, 50));
  child->SetBounds(gfx::Size(50, 50));
  grandchild->SetBounds(gfx::Size(50, 50));

  host()->SetElementIdsForTesting();

  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  FilterOperations start_filters;
  start_filters.Append(FilterOperation::CreateBrightnessFilter(0.1f));
  FilterOperations end_filters;
  end_filters.Append(FilterOperation::CreateBrightnessFilter(0.3f));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), start_filters, nullptr));
  curve->AddKeyframe(FilterKeyframe::Create(
      base::TimeDelta::FromMilliseconds(100), end_filters, nullptr));
  std::unique_ptr<KeyframeModel> keyframe_model =
      KeyframeModel::Create(std::move(curve), 0, 1, TargetProperty::FILTER);
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  keyframe_model->set_time_offset(base::TimeDelta::FromMilliseconds(-1000));

  AddKeyframeModelToElementWithAnimation(child->element_id(), timeline(),
                                         std::move(keyframe_model));
  CommitAndActivate();

  EXPECT_TRUE(GetRenderSurfaceImpl(root));
  EXPECT_NE(GetRenderSurfaceImpl(child), GetRenderSurfaceImpl(root));
  EXPECT_EQ(GetRenderSurfaceImpl(grandchild), GetRenderSurfaceImpl(child));

  EXPECT_TRUE(GetRenderSurfaceImpl(root)->Filters().IsEmpty());
  EXPECT_TRUE(GetRenderSurfaceImpl(child)->Filters().IsEmpty());

  EXPECT_FALSE(FilterIsAnimating(ImplOf(root)));
  EXPECT_FALSE(HasPotentiallyRunningFilterAnimation(*ImplOf(root)));
  EXPECT_FALSE(FilterIsAnimating(ImplOf(child)));
  EXPECT_TRUE(HasPotentiallyRunningFilterAnimation(*ImplOf(child)));
  EXPECT_FALSE(FilterIsAnimating(ImplOf(grandchild)));
  EXPECT_FALSE(HasPotentiallyRunningFilterAnimation(*ImplOf(grandchild)));
}

TEST_F(PropertyTreeBuilderTest, ChangingAxisAlignmentTriggersRebuild) {
  gfx::Transform translate;
  gfx::Transform rotate;

  translate.Translate(10, 10);
  rotate.Rotate(45);

  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(gfx::Size(800, 800));

  host()->SetRootLayer(root);

  UpdateMainDrawProperties();
  EXPECT_FALSE(host()->property_trees()->needs_rebuild);

  root->SetTransform(translate);
  EXPECT_FALSE(host()->property_trees()->needs_rebuild);

  root->SetTransform(rotate);
  EXPECT_TRUE(host()->property_trees()->needs_rebuild);
}

TEST_F(PropertyTreeBuilderTest, ResetPropertyTreeIndices) {
  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(gfx::Size(800, 800));

  gfx::Transform translate_z;
  translate_z.Translate3d(0, 0, 10);

  scoped_refptr<Layer> child = Layer::Create();
  child->SetTransform(translate_z);
  child->SetBounds(gfx::Size(100, 100));

  root->AddChild(child);

  host()->SetRootLayer(root);

  UpdateMainDrawProperties();
  EXPECT_NE(-1, child->transform_tree_index());

  child->RemoveFromParent();

  UpdateMainDrawProperties();
  EXPECT_EQ(-1, child->transform_tree_index());
}

TEST_F(PropertyTreeBuilderTest, RenderSurfaceClipsSubtree) {
  // Ensure that a Clip Node is added when a render surface applies clip.
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto significant_transform = Layer::Create();
  root->AddChild(significant_transform);
  auto layer_clips_subtree = Layer::Create();
  significant_transform->AddChild(layer_clips_subtree);
  auto render_surface = Layer::Create();
  layer_clips_subtree->AddChild(render_surface);
  auto test_layer = Layer::Create();
  render_surface->AddChild(test_layer);

  // This transform should be a significant one so that a transform node is
  // formed for it.
  gfx::Transform transform1;
  transform1.RotateAboutYAxis(45);
  transform1.RotateAboutXAxis(30);
  // This transform should be a 3d transform as we want the render surface
  // to flatten the transform
  gfx::Transform transform2;
  transform2.Translate3d(10, 10, 10);

  root->SetBounds(gfx::Size(30, 30));
  significant_transform->SetTransform(transform1);
  significant_transform->SetBounds(gfx::Size(30, 30));
  layer_clips_subtree->SetBounds(gfx::Size(30, 30));
  layer_clips_subtree->SetMasksToBounds(true);
  layer_clips_subtree->SetForceRenderSurfaceForTesting(true);
  render_surface->SetTransform(transform2);
  render_surface->SetBounds(gfx::Size(30, 30));
  render_surface->SetForceRenderSurfaceForTesting(true);
  test_layer->SetBounds(gfx::Size(30, 30));
  test_layer->SetIsDrawable(true);

  CommitAndActivate();

  EXPECT_TRUE(GetRenderSurfaceImpl(root));
  EXPECT_EQ(GetRenderSurfaceImpl(significant_transform),
            GetRenderSurfaceImpl(root));
  EXPECT_TRUE(GetRenderSurfaceImpl(layer_clips_subtree));
  EXPECT_NE(GetRenderSurfaceImpl(render_surface), GetRenderSurfaceImpl(root));
  EXPECT_EQ(GetRenderSurfaceImpl(test_layer),
            GetRenderSurfaceImpl(render_surface));

  EXPECT_EQ(gfx::Rect(30, 20), ImplOf(test_layer)->visible_layer_rect());
}

TEST_F(PropertyTreeBuilderTest, PropertyTreesRebuildWithOpacityChanges) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  child->SetIsDrawable(true);
  root->AddChild(child);
  host()->SetRootLayer(root);

  root->SetBounds(gfx::Size(100, 100));
  child->SetBounds(gfx::Size(20, 20));
  UpdateMainDrawProperties();

  // Changing the opacity from 1 to non-1 value should trigger rebuild of
  // property trees as a new effect node will be created.
  child->SetOpacity(0.5f);
  PropertyTrees* property_trees = host()->property_trees();
  EXPECT_TRUE(property_trees->needs_rebuild);

  UpdateMainDrawProperties();
  EXPECT_NE(child->effect_tree_index(), root->effect_tree_index());

  // child already has an effect node. Changing its opacity shouldn't trigger
  // a property trees rebuild.
  child->SetOpacity(0.8f);
  property_trees = host()->property_trees();
  EXPECT_FALSE(property_trees->needs_rebuild);

  UpdateMainDrawProperties();
  EXPECT_NE(child->effect_tree_index(), root->effect_tree_index());

  // Changing the opacity from non-1 value to 1 should trigger a rebuild of
  // property trees as the effect node may no longer be needed.
  child->SetOpacity(1.f);
  property_trees = host()->property_trees();
  EXPECT_TRUE(property_trees->needs_rebuild);

  UpdateMainDrawProperties();
  EXPECT_EQ(child->effect_tree_index(), root->effect_tree_index());
}

TEST_F(PropertyTreeBuilderTest, RenderSurfaceListForTrilinearFiltering) {
  auto root = Layer::Create();
  host()->SetRootLayer(root);
  auto parent = Layer::Create();
  root->AddChild(parent);
  auto child1 = Layer::Create();
  parent->AddChild(child1);
  auto child2 = Layer::Create();
  parent->AddChild(child2);

  gfx::Transform scale_matrix;
  scale_matrix.Scale(.25f, .25f);

  root->SetBounds(gfx::Size(200, 200));
  parent->SetTransform(scale_matrix);
  parent->SetTrilinearFiltering(true);
  child1->SetBounds(gfx::Size(50, 50));
  child1->SetIsDrawable(true);
  child1->SetForceRenderSurfaceForTesting(true);
  child2->SetPosition(gfx::PointF(50, 50));
  child2->SetBounds(gfx::Size(50, 50));
  child2->SetIsDrawable(true);
  child2->SetForceRenderSurfaceForTesting(true);

  CommitAndActivate();

  ASSERT_TRUE(GetRenderSurfaceImpl(parent));
  EXPECT_EQ(2, GetRenderSurfaceImpl(parent)->num_contributors());
  EXPECT_EQ(4U, GetRenderSurfaceList().size());

  // The rectangle enclosing child1 and child2 (0,0 100x100), scaled by the
  // scale matrix to (0,0 25x25).
  EXPECT_EQ(gfx::RectF(0, 0, 25, 25),
            GetRenderSurfaceImpl(parent)->DrawableContentRect());
}

TEST_F(PropertyTreeBuilderTest, RoundedCornerBounds) {
  // Layer Tree:
  // +root
  // +--render surface
  // +----rounded corner layer 1 [should trigger render surface]
  // +----layer 1
  // +--rounded corner layer 2 [should trigger render surface]
  // +----layer 2
  // +------rounded corner layer 3 [should trigger render surface]
  // +--------rounded corner layer 4 [should trigger render surface]

  constexpr int kRoundedCorner1Radius = 2;
  constexpr int kRoundedCorner2Radius = 5;
  constexpr int kRoundedCorner3Radius = 1;
  constexpr int kRoundedCorner4Radius = 1;

  constexpr gfx::RectF kRoundedCornerLayer1Bound(15.f, 15.f, 20.f, 20.f);
  constexpr gfx::RectF kRoundedCornerLayer2Bound(40.f, 40.f, 60.f, 60.f);
  constexpr gfx::RectF kRoundedCornerLayer3Bound(0.f, 15.f, 5.f, 5.f);
  constexpr gfx::RectF kRoundedCornerLayer4Bound(1.f, 1.f, 3.f, 3.f);

  constexpr float kDeviceScale = 1.6f;

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> render_surface = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_1 = Layer::Create();
  scoped_refptr<Layer> layer_1 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_2 = Layer::Create();
  scoped_refptr<Layer> layer_2 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_3 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_4 = Layer::Create();

  // Set up layer tree
  root->AddChild(render_surface);
  root->AddChild(rounded_corner_layer_2);

  render_surface->AddChild(rounded_corner_layer_1);
  render_surface->AddChild(layer_1);

  rounded_corner_layer_2->AddChild(layer_2);

  layer_2->AddChild(rounded_corner_layer_3);

  rounded_corner_layer_3->AddChild(rounded_corner_layer_4);

  // Set the root layer on host.
  host()->SetRootLayer(root);

  // Set layer positions.
  render_surface->SetPosition(gfx::PointF(0, 0));
  rounded_corner_layer_1->SetPosition(kRoundedCornerLayer1Bound.origin());
  layer_1->SetPosition(gfx::PointF(10.f, 10.f));
  rounded_corner_layer_2->SetPosition(kRoundedCornerLayer2Bound.origin());
  layer_2->SetPosition(gfx::PointF(30.f, 30.f));
  rounded_corner_layer_3->SetPosition(kRoundedCornerLayer3Bound.origin());
  rounded_corner_layer_4->SetPosition(kRoundedCornerLayer4Bound.origin());

  // Set up layer bounds.
  root->SetBounds(gfx::Size(100, 100));
  render_surface->SetBounds(gfx::Size(50, 50));
  rounded_corner_layer_1->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer1Bound.size()));
  layer_1->SetBounds(gfx::Size(10, 10));
  rounded_corner_layer_2->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer2Bound.size()));
  layer_2->SetBounds(gfx::Size(25, 25));
  rounded_corner_layer_3->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer3Bound.size()));
  rounded_corner_layer_4->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer4Bound.size()));

  // Add Layer transforms.
  gfx::Transform layer_2_transform;
  constexpr gfx::Vector2dF kLayer2Translation(10.f, 10.f);
  layer_2_transform.Translate(kLayer2Translation);
  layer_2->SetTransform(layer_2_transform);

  gfx::Transform rounded_corner_layer_3_transform;
  constexpr float kRoundedCorner3Scale = 2.f;
  rounded_corner_layer_3_transform.Scale(kRoundedCorner3Scale,
                                         kRoundedCorner3Scale);
  rounded_corner_layer_3->SetTransform(rounded_corner_layer_3_transform);

  // Set the layer properties
  render_surface->SetForceRenderSurfaceForTesting(true);

  root->SetIsDrawable(true);
  render_surface->SetIsDrawable(true);
  rounded_corner_layer_1->SetIsDrawable(true);
  layer_1->SetIsDrawable(true);
  rounded_corner_layer_2->SetIsDrawable(true);
  layer_2->SetIsDrawable(true);
  rounded_corner_layer_3->SetIsDrawable(true);
  rounded_corner_layer_4->SetIsDrawable(true);

  // Set Rounded corners
  rounded_corner_layer_1->SetRoundedCorner(
      {kRoundedCorner1Radius, kRoundedCorner1Radius, kRoundedCorner1Radius,
       kRoundedCorner1Radius});
  rounded_corner_layer_2->SetRoundedCorner(
      {kRoundedCorner2Radius, kRoundedCorner2Radius, kRoundedCorner2Radius,
       kRoundedCorner2Radius});
  rounded_corner_layer_3->SetRoundedCorner(
      {kRoundedCorner3Radius, kRoundedCorner3Radius, kRoundedCorner3Radius,
       kRoundedCorner3Radius});
  rounded_corner_layer_4->SetRoundedCorner(
      {kRoundedCorner4Radius, kRoundedCorner4Radius, kRoundedCorner4Radius,
       kRoundedCorner4Radius});

  UpdateMainDrawProperties(kDeviceScale);

  // Since this effect node has no descendants that draw and no descendant that
  // has a rounded corner, it does not need a render surface.
  const EffectNode* effect_node = GetEffectNode(rounded_corner_layer_1.get());
  gfx::RRectF rounded_corner_bounds_1 = effect_node->rounded_corner_bounds;
  EXPECT_FALSE(effect_node->HasRenderSurface());
  EXPECT_FLOAT_EQ(rounded_corner_bounds_1.GetSimpleRadius(),
                  kRoundedCorner1Radius);
  EXPECT_EQ(rounded_corner_bounds_1.rect(),
            gfx::RectF(kRoundedCornerLayer1Bound.size()));

  // Since this node has descendants with roudned corners, it needs a render
  // surface. It also has 2 descendants that draw.
  effect_node = GetEffectNode(rounded_corner_layer_2.get());
  gfx::RRectF rounded_corner_bounds_2 = effect_node->rounded_corner_bounds;
  EXPECT_TRUE(effect_node->HasRenderSurface());
  EXPECT_FLOAT_EQ(rounded_corner_bounds_2.GetSimpleRadius(),
                  kRoundedCorner2Radius);
  EXPECT_EQ(rounded_corner_bounds_2.rect(),
            gfx::RectF(kRoundedCornerLayer2Bound.size()));

  // Since this node has a descendant that has a rounded corner, it will trigger
  // the creation of a render surface.
  effect_node = GetEffectNode(rounded_corner_layer_3.get());
  gfx::RRectF rounded_corner_bounds_3 = effect_node->rounded_corner_bounds;
  EXPECT_TRUE(effect_node->HasRenderSurface());
  EXPECT_FLOAT_EQ(rounded_corner_bounds_3.GetSimpleRadius(),
                  kRoundedCorner3Radius);
  EXPECT_EQ(rounded_corner_bounds_3.rect(),
            gfx::RectF(kRoundedCornerLayer3Bound.size()));

  // Since this node has no descendants that draw nor any descendant that has a
  // rounded corner, it does not need a render surface.
  effect_node = GetEffectNode(rounded_corner_layer_4.get());
  gfx::RRectF rounded_corner_bounds_4 = effect_node->rounded_corner_bounds;
  EXPECT_FALSE(effect_node->HasRenderSurface());
  EXPECT_FLOAT_EQ(rounded_corner_bounds_4.GetSimpleRadius(),
                  kRoundedCorner4Radius);
  EXPECT_EQ(rounded_corner_bounds_4.rect(),
            gfx::RectF(kRoundedCornerLayer4Bound.size()));

  CommitAndActivate(kDeviceScale);
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  // Get the layer impl for each Layer.
  LayerImpl* rounded_corner_layer_1_impl =
      layer_tree_impl->LayerById(rounded_corner_layer_1->id());
  LayerImpl* rounded_corner_layer_2_impl =
      layer_tree_impl->LayerById(rounded_corner_layer_2->id());
  LayerImpl* rounded_corner_layer_3_impl =
      layer_tree_impl->LayerById(rounded_corner_layer_3->id());
  LayerImpl* rounded_corner_layer_4_impl =
      layer_tree_impl->LayerById(rounded_corner_layer_4->id());

  EXPECT_EQ(kDeviceScale, layer_tree_impl->device_scale_factor());

  // Rounded corner layer 1
  // The render target for this layer is |render_surface|, hence its target
  // bounds are relative to |render_surface|.
  // The offset from the origin of the render target is [15, 15] and the device
  // scale factor is 1.6 thus giving the target space origin of [24, 24]. The
  // corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_rrect_1 =
      rounded_corner_layer_1_impl->draw_properties().rounded_corner_bounds;
  gfx::RectF bounds_in_target_space = kRoundedCornerLayer1Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  EXPECT_EQ(actual_rrect_1.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_rrect_1.GetSimpleRadius(),
                  kRoundedCorner1Radius * kDeviceScale);

  // Rounded corner layer 2
  // The render target for this layer is |root|.
  // The offset from the origin of the render target is [40, 40] and the device
  // scale factor is 1.6 thus giving the target space origin of [64, 64]. The
  // corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_self_rrect_2 =
      rounded_corner_layer_2_impl->draw_properties().rounded_corner_bounds;
  EXPECT_TRUE(actual_self_rrect_2.IsEmpty());

  bounds_in_target_space = kRoundedCornerLayer2Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  const gfx::RRectF actual_render_target_rrect_2 =
      rounded_corner_layer_2_impl->render_target()->rounded_corner_bounds();
  EXPECT_EQ(actual_render_target_rrect_2.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_render_target_rrect_2.GetSimpleRadius(),
                  kRoundedCorner2Radius * kDeviceScale);

  // Rounded corner layer 3
  // The render target for this layer is |rounded_corner_2|.
  // The net offset from the origin of the render target is [40, 55] and the
  // device scale factor is 1.6 thus giving the target space origin of [64, 88].
  // The corner radius is also scaled by a factor of 1.6 * transform scale.
  const gfx::RRectF actual_self_rrect_3 =
      rounded_corner_layer_3_impl->draw_properties().rounded_corner_bounds;
  EXPECT_TRUE(actual_self_rrect_3.IsEmpty());

  bounds_in_target_space = kRoundedCornerLayer3Bound;
  bounds_in_target_space +=
      layer_2->position().OffsetFromOrigin() + kLayer2Translation;
  bounds_in_target_space.Scale(kDeviceScale);
  gfx::SizeF transformed_size = bounds_in_target_space.size();
  transformed_size.Scale(kRoundedCorner3Scale);
  bounds_in_target_space.set_size(transformed_size);

  const gfx::RRectF actual_render_target_rrect_3 =
      rounded_corner_layer_3_impl->render_target()->rounded_corner_bounds();
  EXPECT_EQ(actual_render_target_rrect_3.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_render_target_rrect_3.GetSimpleRadius(),
                  kRoundedCorner3Radius * kDeviceScale * kRoundedCorner3Scale);

  // Rounded corner layer 4
  // The render target for this layer is |rounded_corner_3|.
  // The net offset from the origin of the render target is [1, 1] and the
  // net scale is 1.6 * transform scale = 3.2 thus giving the target space o
  // rigin of [3.2, 3.2].
  // The corner radius is also scaled by a factor of 3.2.
  const gfx::RRectF actual_rrect_4 =
      rounded_corner_layer_4_impl->draw_properties().rounded_corner_bounds;
  bounds_in_target_space = kRoundedCornerLayer4Bound;
  bounds_in_target_space.Scale(kDeviceScale * kRoundedCorner3Scale);
  EXPECT_EQ(actual_rrect_4.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_rrect_4.GetSimpleRadius(),
                  kRoundedCorner4Radius * kDeviceScale * kRoundedCorner3Scale);
}

TEST_F(PropertyTreeBuilderTest, RoundedCornerBoundsInterveningRenderTarget) {
  // Layer Tree:
  // +root
  // +--rounded corner layer 1 [should not trigger render surface]
  // +----render surface [Does not draw]
  // +------rounded corner layer 2 [should not trigger render surface]

  constexpr int kRoundedCorner1Radius = 2;
  constexpr int kRoundedCorner2Radius = 5;

  constexpr gfx::RectF kRoundedCornerLayer1Bound(60.f, 0.f, 40.f, 30.f);
  constexpr gfx::RectF kRoundedCornerLayer2Bound(0.f, 0.f, 30.f, 20.f);

  constexpr float kDeviceScale = 1.6f;

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> render_surface = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_1 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_2 = Layer::Create();

  // Set up layer tree
  root->AddChild(rounded_corner_layer_1);
  rounded_corner_layer_1->AddChild(render_surface);
  render_surface->AddChild(rounded_corner_layer_2);

  // Set the root layer on host.
  host()->SetRootLayer(root);

  // Set layer positions.
  rounded_corner_layer_1->SetPosition(kRoundedCornerLayer1Bound.origin());
  render_surface->SetPosition(gfx::PointF(0, 0));
  rounded_corner_layer_2->SetPosition(kRoundedCornerLayer2Bound.origin());

  // Set up layer bounds.
  root->SetBounds(gfx::Size(100, 100));
  rounded_corner_layer_1->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer1Bound.size()));
  render_surface->SetBounds(gfx::Size(30, 30));
  rounded_corner_layer_2->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer2Bound.size()));

  // Set the layer properties
  render_surface->SetForceRenderSurfaceForTesting(true);

  root->SetIsDrawable(true);
  rounded_corner_layer_1->SetIsDrawable(true);
  rounded_corner_layer_2->SetIsDrawable(true);

  // Set Rounded corners
  rounded_corner_layer_1->SetRoundedCorner(
      {kRoundedCorner1Radius, kRoundedCorner1Radius, kRoundedCorner1Radius,
       kRoundedCorner1Radius});
  rounded_corner_layer_2->SetRoundedCorner(
      {kRoundedCorner2Radius, kRoundedCorner2Radius, kRoundedCorner2Radius,
       kRoundedCorner2Radius});

  UpdateMainDrawProperties(kDeviceScale);

  // Since this effect node has only 1 descendant that draws and no descendant
  // that has a rounded corner before the render surface, it does not need a
  // render surface.
  const EffectNode* effect_node = GetEffectNode(rounded_corner_layer_1.get());
  gfx::RRectF rounded_corner_bounds_1 = effect_node->rounded_corner_bounds;
  EXPECT_FALSE(effect_node->HasRenderSurface());
  EXPECT_FLOAT_EQ(rounded_corner_bounds_1.GetSimpleRadius(),
                  kRoundedCorner1Radius);
  EXPECT_EQ(rounded_corner_bounds_1.rect(),
            gfx::RectF(kRoundedCornerLayer1Bound.size()));

  // Since this effect node has no descendants that draw and no descendant that
  // has a rounded corner, it does not need a render surface.
  effect_node = GetEffectNode(rounded_corner_layer_2.get());
  gfx::RRectF rounded_corner_bounds_2 = effect_node->rounded_corner_bounds;
  EXPECT_FALSE(effect_node->HasRenderSurface());
  EXPECT_FLOAT_EQ(rounded_corner_bounds_2.GetSimpleRadius(),
                  kRoundedCorner2Radius);
  EXPECT_EQ(rounded_corner_bounds_2.rect(),
            gfx::RectF(kRoundedCornerLayer2Bound.size()));

  CommitAndActivate(kDeviceScale);
  LayerTreeImpl* layer_tree_impl = host_impl()->active_tree();

  // Get the layer impl for each Layer.
  LayerImpl* rounded_corner_layer_1_impl =
      layer_tree_impl->LayerById(rounded_corner_layer_1->id());
  LayerImpl* rounded_corner_layer_2_impl =
      layer_tree_impl->LayerById(rounded_corner_layer_2->id());

  EXPECT_EQ(kDeviceScale, layer_tree_impl->device_scale_factor());

  // Rounded corner layer 1
  // The render target for this layer is |root|, hence its target
  // bounds are relative to |root|.
  // The offset from the origin of the render target is [60, 0] and the device
  // scale factor is 1.6 thus giving the target space origin of [96, 0]. The
  // corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_rrect_1 =
      rounded_corner_layer_1_impl->draw_properties().rounded_corner_bounds;
  gfx::RectF bounds_in_target_space = kRoundedCornerLayer1Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  EXPECT_EQ(actual_rrect_1.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_rrect_1.GetSimpleRadius(),
                  kRoundedCorner1Radius * kDeviceScale);

  // Rounded corner layer 2
  // The render target for this layer is |render_surface|.
  // The offset from the origin of the render target is [0, 0].
  const gfx::RRectF actual_rrect_2 =
      rounded_corner_layer_2_impl->draw_properties().rounded_corner_bounds;
  bounds_in_target_space = kRoundedCornerLayer2Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  EXPECT_EQ(actual_rrect_2.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_rrect_2.GetSimpleRadius(),
                  kRoundedCorner2Radius * kDeviceScale);
}

TEST_F(PropertyTreeBuilderTest, RoundedCornerBoundsSiblingRenderTarget) {
  // Layer Tree:
  // +root
  // +--rounded corner layer 1 [should trigger render surface]
  // +----render surface [Does not draw]
  // +----rounded corner layer 2 [should not trigger render surface]

  constexpr int kRoundedCorner1Radius = 2;
  constexpr int kRoundedCorner2Radius = 5;

  constexpr gfx::RectF kRoundedCornerLayer1Bound(0.f, 60.f, 30.f, 40.f);
  constexpr gfx::RectF kRoundedCornerLayer2Bound(0.f, 0.f, 20.f, 30.f);

  constexpr float kDeviceScale = 1.6f;

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> render_surface = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_1 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_2 = Layer::Create();

  // Set up layer tree
  root->AddChild(rounded_corner_layer_1);
  rounded_corner_layer_1->AddChild(render_surface);
  rounded_corner_layer_1->AddChild(rounded_corner_layer_2);

  // Set the root layer on host.
  host()->SetRootLayer(root);

  // Set layer positions.
  rounded_corner_layer_1->SetPosition(kRoundedCornerLayer1Bound.origin());
  render_surface->SetPosition(gfx::PointF(0, 0));
  rounded_corner_layer_2->SetPosition(kRoundedCornerLayer2Bound.origin());

  // Set up layer bounds.
  root->SetBounds(gfx::Size(100, 100));
  rounded_corner_layer_1->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer1Bound.size()));
  render_surface->SetBounds(gfx::Size(30, 30));
  rounded_corner_layer_2->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer2Bound.size()));

  // Set the layer properties
  render_surface->SetForceRenderSurfaceForTesting(true);

  root->SetIsDrawable(true);
  rounded_corner_layer_1->SetIsDrawable(true);
  rounded_corner_layer_2->SetIsDrawable(true);

  // Set Rounded corners
  rounded_corner_layer_1->SetRoundedCorner(
      {kRoundedCorner1Radius, kRoundedCorner1Radius, kRoundedCorner1Radius,
       kRoundedCorner1Radius});
  rounded_corner_layer_2->SetRoundedCorner(
      {kRoundedCorner2Radius, kRoundedCorner2Radius, kRoundedCorner2Radius,
       kRoundedCorner2Radius});

  UpdateMainDrawProperties(kDeviceScale);

  // Since this effect node has 1 descendant with a rounded corner without a
  // render surface along the chain, it need a render surface.
  const EffectNode* effect_node = GetEffectNode(rounded_corner_layer_1.get());
  gfx::RRectF rounded_corner_bounds_1 = effect_node->rounded_corner_bounds;
  EXPECT_TRUE(effect_node->HasRenderSurface());
  EXPECT_FLOAT_EQ(rounded_corner_bounds_1.GetSimpleRadius(),
                  kRoundedCorner1Radius);
  EXPECT_EQ(rounded_corner_bounds_1.rect(),
            gfx::RectF(kRoundedCornerLayer1Bound.size()));

  // Since this effect node has no descendants that draw and no descendant that
  // has a rounded corner, it does not need a render surface.
  effect_node = GetEffectNode(rounded_corner_layer_2.get());
  gfx::RRectF rounded_corner_bounds_2 = effect_node->rounded_corner_bounds;
  EXPECT_FALSE(effect_node->HasRenderSurface());
  EXPECT_FLOAT_EQ(rounded_corner_bounds_2.GetSimpleRadius(),
                  kRoundedCorner2Radius);
  EXPECT_EQ(rounded_corner_bounds_2.rect(),
            gfx::RectF(kRoundedCornerLayer2Bound.size()));

  CommitAndActivate(kDeviceScale);
  LayerTreeImpl* layer_tree_impl = host_impl()->active_tree();

  // Get the layer impl for each Layer.
  LayerImpl* rounded_corner_layer_1_impl =
      layer_tree_impl->LayerById(rounded_corner_layer_1->id());
  LayerImpl* rounded_corner_layer_2_impl =
      layer_tree_impl->LayerById(rounded_corner_layer_2->id());

  EXPECT_EQ(kDeviceScale, layer_tree_impl->device_scale_factor());

  // Rounded corner layer 1
  // The render target for this layer is |root|, hence its target
  // bounds are relative to |root|.
  // The offset from the origin of the render target is [0, 60] and the device
  // scale factor is 1.6 thus giving the target space origin of [0, 96]. The
  // corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_self_rrect_1 =
      rounded_corner_layer_1_impl->draw_properties().rounded_corner_bounds;
  EXPECT_TRUE(actual_self_rrect_1.IsEmpty());

  gfx::RectF bounds_in_target_space = kRoundedCornerLayer1Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  const gfx::RRectF actual_render_target_rrect_1 =
      rounded_corner_layer_1_impl->render_target()->rounded_corner_bounds();
  EXPECT_EQ(actual_render_target_rrect_1.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_render_target_rrect_1.GetSimpleRadius(),
                  kRoundedCorner1Radius * kDeviceScale);

  // Rounded corner layer 2
  // The render target for this layer is |render_surface|.
  // The offset from the origin of the render target is [0, 0].
  const gfx::RRectF actual_rrect_2 =
      rounded_corner_layer_2_impl->draw_properties().rounded_corner_bounds;
  bounds_in_target_space = kRoundedCornerLayer2Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  EXPECT_EQ(actual_rrect_2.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_rrect_2.GetSimpleRadius(),
                  kRoundedCorner2Radius * kDeviceScale);
}

TEST_F(PropertyTreeBuilderTest, FastRoundedCornerDoesNotTriggerRenderSurface) {
  // Layer Tree:
  // +root
  // +--fast rounded corner layer [should not trigger render surface]
  // +----layer 1
  // +----layer 2
  // +--rounded corner layer [should trigger render surface]
  // +----layer 3
  // +----layer 4

  constexpr int kRoundedCorner1Radius = 2;
  constexpr int kRoundedCorner2Radius = 5;

  constexpr gfx::RectF kRoundedCornerLayer1Bound(0.f, 0.f, 50.f, 50.f);
  constexpr gfx::RectF kRoundedCornerLayer2Bound(40.f, 40.f, 60.f, 60.f);

  constexpr float kDeviceScale = 1.6f;

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> fast_rounded_corner_layer = Layer::Create();
  scoped_refptr<Layer> layer_1 = Layer::Create();
  scoped_refptr<Layer> layer_2 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer = Layer::Create();
  scoped_refptr<Layer> layer_3 = Layer::Create();
  scoped_refptr<Layer> layer_4 = Layer::Create();

  // Set up layer tree
  root->AddChild(fast_rounded_corner_layer);
  root->AddChild(rounded_corner_layer);

  fast_rounded_corner_layer->AddChild(layer_1);
  fast_rounded_corner_layer->AddChild(layer_2);

  rounded_corner_layer->AddChild(layer_3);
  rounded_corner_layer->AddChild(layer_4);

  // Set the root layer on host.
  host()->SetRootLayer(root);

  // Set layer positions.
  fast_rounded_corner_layer->SetPosition(kRoundedCornerLayer1Bound.origin());
  layer_1->SetPosition(gfx::PointF(0.f, 0.f));
  layer_2->SetPosition(gfx::PointF(25.f, 0.f));
  rounded_corner_layer->SetPosition(kRoundedCornerLayer2Bound.origin());
  layer_3->SetPosition(gfx::PointF(0.f, 0.f));
  layer_4->SetPosition(gfx::PointF(30.f, 0.f));

  // Set up layer bounds.
  root->SetBounds(gfx::Size(100, 100));
  fast_rounded_corner_layer->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer1Bound.size()));
  layer_1->SetBounds(gfx::Size(25, 25));
  layer_2->SetBounds(gfx::Size(25, 25));
  rounded_corner_layer->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer2Bound.size()));
  layer_3->SetBounds(gfx::Size(30, 60));
  layer_4->SetBounds(gfx::Size(30, 60));

  root->SetIsDrawable(true);
  fast_rounded_corner_layer->SetIsDrawable(true);
  layer_1->SetIsDrawable(true);
  layer_2->SetIsDrawable(true);
  rounded_corner_layer->SetIsDrawable(true);
  layer_3->SetIsDrawable(true);
  layer_4->SetIsDrawable(true);

  // Set Rounded corners
  fast_rounded_corner_layer->SetRoundedCorner(
      {kRoundedCorner1Radius, kRoundedCorner1Radius, kRoundedCorner1Radius,
       kRoundedCorner1Radius});
  rounded_corner_layer->SetRoundedCorner(
      {kRoundedCorner2Radius, kRoundedCorner2Radius, kRoundedCorner2Radius,
       kRoundedCorner2Radius});

  fast_rounded_corner_layer->SetIsFastRoundedCorner(true);

  UpdateMainDrawProperties(kDeviceScale);

  // Since this layer has a fast rounded corner, it should not have a render
  // surface even though it has 2 layers in the subtree that draws content.
  const EffectNode* effect_node =
      GetEffectNode(fast_rounded_corner_layer.get());
  gfx::RRectF rounded_corner_bounds_1 = effect_node->rounded_corner_bounds;
  EXPECT_FALSE(effect_node->HasRenderSurface());
  EXPECT_TRUE(effect_node->is_fast_rounded_corner);
  EXPECT_FLOAT_EQ(rounded_corner_bounds_1.GetSimpleRadius(),
                  kRoundedCorner1Radius);
  EXPECT_EQ(rounded_corner_bounds_1.rect(),
            gfx::RectF(kRoundedCornerLayer1Bound.size()));

  // Since this node has 2 descendants that draw, it will have a rounded corner.
  effect_node = GetEffectNode(rounded_corner_layer.get());
  gfx::RRectF rounded_corner_bounds_2 = effect_node->rounded_corner_bounds;
  EXPECT_TRUE(effect_node->HasRenderSurface());
  EXPECT_FALSE(effect_node->is_fast_rounded_corner);
  EXPECT_FLOAT_EQ(rounded_corner_bounds_2.GetSimpleRadius(),
                  kRoundedCorner2Radius);
  EXPECT_EQ(rounded_corner_bounds_2.rect(),
            gfx::RectF(kRoundedCornerLayer2Bound.size()));

  CommitAndActivate(kDeviceScale);
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  // Get the layer impl for each Layer.
  LayerImpl* fast_rounded_corner_layer_impl =
      layer_tree_impl->LayerById(fast_rounded_corner_layer->id());
  LayerImpl* layer_1_impl = layer_tree_impl->LayerById(layer_1->id());
  LayerImpl* layer_2_impl = layer_tree_impl->LayerById(layer_2->id());
  LayerImpl* rounded_corner_layer_impl =
      layer_tree_impl->LayerById(rounded_corner_layer->id());
  LayerImpl* layer_3_impl = layer_tree_impl->LayerById(layer_3->id());
  LayerImpl* layer_4_impl = layer_tree_impl->LayerById(layer_4->id());

  EXPECT_EQ(kDeviceScale, layer_tree_impl->device_scale_factor());

  // Fast rounded corner layer.
  // The render target for this layer is |root|, hence its target bounds are
  // relative to |root|.
  // The offset from the origin of the render target is [0, 0] and the device
  // scale factor is 1.6.
  const gfx::RRectF actual_rrect_1 =
      fast_rounded_corner_layer_impl->draw_properties().rounded_corner_bounds;
  gfx::RectF bounds_in_target_space = kRoundedCornerLayer1Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  EXPECT_EQ(actual_rrect_1.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_rrect_1.GetSimpleRadius(),
                  kRoundedCorner1Radius * kDeviceScale);

  // Layer 1 and layer 2 rounded corner bounds
  // This should have the same rounded corner boudns as fast rounded corner
  // layer.
  const gfx::RRectF layer_1_rrect =
      layer_1_impl->draw_properties().rounded_corner_bounds;
  const gfx::RRectF layer_2_rrect =
      layer_2_impl->draw_properties().rounded_corner_bounds;
  EXPECT_EQ(actual_rrect_1, layer_1_rrect);
  EXPECT_EQ(actual_rrect_1, layer_2_rrect);

  // Rounded corner layer
  // The render target for this layer is |root|.
  // The offset from the origin of the render target is [40, 40] and the device
  // scale factor is 1.6 thus giving the target space origin of [64, 64]. The
  // corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_self_rrect_2 =
      rounded_corner_layer_impl->draw_properties().rounded_corner_bounds;
  EXPECT_TRUE(actual_self_rrect_2.IsEmpty());

  bounds_in_target_space = kRoundedCornerLayer2Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  const gfx::RRectF actual_render_target_rrect_2 =
      rounded_corner_layer_impl->render_target()->rounded_corner_bounds();
  EXPECT_EQ(actual_render_target_rrect_2.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_render_target_rrect_2.GetSimpleRadius(),
                  kRoundedCorner2Radius * kDeviceScale);

  // Layer 3 and layer 4 should have no rounded corner bounds set as their
  // parent is a render surface.
  const gfx::RRectF layer_3_rrect =
      layer_3_impl->draw_properties().rounded_corner_bounds;
  const gfx::RRectF layer_4_rrect =
      layer_4_impl->draw_properties().rounded_corner_bounds;
  EXPECT_TRUE(layer_3_rrect.IsEmpty());
  EXPECT_TRUE(layer_4_rrect.IsEmpty());
}

TEST_F(PropertyTreeBuilderTest,
       FastRoundedCornerTriggersRenderSurfaceInAncestor) {
  // Layer Tree:
  // +root
  // +--rounded corner layer [1] [should trigger render surface]
  // +----fast rounded corner layer [2] [should not trigger render surface]
  // +--rounded corner layer [3] [should trigger render surface]
  // +----rounded corner layer [4] [should not trigger render surface]

  constexpr int kRoundedCorner1Radius = 2;
  constexpr int kRoundedCorner2Radius = 5;
  constexpr int kRoundedCorner3Radius = 1;
  constexpr int kRoundedCorner4Radius = 3;

  constexpr gfx::RectF kRoundedCornerLayer1Bound(5.f, 5.f, 50.f, 50.f);
  constexpr gfx::RectF kRoundedCornerLayer2Bound(0.f, 0.f, 25.f, 25.f);
  constexpr gfx::RectF kRoundedCornerLayer3Bound(40.f, 40.f, 60.f, 60.f);
  constexpr gfx::RectF kRoundedCornerLayer4Bound(30.f, 0.f, 30.f, 60.f);

  constexpr float kDeviceScale = 1.6f;

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_1 = Layer::Create();
  scoped_refptr<Layer> fast_rounded_corner_layer_2 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_3 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_4 = Layer::Create();

  // Set up layer tree
  root->AddChild(rounded_corner_layer_1);
  root->AddChild(rounded_corner_layer_3);

  rounded_corner_layer_1->AddChild(fast_rounded_corner_layer_2);

  rounded_corner_layer_3->AddChild(rounded_corner_layer_4);

  // Set the root layer on host.
  host()->SetRootLayer(root);

  // Set layer positions.
  rounded_corner_layer_1->SetPosition(kRoundedCornerLayer1Bound.origin());
  fast_rounded_corner_layer_2->SetPosition(kRoundedCornerLayer2Bound.origin());
  rounded_corner_layer_3->SetPosition(kRoundedCornerLayer3Bound.origin());
  rounded_corner_layer_4->SetPosition(kRoundedCornerLayer4Bound.origin());

  // Set up layer bounds.
  root->SetBounds(gfx::Size(100, 100));
  rounded_corner_layer_1->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer1Bound.size()));
  fast_rounded_corner_layer_2->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer2Bound.size()));
  rounded_corner_layer_3->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer3Bound.size()));
  rounded_corner_layer_4->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer4Bound.size()));

  root->SetIsDrawable(true);
  rounded_corner_layer_1->SetIsDrawable(true);
  fast_rounded_corner_layer_2->SetIsDrawable(true);
  rounded_corner_layer_3->SetIsDrawable(true);
  rounded_corner_layer_4->SetIsDrawable(true);

  // Set Rounded corners
  rounded_corner_layer_1->SetRoundedCorner(
      {kRoundedCorner1Radius, kRoundedCorner1Radius, kRoundedCorner1Radius,
       kRoundedCorner1Radius});
  fast_rounded_corner_layer_2->SetRoundedCorner(
      {kRoundedCorner2Radius, kRoundedCorner2Radius, kRoundedCorner2Radius,
       kRoundedCorner2Radius});
  rounded_corner_layer_3->SetRoundedCorner(
      {kRoundedCorner3Radius, kRoundedCorner3Radius, kRoundedCorner3Radius,
       kRoundedCorner3Radius});
  rounded_corner_layer_4->SetRoundedCorner(
      {kRoundedCorner4Radius, kRoundedCorner4Radius, kRoundedCorner4Radius,
       kRoundedCorner4Radius});

  fast_rounded_corner_layer_2->SetIsFastRoundedCorner(true);

  UpdateMainDrawProperties(kDeviceScale);

  // Since this layer has a descendant that has rounded corner, this node will
  // require a render surface.
  const EffectNode* effect_node = GetEffectNode(rounded_corner_layer_1.get());
  gfx::RRectF rounded_corner_bounds_1 = effect_node->rounded_corner_bounds;
  EXPECT_TRUE(effect_node->HasRenderSurface());
  EXPECT_FALSE(effect_node->is_fast_rounded_corner);
  EXPECT_FLOAT_EQ(rounded_corner_bounds_1.GetSimpleRadius(),
                  kRoundedCorner1Radius);
  EXPECT_EQ(rounded_corner_bounds_1.rect(),
            gfx::RectF(kRoundedCornerLayer1Bound.size()));

  // Since this layer has no descendant with rounded corner or drawable, it will
  // not have a render surface.
  effect_node = GetEffectNode(fast_rounded_corner_layer_2.get());
  gfx::RRectF rounded_corner_bounds_2 = effect_node->rounded_corner_bounds;
  EXPECT_FALSE(effect_node->HasRenderSurface());
  EXPECT_TRUE(effect_node->is_fast_rounded_corner);
  EXPECT_FLOAT_EQ(rounded_corner_bounds_2.GetSimpleRadius(),
                  kRoundedCorner2Radius);
  EXPECT_EQ(rounded_corner_bounds_2.rect(),
            gfx::RectF(kRoundedCornerLayer2Bound.size()));

  // Since this layer has 1 descendant with a rounded corner, it should have a
  // render surface.
  effect_node = GetEffectNode(rounded_corner_layer_3.get());
  gfx::RRectF rounded_corner_bounds_3 = effect_node->rounded_corner_bounds;
  EXPECT_TRUE(effect_node->HasRenderSurface());
  EXPECT_FALSE(effect_node->is_fast_rounded_corner);
  EXPECT_FLOAT_EQ(rounded_corner_bounds_3.GetSimpleRadius(),
                  kRoundedCorner3Radius);
  EXPECT_EQ(rounded_corner_bounds_3.rect(),
            gfx::RectF(kRoundedCornerLayer3Bound.size()));

  // Since this layer no descendants, it would no thave a render pass.
  effect_node = GetEffectNode(rounded_corner_layer_4.get());
  gfx::RRectF rounded_corner_bounds_4 = effect_node->rounded_corner_bounds;
  EXPECT_FALSE(effect_node->HasRenderSurface());
  EXPECT_FALSE(effect_node->is_fast_rounded_corner);
  EXPECT_FLOAT_EQ(rounded_corner_bounds_4.GetSimpleRadius(),
                  kRoundedCorner4Radius);
  EXPECT_EQ(rounded_corner_bounds_4.rect(),
            gfx::RectF(kRoundedCornerLayer4Bound.size()));

  CommitAndActivate(kDeviceScale);
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  // Get the layer impl for each Layer.
  LayerImpl* rounded_corner_layer_impl_1 =
      layer_tree_impl->LayerById(rounded_corner_layer_1->id());
  LayerImpl* fast_rounded_corner_layer_impl_2 =
      layer_tree_impl->LayerById(fast_rounded_corner_layer_2->id());
  LayerImpl* rounded_corner_layer_impl_3 =
      layer_tree_impl->LayerById(rounded_corner_layer_3->id());
  LayerImpl* rounded_corner_layer_impl_4 =
      layer_tree_impl->LayerById(rounded_corner_layer_4->id());

  EXPECT_EQ(kDeviceScale, layer_tree_impl->device_scale_factor());

  // Rounded corner layer 1.
  // The render target for this layer is |root|, hence its target bounds are
  // relative to |root|.
  // The offset from the origin of the render target is [5, 5] and the device
  // scale factor is 1.6 giving a total offset of [8, 8].
  const gfx::RRectF actual_self_rrect_1 =
      rounded_corner_layer_impl_1->draw_properties().rounded_corner_bounds;
  EXPECT_TRUE(actual_self_rrect_1.IsEmpty());

  gfx::RectF bounds_in_target_space = kRoundedCornerLayer1Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  const gfx::RRectF actual_render_target_rrect_1 =
      rounded_corner_layer_impl_1->render_target()->rounded_corner_bounds();
  EXPECT_EQ(actual_render_target_rrect_1.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_render_target_rrect_1.GetSimpleRadius(),
                  kRoundedCorner1Radius * kDeviceScale);

  // Fast rounded corner layer 2
  // The render target for this layer is |rounded_corner_layer_1|.
  // The offset from the origin of the render target is [0, 0] and the device
  // scale factor is 1.6. The corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_self_rrect_2 =
      fast_rounded_corner_layer_impl_2->draw_properties().rounded_corner_bounds;
  bounds_in_target_space = kRoundedCornerLayer2Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  EXPECT_EQ(actual_self_rrect_2.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_self_rrect_2.GetSimpleRadius(),
                  kRoundedCorner2Radius * kDeviceScale);

  // Rounded corner layer 3
  // The render target for this layer is |root|.
  // The offset from the origin of the render target is [40, 40] and the device
  // scale factor is 1.6 thus giving the target space origin of [64, 64]. The
  // corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_self_rrect_3 =
      rounded_corner_layer_impl_3->draw_properties().rounded_corner_bounds;
  EXPECT_TRUE(actual_self_rrect_3.IsEmpty());

  bounds_in_target_space = kRoundedCornerLayer3Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  const gfx::RRectF actual_render_target_rrect_3 =
      rounded_corner_layer_impl_3->render_target()->rounded_corner_bounds();
  EXPECT_EQ(actual_render_target_rrect_3.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_render_target_rrect_3.GetSimpleRadius(),
                  kRoundedCorner3Radius * kDeviceScale);

  // Rounded corner layer 4
  // The render target for this layer is |rounded_corner_layer_3|.
  // The offset from the origin of the render target is [30, 0] and the device
  // scale factor is 1.6 thus giving the target space origin of [48, 0]. The
  // corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_self_rrect_4 =
      rounded_corner_layer_impl_4->draw_properties().rounded_corner_bounds;
  bounds_in_target_space = kRoundedCornerLayer4Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  EXPECT_EQ(actual_self_rrect_4.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_self_rrect_4.GetSimpleRadius(),
                  kRoundedCorner4Radius * kDeviceScale);
}

TEST_F(PropertyTreeBuilderTest,
       FastRoundedCornerDoesNotTriggerRenderSurfaceFromSubtree) {
  // Layer Tree:
  // +root
  // +--fast rounded corner layer 1 [should trigger render surface]
  // +----rounded corner layer 1 [should not trigger render surface]
  // +--rounded corner layer 2 [should trigger render surface]
  // +----rounded corner layer 3 [should not trigger render surface]
  constexpr int kRoundedCorner1Radius = 2;
  constexpr int kRoundedCorner2Radius = 5;
  constexpr int kRoundedCorner3Radius = 4;
  constexpr int kRoundedCorner4Radius = 5;

  constexpr gfx::RectF kRoundedCornerLayer1Bound(10.f, 5.f, 45.f, 50.f);
  constexpr gfx::RectF kRoundedCornerLayer2Bound(5.f, 5.f, 20.f, 20.f);
  constexpr gfx::RectF kRoundedCornerLayer3Bound(60.f, 5.f, 40.f, 25.f);
  constexpr gfx::RectF kRoundedCornerLayer4Bound(0.f, 10.f, 10.f, 20.f);

  constexpr float kDeviceScale = 1.6f;

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> fast_rounded_corner_layer_1 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_1 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_2 = Layer::Create();
  scoped_refptr<Layer> rounded_corner_layer_3 = Layer::Create();

  // Set up layer tree
  root->AddChild(fast_rounded_corner_layer_1);
  root->AddChild(rounded_corner_layer_2);

  fast_rounded_corner_layer_1->AddChild(rounded_corner_layer_1);
  rounded_corner_layer_2->AddChild(rounded_corner_layer_3);

  // Set the root layer on host.
  host()->SetRootLayer(root);

  // Set layer positions.
  fast_rounded_corner_layer_1->SetPosition(kRoundedCornerLayer1Bound.origin());
  rounded_corner_layer_1->SetPosition(kRoundedCornerLayer2Bound.origin());
  rounded_corner_layer_2->SetPosition(kRoundedCornerLayer3Bound.origin());
  rounded_corner_layer_3->SetPosition(kRoundedCornerLayer4Bound.origin());

  // Set up layer bounds.
  root->SetBounds(gfx::Size(100, 100));
  fast_rounded_corner_layer_1->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer1Bound.size()));
  rounded_corner_layer_1->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer2Bound.size()));
  rounded_corner_layer_2->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer3Bound.size()));
  rounded_corner_layer_3->SetBounds(
      gfx::ToRoundedSize(kRoundedCornerLayer4Bound.size()));

  root->SetIsDrawable(true);
  fast_rounded_corner_layer_1->SetIsDrawable(true);
  rounded_corner_layer_1->SetIsDrawable(true);
  rounded_corner_layer_2->SetIsDrawable(true);
  rounded_corner_layer_3->SetIsDrawable(true);

  // Set Rounded corners
  fast_rounded_corner_layer_1->SetRoundedCorner(
      {kRoundedCorner1Radius, kRoundedCorner1Radius, kRoundedCorner1Radius,
       kRoundedCorner1Radius});
  rounded_corner_layer_1->SetRoundedCorner(
      {kRoundedCorner2Radius, kRoundedCorner2Radius, kRoundedCorner2Radius,
       kRoundedCorner2Radius});
  rounded_corner_layer_2->SetRoundedCorner(
      {kRoundedCorner3Radius, kRoundedCorner3Radius, kRoundedCorner3Radius,
       kRoundedCorner3Radius});
  rounded_corner_layer_3->SetRoundedCorner(
      {kRoundedCorner4Radius, kRoundedCorner4Radius, kRoundedCorner4Radius,
       kRoundedCorner4Radius});

  fast_rounded_corner_layer_1->SetIsFastRoundedCorner(true);

  UpdateMainDrawProperties(kDeviceScale);

  // Since this layer has a descendant with rounded corner, it needs a render
  // surface.
  const EffectNode* effect_node =
      GetEffectNode(fast_rounded_corner_layer_1.get());
  gfx::RRectF rounded_corner_bounds_1 = effect_node->rounded_corner_bounds;
  EXPECT_TRUE(effect_node->HasRenderSurface());
  EXPECT_TRUE(effect_node->is_fast_rounded_corner);
  EXPECT_FLOAT_EQ(rounded_corner_bounds_1.GetSimpleRadius(),
                  kRoundedCorner1Radius);
  EXPECT_EQ(rounded_corner_bounds_1.rect(),
            gfx::RectF(kRoundedCornerLayer1Bound.size()));

  // Since this layer has no descendant with rounded corner or drawable, it will
  // not have a render surface.
  effect_node = GetEffectNode(rounded_corner_layer_1.get());
  gfx::RRectF rounded_corner_bounds_2 = effect_node->rounded_corner_bounds;
  EXPECT_FALSE(effect_node->HasRenderSurface());
  EXPECT_FALSE(effect_node->is_fast_rounded_corner);
  EXPECT_FLOAT_EQ(rounded_corner_bounds_2.GetSimpleRadius(),
                  kRoundedCorner2Radius);
  EXPECT_EQ(rounded_corner_bounds_2.rect(),
            gfx::RectF(kRoundedCornerLayer2Bound.size()));

  // Since this layer has a descendant with rounded corner, it should have a
  // render surface.
  effect_node = GetEffectNode(rounded_corner_layer_2.get());
  gfx::RRectF rounded_corner_bounds_3 = effect_node->rounded_corner_bounds;
  EXPECT_TRUE(effect_node->HasRenderSurface());
  EXPECT_FALSE(effect_node->is_fast_rounded_corner);
  EXPECT_FLOAT_EQ(rounded_corner_bounds_3.GetSimpleRadius(),
                  kRoundedCorner3Radius);
  EXPECT_EQ(rounded_corner_bounds_3.rect(),
            gfx::RectF(kRoundedCornerLayer3Bound.size()));

  // Since this layer has no descendant, it does not need a render surface.
  effect_node = GetEffectNode(rounded_corner_layer_3.get());
  gfx::RRectF rounded_corner_bounds_4 = effect_node->rounded_corner_bounds;
  EXPECT_FALSE(effect_node->HasRenderSurface());
  EXPECT_FALSE(effect_node->is_fast_rounded_corner);
  EXPECT_FLOAT_EQ(rounded_corner_bounds_4.GetSimpleRadius(),
                  kRoundedCorner4Radius);
  EXPECT_EQ(rounded_corner_bounds_4.rect(),
            gfx::RectF(kRoundedCornerLayer4Bound.size()));

  CommitAndActivate(kDeviceScale);
  LayerTreeImpl* layer_tree_impl = host()->host_impl()->active_tree();

  // Get the layer impl for each Layer.
  LayerImpl* fast_rounded_corner_layer_impl_1 =
      layer_tree_impl->LayerById(fast_rounded_corner_layer_1->id());
  LayerImpl* rounded_corner_layer_impl_1 =
      layer_tree_impl->LayerById(rounded_corner_layer_1->id());
  LayerImpl* rounded_corner_layer_impl_2 =
      layer_tree_impl->LayerById(rounded_corner_layer_2->id());
  LayerImpl* rounded_corner_layer_impl_3 =
      layer_tree_impl->LayerById(rounded_corner_layer_3->id());

  EXPECT_EQ(kDeviceScale, layer_tree_impl->device_scale_factor());

  // Fast rounded corner layer 1.
  // The render target for this layer is |root|, hence its target bounds are
  // relative to |root|.
  // The offset from the origin of the render target is [5, 5] and the device
  // scale factor is 1.6.
  const gfx::RRectF actual_self_rrect_1 =
      fast_rounded_corner_layer_impl_1->draw_properties().rounded_corner_bounds;
  EXPECT_TRUE(actual_self_rrect_1.IsEmpty());

  gfx::RectF bounds_in_target_space = kRoundedCornerLayer1Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  const gfx::RRectF actual_render_target_rrect_1 =
      fast_rounded_corner_layer_impl_1->render_target()
          ->rounded_corner_bounds();
  EXPECT_EQ(actual_render_target_rrect_1.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_render_target_rrect_1.GetSimpleRadius(),
                  kRoundedCorner1Radius * kDeviceScale);

  // Rounded corner layer 1
  // The render target for this layer is |fast_rounded_corner_layer_1|.
  // The offset from the origin of the render target is [0, 0] and the device
  // scale factor is 1.6. The corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_self_rrect_2 =
      rounded_corner_layer_impl_1->draw_properties().rounded_corner_bounds;
  bounds_in_target_space = kRoundedCornerLayer2Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  EXPECT_EQ(actual_self_rrect_2.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_self_rrect_2.GetSimpleRadius(),
                  kRoundedCorner2Radius * kDeviceScale);

  // Rounded corner layer 3
  // The render target for this layer is |root|.
  // The offset from the origin of the render target is [5, 5] and the device
  // scale factor is 1.6 thus giving the target space origin of [8, 8]. The
  // corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_self_rrect_3 =
      rounded_corner_layer_impl_2->draw_properties().rounded_corner_bounds;
  EXPECT_TRUE(actual_self_rrect_3.IsEmpty());

  bounds_in_target_space = kRoundedCornerLayer3Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  const gfx::RRectF actual_render_target_rrect_3 =
      rounded_corner_layer_impl_2->render_target()->rounded_corner_bounds();
  EXPECT_EQ(actual_render_target_rrect_3.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_render_target_rrect_3.GetSimpleRadius(),
                  kRoundedCorner3Radius * kDeviceScale);

  // Rounded corner layer 4
  // The render target for this layer is |rounded_corner_layer_2|.
  // The offset from the origin of the render target is [0, 5] and the device
  // scale factor is 1.6 thus giving the target space origin of [0, 8]. The
  // corner radius is also scaled by a factor of 1.6.
  const gfx::RRectF actual_self_rrect_4 =
      rounded_corner_layer_impl_3->draw_properties().rounded_corner_bounds;
  bounds_in_target_space = kRoundedCornerLayer4Bound;
  bounds_in_target_space.Scale(kDeviceScale);
  EXPECT_EQ(actual_self_rrect_4.rect(), bounds_in_target_space);
  EXPECT_FLOAT_EQ(actual_self_rrect_4.GetSimpleRadius(),
                  kRoundedCorner4Radius * kDeviceScale);
}

}  // namespace
}  // namespace cc
