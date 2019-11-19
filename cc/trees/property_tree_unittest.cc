// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree.h"

#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(PropertyTreeTest, ComputeTransformRoot) {
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;
  TransformNode contents_root;
  contents_root.local.Translate(2, 2);
  contents_root.id = tree.Insert(contents_root, 0);
  tree.UpdateTransforms(1);

  gfx::Transform expected;
  gfx::Transform transform;
  expected.Translate(2, 2);
  tree.CombineTransformsBetween(1, 0, &transform);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-2, -2);
  bool success = tree.CombineInversesBetween(0, 1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, SetNeedsUpdate) {
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;
  TransformNode contents_root;
  contents_root.id = tree.Insert(contents_root, 0);

  EXPECT_FALSE(tree.needs_update());
  tree.SetRootScaleAndTransform(0.6f, gfx::Transform());
  EXPECT_TRUE(tree.needs_update());
  tree.set_needs_update(false);
  tree.SetRootScaleAndTransform(0.6f, gfx::Transform());
  EXPECT_FALSE(tree.needs_update());
}

TEST(PropertyTreeTest, ComputeTransformChild) {
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;
  TransformNode contents_root;
  contents_root.local.Translate(2, 2);
  contents_root.id = tree.Insert(contents_root, 0);
  tree.UpdateTransforms(contents_root.id);

  TransformNode child;
  child.local.Translate(3, 3);
  child.id = tree.Insert(child, contents_root.id);

  tree.UpdateTransforms(child.id);

  gfx::Transform expected;
  gfx::Transform transform;

  expected.Translate(3, 3);
  tree.CombineTransformsBetween(2, 1, &transform);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-3, -3);
  bool success = tree.CombineInversesBetween(1, 2, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(5, 5);
  tree.CombineTransformsBetween(2, 0, &transform);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-5, -5);
  success = tree.CombineInversesBetween(0, 2, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, ComputeTransformSibling) {
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;
  TransformNode contents_root;
  contents_root.local.Translate(2, 2);
  contents_root.id = tree.Insert(contents_root, 0);
  tree.UpdateTransforms(1);

  TransformNode child;
  child.local.Translate(3, 3);
  child.id = tree.Insert(child, 1);

  TransformNode sibling;
  sibling.local.Translate(7, 7);
  sibling.id = tree.Insert(sibling, 1);

  tree.UpdateTransforms(2);
  tree.UpdateTransforms(3);

  gfx::Transform expected;
  gfx::Transform transform;

  expected.Translate(4, 4);
  tree.CombineTransformsBetween(3, 2, &transform);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-4, -4);
  bool success = tree.CombineInversesBetween(2, 3, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, ComputeTransformSiblingSingularAncestor) {
  // In this test, we have the following tree:
  // root
  //   + singular
  //     + child
  //     + sibling
  // Since the lowest common ancestor of |child| and |sibling| has a singular
  // transform, we cannot use screen space transforms to compute change of
  // basis
  // transforms between these nodes.
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;
  TransformNode contents_root;
  contents_root.local.Translate(2, 2);
  contents_root.id = tree.Insert(contents_root, 0);
  tree.UpdateTransforms(1);

  TransformNode singular;
  singular.local.matrix().set(2, 2, 0.0);
  singular.id = tree.Insert(singular, 1);

  TransformNode child;
  child.local.Translate(3, 3);
  child.id = tree.Insert(child, 2);

  TransformNode sibling;
  sibling.local.Translate(7, 7);
  sibling.id = tree.Insert(sibling, 2);

  tree.UpdateTransforms(2);
  tree.UpdateTransforms(3);
  tree.UpdateTransforms(4);

  gfx::Transform expected;
  gfx::Transform transform;

  expected.Translate(4, 4);
  tree.CombineTransformsBetween(4, 3, &transform);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-4, -4);
  bool success = tree.CombineInversesBetween(3, 4, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, TransformsWithFlattening) {
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;
  EffectTree& effect_tree = property_trees.effect_tree;

  int grand_parent = tree.Insert(TransformNode(), 0);
  int effect_grand_parent = effect_tree.Insert(EffectNode(), 0);
  effect_tree.Node(effect_grand_parent)->render_surface_reason =
      RenderSurfaceReason::kTest;
  effect_tree.Node(effect_grand_parent)->transform_id = grand_parent;
  effect_tree.Node(effect_grand_parent)->surface_contents_scale =
      gfx::Vector2dF(1.f, 1.f);

  gfx::Transform rotation_about_x;
  rotation_about_x.RotateAboutXAxis(15);

  int parent = tree.Insert(TransformNode(), grand_parent);
  int effect_parent = effect_tree.Insert(EffectNode(), effect_grand_parent);
  effect_tree.Node(effect_parent)->transform_id = parent;
  effect_tree.Node(effect_parent)->render_surface_reason =
      RenderSurfaceReason::kTest;
  effect_tree.Node(effect_parent)->surface_contents_scale =
      gfx::Vector2dF(1.f, 1.f);
  tree.Node(parent)->local = rotation_about_x;

  int child = tree.Insert(TransformNode(), parent);
  tree.Node(child)->flattens_inherited_transform = true;
  tree.Node(child)->local = rotation_about_x;

  int grand_child = tree.Insert(TransformNode(), child);
  tree.Node(grand_child)->flattens_inherited_transform = true;
  tree.Node(grand_child)->local = rotation_about_x;

  tree.set_needs_update(true);
  draw_property_utils::ComputeTransforms(&tree);
  property_trees.ResetCachedData();

  gfx::Transform flattened_rotation_about_x = rotation_about_x;
  flattened_rotation_about_x.FlattenTo2d();

  gfx::Transform to_target;
  property_trees.GetToTarget(child, effect_parent, &to_target);
  EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x, to_target);

  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_x * rotation_about_x,
                                  tree.ToScreen(child));

  property_trees.GetToTarget(grand_child, effect_parent, &to_target);
  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_x * rotation_about_x,
                                  to_target);

  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_x *
                                      flattened_rotation_about_x *
                                      rotation_about_x,
                                  tree.ToScreen(grand_child));

  gfx::Transform grand_child_to_child;
  tree.CombineTransformsBetween(grand_child, child, &grand_child_to_child);
  EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x, grand_child_to_child);

  // Remove flattening at grand_child, and recompute transforms.
  tree.Node(grand_child)->flattens_inherited_transform = false;
  tree.set_needs_update(true);
  draw_property_utils::ComputeTransforms(&tree);

  property_trees.GetToTarget(grand_child, effect_parent, &to_target);
  EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x * rotation_about_x,
                                  to_target);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      flattened_rotation_about_x * rotation_about_x * rotation_about_x,
      tree.ToScreen(grand_child));

  grand_child_to_child.MakeIdentity();
  tree.CombineTransformsBetween(grand_child, child, &grand_child_to_child);
  EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x, grand_child_to_child);
}

