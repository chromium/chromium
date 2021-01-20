// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include "base/stl_util.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(code_to_test)           \
  root->layer_tree_impl()->ResetAllChangeTracking();                      \
  code_to_test;                                                           \
  EXPECT_FALSE(root->LayerPropertyChanged());                             \
  EXPECT_FALSE(child->LayerPropertyChanged());                            \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(code_to_test)             \
  root->layer_tree_impl()->ResetAllChangeTracking();                 \
  code_to_test;                                                      \
  EXPECT_TRUE(root->LayerPropertyChanged());                         \
  EXPECT_TRUE(root->LayerPropertyChangedFromPropertyTrees());        \
  EXPECT_FALSE(root->LayerPropertyChangedNotFromPropertyTrees());    \
  EXPECT_TRUE(child->LayerPropertyChanged());                        \
  EXPECT_TRUE(child->LayerPropertyChangedFromPropertyTrees());       \
  EXPECT_FALSE(child->LayerPropertyChangedNotFromPropertyTrees());   \
  EXPECT_TRUE(grand_child->LayerPropertyChanged());                  \
  EXPECT_TRUE(grand_child->LayerPropertyChangedFromPropertyTrees()); \
  EXPECT_FALSE(grand_child->LayerPropertyChangedNotFromPropertyTrees());

#define EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(code_to_test)               \
  root->layer_tree_impl()->ResetAllChangeTracking();                      \
  root->layer_tree_impl()->property_trees()->full_tree_damaged = false;   \
  code_to_test;                                                           \
  EXPECT_TRUE(root->LayerPropertyChanged());                              \
  EXPECT_FALSE(root->LayerPropertyChangedFromPropertyTrees());            \
  EXPECT_TRUE(root->LayerPropertyChangedNotFromPropertyTrees());          \
  EXPECT_FALSE(child->LayerPropertyChanged());                            \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

#define VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(code_to_test)                   \
  root->layer_tree_impl()->ResetAllChangeTracking();                        \
  host_impl()->ForcePrepareToDraw();                                        \
  EXPECT_FALSE(host_impl()->active_tree()->needs_update_draw_properties()); \
  code_to_test;                                                             \
  EXPECT_TRUE(host_impl()->active_tree()->needs_update_draw_properties());

#define VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(code_to_test)                \
  root->layer_tree_impl()->ResetAllChangeTracking();                        \
  host_impl()->ForcePrepareToDraw();                                        \
  EXPECT_FALSE(host_impl()->active_tree()->needs_update_draw_properties()); \
  code_to_test;                                                             \
  EXPECT_FALSE(host_impl()->active_tree()->needs_update_draw_properties());

static gfx::Vector2dF ScrollDelta(LayerImpl* layer_impl) {
  gfx::ScrollOffset delta = layer_impl->layer_tree_impl()
                                ->property_trees()
                                ->scroll_tree.GetScrollOffsetDeltaForTesting(
                                    layer_impl->element_id());
  return gfx::Vector2dF(delta.x(), delta.y());
}

class LayerImplTest : public LayerTreeImplTestBase, public ::testing::Test {
 public:
  using LayerTreeImplTestBase::LayerTreeImplTestBase;
};

TEST_F(LayerImplTest, VerifyPendingLayerChangesAreTrackedProperly) {
  //
  // This test checks that LayerPropertyChanged() has the correct behavior.
  //

  // The constructor on this will fake that we are on the correct thread.
  // Create a simple LayerImpl tree:
  host_impl()->CreatePendingTree();
  LayerImpl* root = EnsureRootLayerInPendingTree();
  CreateClipNode(root);
  root->layer_tree_impl()->ResetAllChangeTracking();

  LayerImpl* child = AddLayerInPendingTree<LayerImpl>();
  CopyProperties(root, child);
  LayerImpl* grand_child = AddLayerInPendingTree<LayerImpl>();
  CopyProperties(child, grand_child);

  UpdatePendingTreeDrawProperties();

  // Creating children is an internal operation and should not mark layers as
  // changed.
  EXPECT_FALSE(root->LayerPropertyChanged());
  EXPECT_FALSE(child->LayerPropertyChanged());
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

  float arbitrary_number = 0.352f;
  gfx::Size arbitrary_size = gfx::Size(111, 222);
  gfx::Point arbitrary_point = gfx::Point(333, 444);

  gfx::Rect arbitrary_rect = gfx::Rect(arbitrary_point, arbitrary_size);
  SkColor arbitrary_color = SkColorSetRGB(10, 20, 30);
  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  FilterOperations arbitrary_filters;
  arbitrary_filters.Append(FilterOperation::CreateOpacityFilter(0.5f));

  // These properties are internal, and should not be considered "change" when
  // they are used.
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->UnionUpdateRect(arbitrary_rect));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBounds(arbitrary_size));
  UpdatePendingTreeDrawProperties();

  // Changing these properties affects the entire subtree of layers.
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      host_impl()->pending_tree()->SetFilterMutated(root->element_id(),
                                                    arbitrary_filters));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      host_impl()->pending_tree()->SetFilterMutated(root->element_id(),
                                                    FilterOperations()));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      host_impl()->pending_tree()->SetOpacityMutated(root->element_id(),
                                                     arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      host_impl()->pending_tree()->SetTransformMutated(root->element_id(),
                                                       arbitrary_transform));

  // Changing these properties only affects the layer itself.
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetDrawsContent(true));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetBackgroundColor(arbitrary_color));

  // Changing these properties does not cause the layer to be marked as changed
  // but does cause the layer to need to push properties.
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetElementId(ElementId(2)));

  // After setting all these properties already, setting to the exact same
  // values again should not cause any change.
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetContentsOpaque(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetDrawsContent(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetBounds(root->bounds()));
}

