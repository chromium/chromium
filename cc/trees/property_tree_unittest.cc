// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree.h"

#include <utility>

#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "cc/trees/viewport_property_ids.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace cc {
namespace {

class FakeProtectedSequenceSynchronizer : public ProtectedSequenceSynchronizer {
 public:
  bool IsOwnerThread() const override { return true; }
  bool InProtectedSequence() const override { return false; }
  void WaitForProtectedSequenceCompletion() const override {}
};

TEST(PropertyTreeTest, ComputeTransformRoot) {
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();
  TransformNode contents_root;
  contents_root.local.Translate(2, 2);
  contents_root.id = tree.Insert(contents_root, 0);
  tree.UpdateTransforms(1);

  gfx::Transform expected;
  gfx::Transform transform;
  expected.Translate(2, 2);
  tree.CombineTransformsBetween(1, 0, &transform);
  EXPECT_TRANSFORM_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-2, -2);
  bool success = tree.CombineInversesBetween(0, 1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORM_EQ(expected, transform);
}

TEST(PropertyTreeTest, SetNeedsUpdate) {
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();
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
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();
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
  EXPECT_TRANSFORM_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-3, -3);
  bool success = tree.CombineInversesBetween(1, 2, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORM_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(5, 5);
  tree.CombineTransformsBetween(2, 0, &transform);
  EXPECT_TRANSFORM_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-5, -5);
  success = tree.CombineInversesBetween(0, 2, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORM_EQ(expected, transform);
}

TEST(PropertyTreeTest, ComputeTransformSibling) {
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();
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
  EXPECT_TRANSFORM_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-4, -4);
  bool success = tree.CombineInversesBetween(2, 3, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORM_EQ(expected, transform);
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
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();
  TransformNode contents_root;
  contents_root.local.Translate(2, 2);
  contents_root.id = tree.Insert(contents_root, 0);
  tree.UpdateTransforms(1);

  TransformNode singular;
  singular.local.set_rc(2, 2, 0.0);
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
  EXPECT_TRANSFORM_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-4, -4);
  bool success = tree.CombineInversesBetween(3, 4, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORM_EQ(expected, transform);
}

// Tests that the transform for fixed elements is translated based on the
// overscroll nodes scroll_offset and that the clip node has an outset based on
// the overscroll distance.
TEST(PropertyTreeTest, UndoOverscroll) {
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);

  ViewportPropertyIds viewport_property_ids;
  ClipTree& clip_tree = property_trees.clip_tree_mutable();
  const gfx::RectF clip_rect(0, 0, 100, 100);
  ClipNode clip_node;
  clip_node.id = 1;
  clip_node.parent_id = 0;
  clip_node.clip = clip_rect;
  clip_tree.Insert(clip_node, 0);
  viewport_property_ids.outer_clip = clip_node.id;

  TransformTree& transform_tree = property_trees.transform_tree_mutable();
  TransformNode contents_root;
  contents_root.local.Translate(2, 2);
  contents_root.id = transform_tree.Insert(contents_root, 0);
  transform_tree.UpdateTransforms(1, &viewport_property_ids);

  const gfx::PointF overscroll_offset(0, 10);
  TransformNode overscroll_node;
  overscroll_node.scroll_offset = overscroll_offset;
  overscroll_node.id = transform_tree.Insert(overscroll_node, 1);
  viewport_property_ids.overscroll_elasticity_transform = overscroll_node.id;

  TransformNode fixed_node;
  fixed_node.should_undo_overscroll = true;
  fixed_node.id = transform_tree.Insert(fixed_node, 2);

  transform_tree.UpdateTransforms(2,
                                  &viewport_property_ids);  // overscroll_node
  transform_tree.UpdateTransforms(3, &viewport_property_ids);  // fixed_node

  gfx::Transform expected;
  expected.Translate(overscroll_offset.OffsetFromOrigin());
  EXPECT_TRANSFORM_EQ(expected, transform_tree.Node(fixed_node.id)->to_parent);

  gfx::RectF expected_clip_rect(clip_rect);
  expected_clip_rect.set_height(clip_rect.height() + overscroll_offset.y());
  EXPECT_EQ(clip_tree.Node(viewport_property_ids.outer_clip)->clip,
            expected_clip_rect);
}