TEST(PropertyTreeTest, MultiplicationOrder) {
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;
  TransformNode contents_root;
  contents_root.local.Translate(2, 2);
  contents_root.id = tree.Insert(contents_root, 0);
  tree.UpdateTransforms(1);

  TransformNode child;
  child.local.Scale(2, 2);
  child.id = tree.Insert(child, 1);

  tree.UpdateTransforms(2);

  gfx::Transform expected;
  expected.Translate(2, 2);
  expected.Scale(2, 2);

  gfx::Transform transform;
  gfx::Transform inverse;

  tree.CombineTransformsBetween(2, 0, &transform);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  bool success = tree.CombineInversesBetween(0, 2, &inverse);
  EXPECT_TRUE(success);

  transform = transform * inverse;
  expected.MakeIdentity();
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, ComputeTransformWithUninvertibleTransform) {
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;
  TransformNode contents_root;
  contents_root.id = tree.Insert(contents_root, 0);
  tree.UpdateTransforms(1);

  TransformNode child;
  child.local.Scale(0, 0);
  child.id = tree.Insert(child, 1);

  tree.UpdateTransforms(2);

  gfx::Transform expected;
  expected.Scale(0, 0);

  gfx::Transform transform;
  gfx::Transform inverse;

  tree.CombineTransformsBetween(2, 1, &transform);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  // To compute this would require inverting the 0 matrix, so we cannot
  // succeed.
  bool success = tree.CombineInversesBetween(1, 2, &inverse);
  EXPECT_FALSE(success);
}