TEST_F(LayerImplTest, VerifyNeedsUpdateDrawProperties) {
  LayerImpl* root = root_layer();
  LayerImpl* layer = AddLayer<LayerImpl>();
  layer->SetBounds(gfx::Size(100, 100));
  LayerImpl* layer2 = AddLayer<LayerImpl>();
  SetElementIdsForTesting();

  CopyProperties(root, layer);
  CreateTransformNode(layer);
  CreateScrollNode(layer, gfx::Size(1, 1));
  CopyProperties(root, layer2);

  DCHECK(host_impl()->CanDraw());
  UpdateActiveTreeDrawProperties();

  float arbitrary_number = 0.352f;
  gfx::Size arbitrary_size = gfx::Size(111, 222);
  gfx::Vector2d arbitrary_vector2d = gfx::Vector2d(111, 222);
  gfx::Size large_size = gfx::Size(1000, 1000);
  SkColor arbitrary_color = SkColorSetRGB(10, 20, 30);
  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  FilterOperations arbitrary_filters;
  arbitrary_filters.Append(FilterOperation::CreateOpacityFilter(0.5f));

  // Set layer to draw content so that their draw property by property trees is
  // verified.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetDrawsContent(true));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer2->SetDrawsContent(true));

  // Create a render surface, because we must have a render surface if we have
  // filters.
  CreateEffectNode(layer).render_surface_reason = RenderSurfaceReason::kTest;
  UpdateActiveTreeDrawProperties();

  // Related filter functions.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      host_impl()->active_tree()->SetFilterMutated(root->element_id(),
                                                   arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      host_impl()->active_tree()->SetFilterMutated(root->element_id(),
                                                   arbitrary_filters));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      host_impl()->active_tree()->SetFilterMutated(root->element_id(),
                                                   FilterOperations()));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      host_impl()->active_tree()->SetFilterMutated(root->element_id(),
                                                   arbitrary_filters));

  // Related scrolling functions.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(large_size));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(large_size));
  host_impl()->active_tree()->set_needs_update_draw_properties();
  UpdateActiveTreeDrawProperties();

  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->ScrollBy(arbitrary_vector2d));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->ScrollBy(gfx::Vector2d()));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->layer_tree_impl()->DidUpdateScrollOffset(layer->element_id()));
  layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.SetScrollOffsetDeltaForTesting(layer->element_id(),
                                                   gfx::Vector2dF());
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetCurrentScrollOffset(
      gfx::ScrollOffset(arbitrary_vector2d.x(), arbitrary_vector2d.y())));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetCurrentScrollOffset(
      gfx::ScrollOffset(arbitrary_vector2d.x(), arbitrary_vector2d.y())));

  // Unrelated functions, always set to new values, always set needs update.
  host_impl()->active_tree()->set_needs_update_draw_properties();
  UpdateActiveTreeDrawProperties();
  CreateClipNode(layer);
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetContentsOpaque(true);
                                      layer->NoteLayerPropertyChanged());
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBackgroundColor(arbitrary_color));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      host_impl()->active_tree()->SetOpacityMutated(layer->element_id(),
                                                    arbitrary_number));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      host_impl()->active_tree()->SetTransformMutated(layer->element_id(),
                                                      arbitrary_transform));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(arbitrary_size);
                                      layer->NoteLayerPropertyChanged());

  // Unrelated functions, set to the same values, no needs update.
  GetEffectNode(layer)->filters = arbitrary_filters;
  UpdateActiveTreeDrawProperties();
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      host_impl()->active_tree()->SetFilterMutated(layer->element_id(),
                                                   arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetContentsOpaque(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetDrawsContent(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      layer->SetBackgroundColor(arbitrary_color));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetBounds(arbitrary_size));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(layer->SetElementId(ElementId(2)));
}