TEST(PropertyTreeTest, TransformsWithFlattening) {
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();
  EffectTree& effect_tree = property_trees.effect_tree_mutable();

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
  draw_property_utils::ComputeTransforms(&tree, ViewportPropertyIds());
  property_trees.ResetCachedData();

  gfx::Transform flattened_rotation_about_x = rotation_about_x;
  flattened_rotation_about_x.Flatten();

  gfx::Transform to_target;
  property_trees.GetToTarget(child, effect_parent, &to_target);
  EXPECT_TRANSFORM_EQ(rotation_about_x, to_target);

  EXPECT_TRANSFORM_EQ(flattened_rotation_about_x * rotation_about_x,
                      tree.ToScreen(child));

  property_trees.GetToTarget(grand_child, effect_parent, &to_target);
  EXPECT_TRANSFORM_EQ(flattened_rotation_about_x * rotation_about_x, to_target);

  EXPECT_TRANSFORM_EQ(flattened_rotation_about_x * flattened_rotation_about_x *
                          rotation_about_x,
                      tree.ToScreen(grand_child));

  gfx::Transform grand_child_to_child;
  tree.CombineTransformsBetween(grand_child, child, &grand_child_to_child);
  EXPECT_TRANSFORM_EQ(rotation_about_x, grand_child_to_child);

  // Remove flattening at grand_child, and recompute transforms.
  tree.Node(grand_child)->flattens_inherited_transform = false;
  tree.set_needs_update(true);
  draw_property_utils::ComputeTransforms(&tree, ViewportPropertyIds());

  property_trees.GetToTarget(grand_child, effect_parent, &to_target);
  EXPECT_TRANSFORM_EQ(rotation_about_x * rotation_about_x, to_target);

  EXPECT_TRANSFORM_EQ(
      flattened_rotation_about_x * rotation_about_x * rotation_about_x,
      tree.ToScreen(grand_child));

  grand_child_to_child.MakeIdentity();
  tree.CombineTransformsBetween(grand_child, child, &grand_child_to_child);
  EXPECT_TRANSFORM_EQ(rotation_about_x, grand_child_to_child);
}

TEST(PropertyTreeTest, MultiplicationOrder) {
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();
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
  EXPECT_TRANSFORM_EQ(expected, transform);

  bool success = tree.CombineInversesBetween(0, 2, &inverse);
  EXPECT_TRUE(success);

  transform = transform * inverse;
  expected.MakeIdentity();
  EXPECT_TRANSFORM_EQ(expected, transform);
}

TEST(PropertyTreeTest, ComputeTransformWithUninvertibleTransform) {
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();
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
  EXPECT_TRANSFORM_EQ(expected, transform);

  // To compute this would require inverting the 0 matrix, so we cannot
  // succeed.
  bool success = tree.CombineInversesBetween(1, 2, &inverse);
  EXPECT_FALSE(success);
}

TEST(PropertyTreeTest, ComputeTransformToTargetWithZeroSurfaceContentsScale) {
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();
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
  EXPECT_TRANSFORM_EQ(expected_transform, transform);

  tree.Node(grand_parent_id)->local.MakeIdentity();
  tree.Node(grand_parent_id)->local.Scale(0.f, 2.f);
  tree.Node(grand_parent_id)->needs_local_transform_update = true;
  tree.set_needs_update(true);

  draw_property_utils::ComputeTransforms(&tree, ViewportPropertyIds());

  transform.MakeIdentity();
  tree.CombineTransformsBetween(child_id, grand_parent_id, &transform);
  EXPECT_TRANSFORM_EQ(expected_transform, transform);

  tree.Node(grand_parent_id)->local.MakeIdentity();
  tree.Node(grand_parent_id)->local.Scale(0.f, 0.f);
  tree.Node(grand_parent_id)->needs_local_transform_update = true;
  tree.set_needs_update(true);

  draw_property_utils::ComputeTransforms(&tree, ViewportPropertyIds());

  transform.MakeIdentity();
  tree.CombineTransformsBetween(child_id, grand_parent_id, &transform);
  EXPECT_TRANSFORM_EQ(expected_transform, transform);
}