TEST(PropertyTreeTest, ComputeTransformToTargetWithZeroSurfaceContentsScale) {
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;
  TransformNode contents_root;
  contents_root.id = tree.Insert(contents_root, 0);
  tree.UpdateTransforms(1);

  TransformNode grand_parent;
  grand_parent.local.Scale(2.f, 0.f);
  int grand_parent_id = tree.Insert(grand_parent, 1);
  tree.UpdateTransforms(grand_parent_id);

  TransformNode parent;
  parent.local.Translate(1.f, 1.f);
  int parent_id = tree.Insert(parent, grand_parent_id);
  tree.UpdateTransforms(parent_id);

  TransformNode child;
  child.local.Translate(3.f, 4.f);
  int child_id = tree.Insert(child, parent_id);
  tree.UpdateTransforms(child_id);

  gfx::Transform expected_transform;
  expected_transform.Translate(4.f, 5.f);

  gfx::Transform transform;
  tree.CombineTransformsBetween(child_id, grand_parent_id, &transform);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform, transform);

  tree.Node(grand_parent_id)->local.MakeIdentity();
  tree.Node(grand_parent_id)->local.Scale(0.f, 2.f);
  tree.Node(grand_parent_id)->needs_local_transform_update = true;
  tree.set_needs_update(true);

  draw_property_utils::ComputeTransforms(&tree);

  transform.MakeIdentity();
  tree.CombineTransformsBetween(child_id, grand_parent_id, &transform);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform, transform);

  tree.Node(grand_parent_id)->local.MakeIdentity();
  tree.Node(grand_parent_id)->local.Scale(0.f, 0.f);
  tree.Node(grand_parent_id)->needs_local_transform_update = true;
  tree.set_needs_update(true);

  draw_property_utils::ComputeTransforms(&tree);

  transform.MakeIdentity();
  tree.CombineTransformsBetween(child_id, grand_parent_id, &transform);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform, transform);
}

TEST(PropertyTreeTest, FlatteningWhenDestinationHasOnlyFlatAncestors) {
  // This tests that flattening is performed correctly when
  // destination and its ancestors are flat, but there are 3d transforms
  // and flattening between the source and destination.
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;

  int parent = tree.Insert(TransformNode(), 0);
  tree.Node(parent)->local.Translate(2, 2);

  gfx::Transform rotation_about_x;
  rotation_about_x.RotateAboutXAxis(15);

  int child = tree.Insert(TransformNode(), parent);
  tree.Node(child)->local = rotation_about_x;

  int grand_child = tree.Insert(TransformNode(), child);
  tree.Node(grand_child)->flattens_inherited_transform = true;

  tree.set_needs_update(true);
  draw_property_utils::ComputeTransforms(&tree);

  gfx::Transform flattened_rotation_about_x = rotation_about_x;
  flattened_rotation_about_x.FlattenTo2d();

  gfx::Transform grand_child_to_parent;
  tree.CombineTransformsBetween(grand_child, parent, &grand_child_to_parent);
  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_x,
                                  grand_child_to_parent);
}