TEST_F(LayerImplTest, PerspectiveTransformHasReasonableScale) {
  LayerImpl* layer = root_layer();
  layer->SetBounds(gfx::Size(10, 10));
  layer->set_contributes_to_drawn_render_surface(true);

  // Ensure that we are close to the maximum scale for the matrix.
  {
    gfx::Transform transform;
    transform.Scale(10.2f, 15.1f);
    transform.ApplyPerspectiveDepth(10);
    layer->draw_properties().screen_space_transform = transform;

    ASSERT_TRUE(layer->ScreenSpaceTransform().HasPerspective());
    EXPECT_FLOAT_EQ(15.f, layer->GetIdealContentsScale());
  }
  // Ensure that we don't fall below the device scale factor.
  {
    gfx::Transform transform;
    transform.Scale(0.1f, 0.2f);
    transform.ApplyPerspectiveDepth(10);
    layer->draw_properties().screen_space_transform = transform;

    ASSERT_TRUE(layer->ScreenSpaceTransform().HasPerspective());
    EXPECT_FLOAT_EQ(1.f, layer->GetIdealContentsScale());
  }
  // Ensure that large scales don't end up extremely large.
  {
    gfx::Transform transform;
    transform.Scale(10000.1f, 10000.2f);
    transform.ApplyPerspectiveDepth(10);
    layer->draw_properties().screen_space_transform = transform;

    ASSERT_TRUE(layer->ScreenSpaceTransform().HasPerspective());
    EXPECT_FLOAT_EQ(127.f, layer->GetIdealContentsScale());
  }
  // Test case from crbug.com/766021.
  {
    gfx::Transform transform(-0.9397f, -0.7019f, 0.2796f, 2383.4521f,   // row 1
                             -0.0038f, 0.0785f, 1.0613f, 1876.4553f,    // row 2
                             -0.0835f, 0.9081f, -0.4105f, -2208.3035f,  // row 3
                             0.0001f, -0.0008f, 0.0003f, 2.8435f);      // row 4
    layer->draw_properties().screen_space_transform = transform;

    ASSERT_TRUE(layer->ScreenSpaceTransform().HasPerspective());
    EXPECT_FLOAT_EQ(1.f, layer->GetIdealContentsScale());
  }
}

class LayerImplScrollTest : public LayerImplTest {
 public:
  LayerImplScrollTest() : LayerImplScrollTest(LayerListSettings()) {}

  explicit LayerImplScrollTest(const LayerTreeSettings& settings)
      : LayerImplTest(settings) {
    LayerImpl* root = root_layer();
    root->SetBounds(gfx::Size(1, 1));

    layer_ = AddLayer<LayerImpl>();
    SetElementIdsForTesting();
    // Set the max scroll offset by noting that the root layer has bounds (1,1),
    // thus whatever bounds are set for the layer will be the max scroll
    // offset plus 1 in each direction.
    layer_->SetBounds(gfx::Size(51, 81));
    CopyProperties(root, layer_);
    CreateTransformNode(layer_);
    CreateScrollNode(layer_, gfx::Size(1, 1));
    UpdateActiveTreeDrawProperties();
  }

  LayerImpl* layer() { return layer_; }

  ScrollTree* scroll_tree(LayerImpl* layer_impl) {
    return &layer_impl->layer_tree_impl()->property_trees()->scroll_tree;
  }

 private:
  LayerImpl* layer_;
};

class CommitToPendingTreeLayerImplScrollTest : public LayerImplScrollTest {
 public:
  CommitToPendingTreeLayerImplScrollTest() : LayerImplScrollTest(settings()) {}

  LayerTreeSettings settings() {
    LayerListSettings settings;
    settings.commit_to_active_tree = false;
    return settings;
  }
};

TEST_F(LayerImplScrollTest, ScrollByWithZeroOffset) {
  // Test that LayerImpl::ScrollBy only affects ScrollDelta and total scroll
  // offset is bounded by the range [0, max scroll offset].

  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(layer()));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                   scroll_tree(layer())->GetScrollOffsetBaseForTesting(
                       layer()->element_id()));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), ScrollDelta(layer()));

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), CurrentScrollOffset(layer()));

  EXPECT_VECTOR_EQ(ScrollDelta(layer()), CurrentScrollOffset(layer()));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                   scroll_tree(layer())->GetScrollOffsetBaseForTesting(
                       layer()->element_id()));

  layer()->ScrollBy(gfx::Vector2dF(100, -100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), CurrentScrollOffset(layer()));

  EXPECT_VECTOR_EQ(ScrollDelta(layer()), CurrentScrollOffset(layer()));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                   scroll_tree(layer())->GetScrollOffsetBaseForTesting(
                       layer()->element_id()));
}