TEST(PropertyTreeTest, FlatteningWhenDestinationHasOnlyFlatAncestors) {
  // This tests that flattening is performed correctly when
  // destination and its ancestors are flat, but there are 3d transforms
  // and flattening between the source and destination.
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();

  int parent = tree.Insert(TransformNode(), 0);
  tree.Node(parent)->local.Translate(2, 2);

  gfx::Transform rotation_about_x;
  rotation_about_x.RotateAboutXAxis(15);

  int child = tree.Insert(TransformNode(), parent);
  tree.Node(child)->local = rotation_about_x;

  int grand_child = tree.Insert(TransformNode(), child);
  tree.Node(grand_child)->flattens_inherited_transform = true;

  tree.set_needs_update(true);
  draw_property_utils::ComputeTransforms(&tree, ViewportPropertyIds());

  gfx::Transform flattened_rotation_about_x = rotation_about_x;
  flattened_rotation_about_x.Flatten();

  gfx::Transform grand_child_to_parent;
  tree.CombineTransformsBetween(grand_child, parent, &grand_child_to_parent);
  EXPECT_TRANSFORM_EQ(flattened_rotation_about_x, grand_child_to_parent);
}

TEST(PropertyTreeTest, ScreenSpaceOpacityUpdateTest) {
  // This tests that screen space opacity is updated for the subtree when
  // opacity of a node changes.
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  EffectTree& tree = property_trees.effect_tree_mutable();

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

TEST(PropertyTreeTest, SingularTransformSnapTest) {
  // This tests that to_target transform is not snapped when it has a singular
  // transform.
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  TransformTree& tree = property_trees.transform_tree_mutable();
  EffectTree& effect_tree = property_trees.effect_tree_mutable();

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

  draw_property_utils::ComputeTransforms(&tree, ViewportPropertyIds());
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
  rounded.Round2dTranslationComponents();
  property_trees.GetToTarget(child, effect_parent, &to_target);
  EXPECT_NE(to_target, rounded);
}

// Tests that CopyOutputRequests are transformed by the EffectTree, such that
// assumptions the original requestor made about coordinate spaces remains true
// after the EffectTree transforms the requests.
TEST(EffectTreeTest, CopyOutputRequestsAreTransformed) {
  using viz::CopyOutputRequest;

  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);

  TransformTree& transform_tree = property_trees.transform_tree_mutable();
  TransformNode contents_root;
  contents_root.local.Scale(2, 2);
  contents_root.id = transform_tree.Insert(contents_root, 0);
  transform_tree.UpdateTransforms(contents_root.id);

  EffectTree& effect_tree = property_trees.effect_tree_mutable();
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

  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);

  TransformTree& transform_tree = property_trees.transform_tree_mutable();
  TransformNode contents_root;
  contents_root.local.Scale(1.0f / 1.0e9f, 1.0f / 1.0e9f);
  contents_root.id = transform_tree.Insert(contents_root, 0);
  transform_tree.UpdateTransforms(contents_root.id);

  EffectTree& effect_tree = property_trees.effect_tree_mutable();
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

// Tests that GetPixelSnappedScrollOffset cannot return a negative offset, even
// when the snap amount is larger than the scroll offset. The snap amount can be
// (fractionally) larger due to floating point precision errors, and if the
// scroll offset is near zero that can naively lead to a negative offset being
// returned which is not desirable.
TEST(ScrollTreeTest, GetScrollOffsetForScrollTimelineNegativeOffset) {
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  ScrollTree& scroll_tree = property_trees.scroll_tree_mutable();
  TransformTree& transform_tree = property_trees.transform_tree_mutable();

  ElementId element_id(5);
  int transform_node_id = transform_tree.Insert(TransformNode(), 0);
  int scroll_node_id = scroll_tree.Insert(ScrollNode(), 0);
  scroll_tree.Node(scroll_node_id)->transform_id = transform_node_id;
  scroll_tree.Node(scroll_node_id)->element_id = element_id;

  // Set a scroll value close to 0.
  scroll_tree.SetScrollOffset(element_id, gfx::PointF(0, 0.1));
  transform_tree.Node(transform_node_id)->scrolls = true;
  transform_tree.Node(transform_node_id)->scroll_offset = gfx::PointF(0, 0.1);

  // Pretend that the snap amount was slightly larger than 0.1.
  transform_tree.Node(transform_node_id)->snap_amount = gfx::Vector2dF(0, 0.2);
  transform_tree.Node(transform_node_id)->needs_local_transform_update = false;

  // The returned offset should be clamped at a minimum of 0.
  gfx::PointF offset = scroll_tree.GetScrollOffsetForScrollTimeline(
      *scroll_tree.Node(scroll_node_id));
  EXPECT_EQ(offset.y(), 0);
}