TEST(PropertyTreeTest, ScreenSpaceOpacityUpdateTest) {
  // This tests that screen space opacity is updated for the subtree when
  // opacity of a node changes.
  PropertyTrees property_trees;
  EffectTree& tree = property_trees.effect_tree;

  int parent = tree.Insert(EffectNode(), 0);
  int child = tree.Insert(EffectNode(), parent);

  EXPECT_EQ(tree.Node(child)->screen_space_opacity, 1.f);
  tree.Node(parent)->opacity = 0.5f;
  tree.set_needs_update(true);
  draw_property_utils::ComputeEffects(&tree);
  EXPECT_EQ(tree.Node(child)->screen_space_opacity, 0.5f);

  tree.Node(child)->opacity = 0.5f;
  tree.set_needs_update(true);
  draw_property_utils::ComputeEffects(&tree);
  EXPECT_EQ(tree.Node(child)->screen_space_opacity, 0.25f);
}

TEST(PropertyTreeTest, NonIntegerTranslationTest) {
  // This tests that when a node has non-integer translation, the information
  // is propagated to the subtree.
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;

  int parent = tree.Insert(TransformNode(), 0);
  tree.Node(parent)->local.Translate(1.5f, 1.5f);

  int child = tree.Insert(TransformNode(), parent);
  tree.Node(child)->local.Translate(1, 1);
  tree.set_needs_update(true);
  draw_property_utils::ComputeTransforms(&tree);
  EXPECT_FALSE(
      tree.Node(parent)->node_and_ancestors_have_only_integer_translation);
  EXPECT_FALSE(
      tree.Node(child)->node_and_ancestors_have_only_integer_translation);

  tree.Node(parent)->local.Translate(0.5f, 0.5f);
  tree.Node(child)->local.Translate(0.5f, 0.5f);
  tree.Node(parent)->needs_local_transform_update = true;
  tree.Node(child)->needs_local_transform_update = true;
  tree.set_needs_update(true);
  draw_property_utils::ComputeTransforms(&tree);
  EXPECT_TRUE(
      tree.Node(parent)->node_and_ancestors_have_only_integer_translation);
  EXPECT_FALSE(
      tree.Node(child)->node_and_ancestors_have_only_integer_translation);

  tree.Node(child)->local.Translate(0.5f, 0.5f);
  tree.Node(child)->needs_local_transform_update = true;
  tree.set_needs_update(true);
  draw_property_utils::ComputeTransforms(&tree);
  EXPECT_TRUE(
      tree.Node(parent)->node_and_ancestors_have_only_integer_translation);
  EXPECT_TRUE(
      tree.Node(child)->node_and_ancestors_have_only_integer_translation);
}

TEST(PropertyTreeTest, SingularTransformSnapTest) {
  // This tests that to_target transform is not snapped when it has a singular
  // transform.
  PropertyTrees property_trees;
  TransformTree& tree = property_trees.transform_tree;
  EffectTree& effect_tree = property_trees.effect_tree;

  int parent = tree.Insert(TransformNode(), 0);
  int effect_parent = effect_tree.Insert(EffectNode(), 0);
  effect_tree.Node(effect_parent)->render_surface_reason =
      RenderSurfaceReason::kTest;
  effect_tree.Node(effect_parent)->surface_contents_scale =
      gfx::Vector2dF(1.f, 1.f);
  tree.Node(parent)->scrolls = true;

  int child = tree.Insert(TransformNode(), parent);
  TransformNode* child_node = tree.Node(child);
  child_node->scrolls = true;
  child_node->local.Scale3d(6.0f, 6.0f, 0.0f);
  child_node->local.Translate(1.3f, 1.3f);
  tree.set_needs_update(true);

  draw_property_utils::ComputeTransforms(&tree);
  property_trees.ResetCachedData();

  gfx::Transform from_target;
  gfx::Transform to_target;
  property_trees.GetToTarget(child, effect_parent, &to_target);
  EXPECT_FALSE(to_target.GetInverse(&from_target));
  // The following checks are to ensure that snapping is skipped because of
  // singular transform (and not because of other reasons which also cause
  // snapping to be skipped).
  EXPECT_TRUE(child_node->scrolls);
  property_trees.GetToTarget(child, effect_parent, &to_target);
  EXPECT_TRUE(to_target.IsScaleOrTranslation());
  EXPECT_FALSE(child_node->to_screen_is_potentially_animated);
  EXPECT_FALSE(child_node->ancestors_are_invertible);

  gfx::Transform rounded;
  property_trees.GetToTarget(child, effect_parent, &rounded);
  rounded.RoundTranslationComponents();
  property_trees.GetToTarget(child, effect_parent, &to_target);
  EXPECT_NE(to_target, rounded);
}