TEST_F(LayerImplScrollTest, ScrollByWithNonZeroOffset) {
  gfx::ScrollOffset scroll_offset(10, 5);
  scroll_tree(layer())->UpdateScrollOffsetBaseForTesting(layer()->element_id(),
                                                         scroll_offset);

  EXPECT_VECTOR_EQ(scroll_offset, CurrentScrollOffset(layer()));
  EXPECT_VECTOR_EQ(scroll_offset,
                   scroll_tree(layer())->GetScrollOffsetBaseForTesting(
                       layer()->element_id()));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), ScrollDelta(layer()));

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), CurrentScrollOffset(layer()));

  EXPECT_VECTOR_EQ(
      gfx::ScrollOffsetWithDelta(scroll_offset, ScrollDelta(layer())),
      CurrentScrollOffset(layer()));
  EXPECT_VECTOR_EQ(scroll_offset,
                   scroll_tree(layer())->GetScrollOffsetBaseForTesting(
                       layer()->element_id()));

  layer()->ScrollBy(gfx::Vector2dF(100, -100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), CurrentScrollOffset(layer()));

  EXPECT_VECTOR_EQ(
      gfx::ScrollOffsetWithDelta(scroll_offset, ScrollDelta(layer())),
      CurrentScrollOffset(layer()));
  EXPECT_VECTOR_EQ(scroll_offset,
                   scroll_tree(layer())->GetScrollOffsetBaseForTesting(
                       layer()->element_id()));
}

TEST_F(LayerImplScrollTest, ApplySentScrollsNoListener) {
  gfx::ScrollOffset scroll_offset(10, 5);
  gfx::Vector2dF scroll_delta(20.5f, 8.5f);
  gfx::Vector2d sent_scroll_delta(12, -3);

  scroll_tree(layer())->UpdateScrollOffsetBaseForTesting(layer()->element_id(),
                                                         scroll_offset);
  layer()->ScrollBy(sent_scroll_delta);
  scroll_tree(layer())->CollectScrollDeltasForTesting();
  layer()->SetCurrentScrollOffset(scroll_offset +
                                  gfx::ScrollOffset(scroll_delta));

  EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(scroll_offset, scroll_delta),
                   CurrentScrollOffset(layer()));
  EXPECT_VECTOR_EQ(scroll_delta, ScrollDelta(layer()));
  EXPECT_VECTOR_EQ(scroll_offset,
                   scroll_tree(layer())->GetScrollOffsetBaseForTesting(
                       layer()->element_id()));

  scroll_tree(layer())->ApplySentScrollDeltasFromAbortedCommit();

  EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(scroll_offset, scroll_delta),
                   CurrentScrollOffset(layer()));
  EXPECT_VECTOR_EQ(scroll_delta - sent_scroll_delta, ScrollDelta(layer()));
  EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(scroll_offset, sent_scroll_delta),
                   scroll_tree(layer())->GetScrollOffsetBaseForTesting(
                       layer()->element_id()));
}

TEST_F(LayerImplScrollTest, ScrollUserUnscrollableLayer) {
  gfx::ScrollOffset scroll_offset(10, 5);
  gfx::Vector2dF scroll_delta(20.5f, 8.5f);

  GetScrollNode(layer())->user_scrollable_vertical = false;
  UpdateDrawProperties(layer()->layer_tree_impl());
  scroll_tree(layer())->UpdateScrollOffsetBaseForTesting(layer()->element_id(),
                                                         scroll_offset);
  gfx::Vector2dF unscrolled = layer()->ScrollBy(scroll_delta);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 8.5f), unscrolled);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(30.5f, 5), CurrentScrollOffset(layer()));
}