// Verify that when fractional scroll delta is turned off, that the remaining
// fractional delta does not cause additional property changes.
TEST(ScrollTreeTest, PushScrollUpdatesFromMainThreadIntegerDelta) {
  const bool use_fractional_deltas = false;

  // Set up main property trees.
  FakeProtectedSequenceSynchronizer synchronizer;
  PropertyTrees property_trees(synchronizer);
  ScrollTree& main_scroll_tree = property_trees.scroll_tree_mutable();
  TransformTree& transform_tree = property_trees.transform_tree_mutable();
  ElementId element_id(5);
  int transform_node_id = transform_tree.Insert(TransformNode(), 0);
  int scroll_node_id = main_scroll_tree.Insert(ScrollNode(), 0);
  main_scroll_tree.Node(scroll_node_id)->transform_id = transform_node_id;
  main_scroll_tree.Node(scroll_node_id)->element_id = element_id;
  main_scroll_tree.Node(scroll_node_id)->is_composited = true;

  // Set up FakeLayerTreeHostImpl.
  TestTaskGraphRunner task_graph_runner;
  FakeImplTaskRunnerProvider impl_task_runner_provider;
  FakeLayerTreeHostImpl host_impl(CommitToPendingTreeLayerTreeSettings(),
                                  &impl_task_runner_provider,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();

  // Set up pending property trees.
  PropertyTrees* pending_property_trees =
      host_impl.pending_tree()->property_trees();
  EXPECT_TRUE(pending_property_trees);
  ScrollTree& pending_scroll_tree =
      pending_property_trees->scroll_tree_mutable();
  TransformTree& pending_transform_tree =
      pending_property_trees->transform_tree_mutable();
  transform_node_id = pending_transform_tree.Insert(TransformNode(), 0);
  scroll_node_id = pending_scroll_tree.Insert(ScrollNode(), 0);
  pending_scroll_tree.Node(scroll_node_id)->transform_id = transform_node_id;
  pending_scroll_tree.Node(scroll_node_id)->element_id = element_id;
  pending_scroll_tree.Node(scroll_node_id)->is_composited = true;
  pending_property_trees->scroll_tree_mutable().SetElementIdForNodeId(
      scroll_node_id, element_id);

  // Push main scroll to pending.
  main_scroll_tree.SetScrollOffset(element_id, gfx::PointF(0, 1));
  pending_scroll_tree.PushScrollUpdatesFromMainThread(
      property_trees, host_impl.pending_tree(), use_fractional_deltas);
  const SyncedScrollOffset* scroll_offset =
      pending_scroll_tree.GetSyncedScrollOffset(element_id);
  EXPECT_TRUE(scroll_offset);

  // Set a fractional delta and check it is not pulled with fractional delta
  // turned off.
  pending_scroll_tree.SetScrollOffsetDeltaForTesting(element_id,
                                                     gfx::Vector2dF(0, 0.25));
  main_scroll_tree.CollectScrollDeltasForTesting(use_fractional_deltas);
  EXPECT_EQ(gfx::PointF(0, 1),
            main_scroll_tree.current_scroll_offset(element_id));

  // Rounding logic turned on should not cause property change on push.
  host_impl.pending_tree()->property_trees()->set_changed(false);
  pending_scroll_tree.PushScrollUpdatesFromMainThread(
      property_trees, host_impl.pending_tree(), use_fractional_deltas);
  EXPECT_FALSE(host_impl.pending_tree()->property_trees()->changed());

  // Rounding logic turned off should cause property change on push.
  host_impl.pending_tree()->property_trees()->set_changed(false);
  pending_scroll_tree.PushScrollUpdatesFromMainThread(
      property_trees, host_impl.pending_tree(), true);
  EXPECT_TRUE(host_impl.pending_tree()->property_trees()->changed());
}

}  // namespace
}  // namespace cc