// Tests that CopyOutputRequests are transformed by the EffectTree, such that
// assumptions the original requestor made about coordinate spaces remains true
// after the EffectTree transforms the requests.
TEST(EffectTreeTest, CopyOutputRequestsAreTransformed) {
  using viz::CopyOutputRequest;

  PropertyTrees property_trees;

  TransformTree& transform_tree = property_trees.transform_tree;
  TransformNode contents_root;
  contents_root.local.Scale(2, 2);
  contents_root.id = transform_tree.Insert(contents_root, 0);
  transform_tree.UpdateTransforms(contents_root.id);

  EffectTree& effect_tree = property_trees.effect_tree;
  EffectNode effect_node;
  effect_node.render_surface_reason = RenderSurfaceReason::kTest;
  effect_node.has_copy_request = true;
  effect_node.transform_id = contents_root.id;
  effect_node.id = effect_tree.Insert(effect_node, 0);
  effect_tree.UpdateEffects(effect_node.id);

  // A CopyOutputRequest with only its area set should be transformed into one
  // that is scaled by two. In this case, by specifying no result selection, the
  // requestor has indicated they want all the pixels, regardless of size. Thus,
  // the result selection and scale ratio should still be unset in the
  // transformed request to carry-over those semantics.
  auto request_in = CopyOutputRequest::CreateStubForTesting();
  request_in->set_area(gfx::Rect(10, 20, 30, 40));
  effect_tree.AddCopyRequest(effect_node.id, std::move(request_in));
  std::vector<std::unique_ptr<CopyOutputRequest>> requests_out;
  effect_tree.TakeCopyRequestsAndTransformToSurface(effect_node.id,
                                                    &requests_out);
  ASSERT_EQ(1u, requests_out.size());
  const CopyOutputRequest* request_out = requests_out.front().get();
  ASSERT_TRUE(request_out->has_area());
  EXPECT_EQ(gfx::Rect(20, 40, 60, 80), request_out->area());
  EXPECT_FALSE(request_out->has_result_selection());
  EXPECT_FALSE(request_out->is_scaled());

  // A CopyOutputRequest with its area and result selection set, but no scaling
  // specified, should be transformed into one that has its area scaled by two,
  // but now also includes a scale ratio of 1/2. This is because the requestor
  // had originally specified a result selection under old assumptions about the
  // source coordinate system.
  request_in = CopyOutputRequest::CreateStubForTesting();
  request_in->set_area(gfx::Rect(10, 20, 30, 40));
  request_in->set_result_selection(gfx::Rect(1, 2, 3, 4));
  effect_tree.AddCopyRequest(effect_node.id, std::move(request_in));
  requests_out.clear();
  effect_tree.TakeCopyRequestsAndTransformToSurface(effect_node.id,
                                                    &requests_out);
  ASSERT_EQ(1u, requests_out.size());
  request_out = requests_out.front().get();
  ASSERT_TRUE(request_out->has_area());
  EXPECT_EQ(gfx::Rect(20, 40, 60, 80), request_out->area());
  ASSERT_TRUE(request_out->has_result_selection());
  EXPECT_EQ(gfx::Rect(1, 2, 3, 4), request_out->result_selection());
  ASSERT_TRUE(request_out->is_scaled());
  EXPECT_NEAR(0.5f,
              static_cast<float>(request_out->scale_to().x()) /
                  request_out->scale_from().x(),
              0.000001);
  EXPECT_NEAR(0.5f,
              static_cast<float>(request_out->scale_to().y()) /
                  request_out->scale_from().y(),
              0.000001);

  // A CopyOutputRequest with all three of: area, result selection, and scale
  // ratio; should be transformed into one with an updated area and combined
  // scale ratio.
  request_in = CopyOutputRequest::CreateStubForTesting();
  request_in->set_area(gfx::Rect(10, 20, 30, 40));
  request_in->set_result_selection(gfx::Rect(1, 2, 3, 4));
  // Request has a 3X scale in X, and 5X scale in Y.
  request_in->SetScaleRatio(gfx::Vector2d(1, 1), gfx::Vector2d(3, 5));
  effect_tree.AddCopyRequest(effect_node.id, std::move(request_in));
  requests_out.clear();
  effect_tree.TakeCopyRequestsAndTransformToSurface(effect_node.id,
                                                    &requests_out);
  ASSERT_EQ(1u, requests_out.size());
  request_out = requests_out.front().get();
  ASSERT_TRUE(request_out->has_area());
  EXPECT_EQ(gfx::Rect(20, 40, 60, 80), request_out->area());
  ASSERT_TRUE(request_out->has_result_selection());
  EXPECT_EQ(gfx::Rect(1, 2, 3, 4), request_out->result_selection());
  ASSERT_TRUE(request_out->is_scaled());
  EXPECT_NEAR(3.0f / 2.0f,
              static_cast<float>(request_out->scale_to().x()) /
                  request_out->scale_from().x(),
              0.000001);
  EXPECT_NEAR(5.0f / 2.0f,
              static_cast<float>(request_out->scale_to().y()) /
                  request_out->scale_from().y(),
              0.000001);
}