// |LayerImpl::all_touch_action_regions_| is a cache of all regions on
// |LayerImpl::touch_action_region_| and must be invalidated on changes.
TEST_F(LayerImplScrollTest, TouchActionRegionCacheInvalidation) {
  host_impl()->CreatePendingTree();
  std::unique_ptr<LayerImpl> pending_layer =
      LayerImpl::Create(host_impl()->pending_tree(), 2);

  TouchActionRegion region;
  region.Union(TouchAction::kNone, gfx::Rect(0, 0, 50, 50));
  pending_layer->SetTouchActionRegion(region);

  // The values for GetAllTouchActionRegions should be correct on both layers.
  // Note that querying GetAllTouchActionRegions will update the cached value
  // in |LayerImpl::all_touch_action_regions_|.
  EXPECT_EQ(pending_layer->GetAllTouchActionRegions(), region.GetAllRegions());
  EXPECT_EQ(layer()->GetAllTouchActionRegions(), Region());

  pending_layer->PushPropertiesTo(layer());

  // After pushing properties, the value for GetAllTouchActionRegions should
  // not be stale.
  EXPECT_EQ(pending_layer->GetAllTouchActionRegions(), region.GetAllRegions());
  EXPECT_EQ(layer()->GetAllTouchActionRegions(), region.GetAllRegions());
}

TEST_F(CommitToPendingTreeLayerImplScrollTest,
       PushPropertiesToMirrorsCurrentScrollOffset) {
  gfx::ScrollOffset scroll_offset(10, 5);
  gfx::Vector2dF scroll_delta(12, 18);

  host_impl()->CreatePendingTree();

  scroll_tree(layer())->UpdateScrollOffsetBaseForTesting(layer()->element_id(),
                                                         scroll_offset);
  gfx::Vector2dF unscrolled = layer()->ScrollBy(scroll_delta);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), unscrolled);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(22, 23), CurrentScrollOffset(layer()));

  scroll_tree(layer())->CollectScrollDeltasForTesting();

  std::unique_ptr<LayerImpl> pending_layer =
      LayerImpl::Create(host_impl()->sync_tree(), layer()->id());
  pending_layer->SetElementId(
      LayerIdToElementIdForTesting(pending_layer->id()));
  scroll_tree(pending_layer.get())
      ->UpdateScrollOffsetBaseForTesting(pending_layer->element_id(),
                                         CurrentScrollOffset(layer()));

  pending_layer->PushPropertiesTo(layer());

  EXPECT_VECTOR_EQ(gfx::Vector2dF(22, 23), CurrentScrollOffset(layer()));
  EXPECT_VECTOR_EQ(CurrentScrollOffset(layer()),
                   CurrentScrollOffset(pending_layer.get()));
}

TEST_F(LayerImplTest, JitterTest) {
  host_impl()->CreatePendingTree();
  auto* root_layer = EnsureRootLayerInPendingTree();
  root_layer->SetBounds(gfx::Size(50, 50));
  SetupViewport(root_layer, gfx::Size(100, 100), gfx::Size(100, 100));
  auto* scroll_layer =
      host_impl()->pending_tree()->InnerViewportScrollLayerForTesting();
  auto* content_layer = AddLayerInPendingTree<LayerImpl>();
  content_layer->SetBounds(gfx::Size(100, 100));
  content_layer->SetDrawsContent(true);
  CopyProperties(
      host_impl()->pending_tree()->OuterViewportScrollLayerForTesting(),
      content_layer);
  UpdatePendingTreeDrawProperties();

  host_impl()->pending_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
  const int scroll = 5;
  int accumulated_scroll = 0;
  for (int i = 0; i < LayerTreeImpl::kFixedPointHitsThreshold + 1; ++i) {
    host_impl()->ActivateSyncTree();
    accumulated_scroll += scroll;
    SetScrollOffset(
        host_impl()->active_tree()->InnerViewportScrollLayerForTesting(),
        gfx::ScrollOffset(0, accumulated_scroll));
    UpdateActiveTreeDrawProperties();

    host_impl()->CreatePendingTree();
    LayerTreeImpl* pending_tree = host_impl()->pending_tree();
    pending_tree->set_source_frame_number(i + 1);
    pending_tree->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
    // Simulate scroll offset pushed from the main thread.
    SetScrollOffset(scroll_layer, gfx::ScrollOffset(0, accumulated_scroll));
    // The scroll done on the active tree is undone on the pending tree.
    content_layer->SetOffsetToTransformParent(
        gfx::Vector2dF(0, accumulated_scroll));
    content_layer->SetNeedsPushProperties();
    UpdateDrawProperties(pending_tree);

    float jitter = content_layer->CalculateJitter();
    // There should not be any jitter measured till we hit the fixed point hits
    // threshold. 250 is sqrt(50 * 50) * 5. 50x50 is the visible bounds of
    // content (clipped by the viewport). 5 is the distance between the
    // locations of the content in the pending tree and the active tree.
    float expected_jitter =
        (i == pending_tree->kFixedPointHitsThreshold) ? 250 : 0;
    EXPECT_EQ(jitter, expected_jitter);
  }
}

}  // namespace
}  // namespace cc