// Tests that a good CopyOutputRequest which becomes transformed into an invalid
// one is dropped (i.e., the requestor would get an "empty response" in its
// result callback). The scaling transform in this test is so extreme that it
// would result in an illegal adjustment to the CopyOutputRequest's scale ratio.
TEST(EffectTreeTest, CopyOutputRequestsThatBecomeIllegalAreDropped) {
  using viz::CopyOutputRequest;

  PropertyTrees property_trees;

  TransformTree& transform_tree = property_trees.transform_tree;
  TransformNode contents_root;
  contents_root.local.Scale(1.0f / 1.0e9f, 1.0f / 1.0e9f);
  contents_root.id = transform_tree.Insert(contents_root, 0);
  transform_tree.UpdateTransforms(contents_root.id);

  EffectTree& effect_tree = property_trees.effect_tree;
  EffectNode effect_node;
  effect_node.render_surface_reason = RenderSurfaceReason::kTest;
  effect_node.has_copy_request = true;
  effect_node.transform_id = contents_root.id;
  effect_node.id = effect_tree.Insert(effect_node, 0);
  effect_tree.UpdateEffects(effect_node.id);

  auto request_in = CopyOutputRequest::CreateStubForTesting();
  request_in->set_area(gfx::Rect(10, 20, 30, 40));
  request_in->set_result_selection(gfx::Rect(1, 2, 3, 4));
  request_in->SetScaleRatio(gfx::Vector2d(1, 1), gfx::Vector2d(3, 5));
  effect_tree.AddCopyRequest(effect_node.id, std::move(request_in));
  std::vector<std::unique_ptr<CopyOutputRequest>> requests_out;
  effect_tree.TakeCopyRequestsAndTransformToSurface(effect_node.id,
                                                    &requests_out);
  EXPECT_TRUE(requests_out.empty());
}

}  // namespace
}  // namespace cc
