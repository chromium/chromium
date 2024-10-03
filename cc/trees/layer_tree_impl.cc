// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <unordered_set>
#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/features.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/base/synced_property.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/layers/effect_tree_layer_list_iterator.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/resources/ui_resource_request.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "cc/trees/tree_synchronizer.h"
#include "cc/view_transition/view_transition_request.h"
#include "components/viz/common/traced_value.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {
namespace {
// Small helper class that saves the current viewport location as the user sees
// it and resets to the same location.
class ViewportAnchor {
 public:
  ViewportAnchor(ScrollNode* inner_scroll,
                 ScrollNode* outer_scroll,
                 LayerTreeImpl* tree_impl)
      : inner_(inner_scroll), outer_(outer_scroll), tree_impl_(tree_impl) {
    viewport_in_content_coordinates_ =
        scroll_tree()
            .current_scroll_offset(inner_->element_id)
            .OffsetFromOrigin();

    if (outer_) {
      viewport_in_content_coordinates_ +=
          scroll_tree()
              .current_scroll_offset(outer_->element_id)
              .OffsetFromOrigin();
    }
  }

  void ResetViewportToAnchoredPosition() {
    DCHECK(outer_);

    scroll_tree().ClampScrollToMaxScrollOffset(*inner_, tree_impl_);
    scroll_tree().ClampScrollToMaxScrollOffset(*outer_, tree_impl_);

    gfx::Vector2dF viewport_location =
        scroll_tree()
            .current_scroll_offset(inner_->element_id)
            .OffsetFromOrigin() +
        scroll_tree()
            .current_scroll_offset(outer_->element_id)
            .OffsetFromOrigin();

    gfx::Vector2dF delta = viewport_in_content_coordinates_ - viewport_location;

    delta = scroll_tree().ScrollBy(*inner_, delta, tree_impl_);
    scroll_tree().ScrollBy(*outer_, delta, tree_impl_);
  }

 private:
  ScrollTree& scroll_tree() {
    return tree_impl_->property_trees()->scroll_tree_mutable();
  }

  raw_ptr<ScrollNode> inner_;
  raw_ptr<ScrollNode> outer_;
  raw_ptr<LayerTreeImpl> tree_impl_;
  gfx::Vector2dF viewport_in_content_coordinates_;
};

std::pair<gfx::PointF, gfx::PointF> GetVisibleSelectionEndPoints(
    const gfx::RectF& rect,
    const gfx::PointF& top,
    const gfx::PointF& bottom) {
  gfx::PointF start(std::clamp(top.x(), rect.x(), rect.right()),
                    std::clamp(top.y(), rect.y(), rect.bottom()));
  gfx::PointF end = start + (bottom - top);
  return {start, end};
}

}  // namespace

void LayerTreeLifecycle::AdvanceTo(LifecycleState next_state) {
  switch (next_state) {
    case (kNotSyncing):
      DCHECK_EQ(state_, kLastSyncState);
      break;
    case (kBeginningSync):
    case (kSyncedPropertyTrees):
    case (kSyncedLayerProperties):
      // Only allow tree synchronization states to be transitioned in order.
      DCHECK_EQ(state_ + 1, next_state);
      break;
  }
  state_ = next_state;
}

LayerTreeImpl::LayerTreeImpl(
    LayerTreeHostImpl& host_impl,
    scoped_refptr<SyncedScale> page_scale_factor,
    scoped_refptr<SyncedBrowserControls> top_controls_shown_ratio,
    scoped_refptr<SyncedBrowserControls> bottom_controls_shown_ratio,
    scoped_refptr<SyncedElasticOverscroll> elastic_overscroll)
    : host_impl_(&host_impl),
      source_frame_number_(-1),
      is_first_frame_after_commit_tracker_(-1),
      hud_layer_(nullptr),
      property_trees_(host_impl),
      background_color_(SkColors::kTransparent),
      last_scrolled_scroll_node_index_(kInvalidPropertyNodeId),
      page_scale_factor_(page_scale_factor),
      min_page_scale_factor_(0),
      max_page_scale_factor_(0),
      external_page_scale_factor_(1.f),
      device_scale_factor_(1.f),
      painted_device_scale_factor_(1.f),
      always_push_properties_on_picture_layers_(!base::FeatureList::IsEnabled(
          features::kDontAlwaysPushPictureLayerImpls)),
      elastic_overscroll_(elastic_overscroll),
      event_listener_properties_(),
      top_controls_shown_ratio_(std::move(top_controls_shown_ratio)),
      bottom_controls_shown_ratio_(std::move(bottom_controls_shown_ratio)) {
  property_trees()->set_is_main_thread(false);
}

LayerTreeImpl::~LayerTreeImpl() {
  // Need to explicitly clear the tree prior to destroying this so that
  // the LayerTreeImpl pointer is still valid in the LayerImpl dtor.
  DCHECK(LayerListIsEmpty());
}

void LayerTreeImpl::Shutdown() {
  DetachLayers();
  BreakSwapPromises(IsActiveTree() ? SwapPromise::SWAP_FAILS
                                   : SwapPromise::ACTIVATION_FAILS);
  DCHECK(LayerListIsEmpty());
}

void LayerTreeImpl::ReleaseResources() {
  for (auto* layer : *this)
    layer->ReleaseResources();
}

void LayerTreeImpl::OnPurgeMemory() {
  for (auto* layer : *this)
    layer->OnPurgeMemory();
}

void LayerTreeImpl::ReleaseTileResources() {
  for (auto* layer : *this)
    layer->ReleaseTileResources();
}

void LayerTreeImpl::RecreateTileResources() {
  for (auto* layer : *this)
    layer->RecreateTileResources();
}

void LayerTreeImpl::SetVisible(bool visible) {
  if (!visible) {
    for (auto* layer : *this) {
      layer->SetInInvisibleLayerTree();
    }
  }
}

void LayerTreeImpl::DidUpdateScrollOffset(
    ElementId id,
    bool pushed_from_main_or_pending_tree) {
  DCHECK(lifecycle().AllowsPropertyTreeAccess());
  const ScrollTree& scroll_tree = property_trees()->scroll_tree();
  const auto* scroll_node = scroll_tree.FindNodeFromElementId(id);

  if (!scroll_node) {
    // A scroll node should always exist on the active tree but may not exist
    // if we're updating the other trees from the active tree. This can occur
    // when the pending tree represents a different page, for example.
    DCHECK(!IsActiveTree());
    return;
  }

  bool can_realize_on_active_tree =
      scroll_tree.CanRealizeScrollsOnActiveTree(*scroll_node);
  bool can_realize_on_pending_tree =
      scroll_tree.CanRealizeScrollsOnPendingTree(*scroll_node);
  // This bit controls whether we'll update the transform node based on a
  // changed scroll offset. We have mutated the scroll nodes, but if the scroll
  // needs additional handling in the pending tree or the main thread, we don't
  // want to produce any immediate changes in the active tree or any compositor
  // tree. For example, if the scroll needs main thread, we want the scroll to
  // propagate through Blink in a commit and have Blink update properties,
  // paint, compositing, etc. Thus, we avoid mutating the transform tree in
  // these cases.
  bool can_realize_now = can_realize_on_active_tree;
  if (!IsActiveTree()) {
    can_realize_now |= can_realize_on_pending_tree;
  }

  if (can_realize_now) {
    CHECK_NE(scroll_node->transform_id, kInvalidPropertyNodeId);
    TransformTree& transform_tree = property_trees()->transform_tree_mutable();
    auto* transform_node = transform_tree.Node(scroll_node->transform_id);
    if (transform_node->scroll_offset !=
        scroll_tree.current_scroll_offset(id)) {
      transform_node->scroll_offset = scroll_tree.current_scroll_offset(id);
      transform_node->needs_local_transform_update = true;
      transform_tree.set_needs_update(true);
    }
    transform_node->transform_changed = true;
    property_trees()->set_changed(true);
    set_needs_update_draw_properties();
  } else if (can_realize_on_pending_tree) {
    host_impl_->RequestImplSideInvalidationForRasterInducingScroll(id);
  }

  if (IsActiveTree()) {
    // Ensure the other trees are kept in sync.
    if (host_impl_->pending_tree() && !pushed_from_main_or_pending_tree) {
      host_impl_->pending_tree()->DidUpdateScrollOffset(
          id, pushed_from_main_or_pending_tree);
    }
    if (host_impl_->recycle_tree()) {
      host_impl_->recycle_tree()->DidUpdateScrollOffset(
          id, pushed_from_main_or_pending_tree);
    }

    // If the scroll offset is pushed from main or pending tree, we'll call
    // UpdateAllScrollbarGeometries() soon, so no need to update here. Also we
    // can't do that here because the needed data may not be complete yet.
    if (!pushed_from_main_or_pending_tree) {
      if (scroll_node == InnerViewportScrollNode() ||
          scroll_node == OuterViewportScrollNode()) {
        UpdateViewportScrollbarGeometries();
      } else {
        UpdateScrollbarGeometries(*scroll_node);
      }
    }
  }
}

void LayerTreeImpl::UpdateAllScrollbarGeometries() {
  if (!IsActiveTree())
    return;

  // Property tree and layer properties such should be up-to-date.
  DCHECK(lifecycle().AllowsPropertyTreeAccess());
  DCHECK(lifecycle().AllowsLayerPropertyAccess());

  for (auto& pair : element_id_to_scrollbar_layer_ids_) {
    if (const auto* scroll_node =
            property_trees()->scroll_tree().FindNodeFromElementId(pair.first)) {
      UpdateScrollbarGeometries(*scroll_node);
    }
  }
}

void LayerTreeImpl::UpdateViewportScrollbarGeometries() {
  if (!IsActiveTree()) {
    return;
  }
  if (auto* inner_scroll = InnerViewportScrollNode()) {
    UpdateScrollbarGeometries(*inner_scroll);
  }
  if (auto* outer_scroll = OuterViewportScrollNode()) {
    UpdateScrollbarGeometries(*outer_scroll);
  }
}

void LayerTreeImpl::UpdateScrollbarGeometries(const ScrollNode& scroll_node) {
  DCHECK(IsActiveTree());
  const auto& scroll_tree = property_trees()->scroll_tree();
  gfx::PointF current_offset =
      scroll_tree.current_scroll_offset(scroll_node.element_id);
  gfx::SizeF scrolling_size(scroll_node.bounds);
  gfx::Size bounds_size(scroll_tree.container_bounds(scroll_node.id));

  bool is_viewport_scrollbar = &scroll_node == InnerViewportScrollNode() ||
                               &scroll_node == OuterViewportScrollNode();
  if (is_viewport_scrollbar) {
    gfx::SizeF viewport_bounds(bounds_size);
    if (&scroll_node == InnerViewportScrollNode()) {
      auto* outer_scroll_node = OuterViewportScrollNode();
      DCHECK(outer_scroll_node);

      // Add offset and bounds contribution of outer viewport.
      current_offset +=
          scroll_tree.current_scroll_offset(outer_scroll_node->element_id)
              .OffsetFromOrigin();
      gfx::SizeF outer_viewport_bounds(
          scroll_tree.container_bounds(outer_scroll_node->id));
      viewport_bounds.SetToMin(outer_viewport_bounds);
      // The scrolling size is only determined by the outer viewport.
      scrolling_size = gfx::SizeF(outer_scroll_node->bounds);
    } else {
      DCHECK_EQ(&scroll_node, OuterViewportScrollNode());
      auto* inner_scroll_node = InnerViewportScrollNode();
      DCHECK(inner_scroll_node);
      // Add offset and bounds contribution of inner viewport.
      current_offset +=
          scroll_tree.current_scroll_offset(inner_scroll_node->element_id)
              .OffsetFromOrigin();
      gfx::SizeF inner_viewport_bounds(
          scroll_tree.container_bounds(inner_scroll_node->id));
      viewport_bounds.SetToMin(inner_viewport_bounds);
    }
    viewport_bounds.InvScale(current_page_scale_factor());
    bounds_size = ToCeiledSize(viewport_bounds);
  }

  for (auto* scrollbar : ScrollbarsFor(scroll_node.element_id)) {
    if (scrollbar->orientation() == ScrollbarOrientation::kHorizontal) {
      scrollbar->SetCurrentPos(current_offset.x());
      scrollbar->SetClipLayerLength(bounds_size.width());
      scrollbar->SetScrollLayerLength(scrolling_size.width());
    } else {
      scrollbar->SetCurrentPos(current_offset.y());
      scrollbar->SetClipLayerLength(bounds_size.height());
      scrollbar->SetScrollLayerLength(scrolling_size.height());
    }
    if (is_viewport_scrollbar) {
      scrollbar->SetVerticalAdjust(
          property_trees_.inner_viewport_container_bounds_delta().y());
    }
  }
}

const RenderSurfaceImpl* LayerTreeImpl::RootRenderSurface() const {
  return property_trees_.effect_tree().GetRenderSurface(
      kContentsRootPropertyNodeId);
}

bool LayerTreeImpl::LayerListIsEmpty() const {
  return layer_list_.empty();
}

void LayerTreeImpl::SetRootLayerForTesting(std::unique_ptr<LayerImpl> layer) {
  DetachLayers();
  if (layer)
    AddLayer(std::move(layer));
  host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::OnCanDrawStateChangedForTree() {
  host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::InvalidateRegionForImages(
    const PaintImageIdFlatSet& images_to_invalidate) {
  TRACE_EVENT_BEGIN1("cc", "LayerTreeImpl::InvalidateRegionForImages",
                     "total_layer_count", picture_layers_.size());
  DCHECK(IsSyncTree());

  size_t no_images_count = 0;
  size_t no_invalidation_count = 0;
  size_t invalidated_count = 0;
  if (!images_to_invalidate.empty()) {
    // TODO(khushalsagar): It might be better to keep track of layers with
    // images and only iterate through those here.
    for (PictureLayerImpl* picture_layer : picture_layers_) {
      auto result =
          picture_layer->InvalidateRegionForImages(images_to_invalidate);
      switch (result) {
        case PictureLayerImpl::ImageInvalidationResult::kNoImages:
          ++no_images_count;
          break;
        case PictureLayerImpl::ImageInvalidationResult::kNoInvalidation:
          ++no_invalidation_count;
          break;
        case PictureLayerImpl::ImageInvalidationResult::kInvalidated:
          ++invalidated_count;
          break;
      }
    }
  }
  TRACE_EVENT_END1(
      "cc", "LayerTreeImpl::InvalidateRegionForImages", "counts",
      base::StringPrintf("no_images[%zu] no_invalidaton[%zu] invalidated[%zu]",
                         no_images_count, no_invalidation_count,
                         invalidated_count));
}

void LayerTreeImpl::InvalidateRasterInducingScrolls(
    const base::flat_set<ElementId>& scrolls_to_invalidate) {
  if (scrolls_to_invalidate.empty()) {
    return;
  }
  DCHECK(IsSyncTree());
  for (PictureLayerImpl* picture_layer : picture_layers_) {
    picture_layer->InvalidateRasterInducingScrolls(scrolls_to_invalidate);
  }
}

void LayerTreeImpl::UpdateViewportContainerSizes() {
  if (!InnerViewportScrollNode())
    return;

  DCHECK(OuterViewportScrollNode());
  ViewportAnchor anchor(InnerViewportScrollNode(), OuterViewportScrollNode(),
                        this);

  float top_controls_shown_ratio =
      top_controls_shown_ratio_->Current(IsActiveTree());
  float bottom_controls_shown_ratio =
      bottom_controls_shown_ratio_->Current(IsActiveTree());
  float top_controls_layout_height = browser_controls_shrink_blink_size()
                                         ? top_controls_height()
                                         : top_controls_min_height();
  float top_content_offset =
      top_controls_height() > 0
          ? top_controls_height() * top_controls_shown_ratio
          : 0.f;
  float delta_from_top_controls =
      top_controls_layout_height - top_content_offset;
  float bottom_controls_layout_height = browser_controls_shrink_blink_size()
                                            ? bottom_controls_height()
                                            : bottom_controls_min_height();
  float bottom_content_offset =
      bottom_controls_height() > 0
          ? bottom_controls_height() * bottom_controls_shown_ratio
          : 0.f;
  delta_from_top_controls +=
      bottom_controls_layout_height - bottom_content_offset;

  // Adjust the viewport layers by shrinking/expanding the container to account
  // for changes in the size (e.g. browser controls) since the last resize from
  // Blink.
  auto* property_trees = this->property_trees();
  gfx::Vector2dF bounds_delta(0.f, delta_from_top_controls);
  if (property_trees->inner_viewport_container_bounds_delta() == bounds_delta)
    return;

  property_trees->SetInnerViewportContainerBoundsDelta(bounds_delta);

  // Adjust the outer viewport container as well, since adjusting only the
  // inner may cause its bounds to exceed those of the outer, causing scroll
  // clamping.
  gfx::Vector2dF scaled_bounds_delta =
      gfx::ScaleVector2d(bounds_delta, 1.f / min_page_scale_factor());

  property_trees->SetOuterViewportContainerBoundsDelta(scaled_bounds_delta);
  // outer_viewport_container_bounds_delta and
  // inner_viewport_scroll_bounds_delta are the same thing.
  DCHECK_EQ(scaled_bounds_delta,
            property_trees->inner_viewport_scroll_bounds_delta());

  if (auto* outer_clip_node = OuterViewportClipNode()) {
    float adjusted_container_height =
        OuterViewportScrollNode()->container_bounds.height() +
        scaled_bounds_delta.y();
    outer_clip_node->clip.set_height(adjusted_container_height);

    // Expand all clips between the outer viewport and the inner viewport.
    auto* outer_ancestor =
        property_trees->clip_tree_mutable().parent(outer_clip_node);
    while (outer_ancestor && outer_ancestor->id != kRootPropertyNodeId) {
      outer_ancestor->clip.Union(outer_clip_node->clip);
      outer_ancestor =
          property_trees->clip_tree_mutable().parent(outer_ancestor);
    }
  }

  anchor.ResetViewportToAnchoredPosition();

  property_trees->clip_tree_mutable().set_needs_update(true);
  property_trees->set_full_tree_damaged(true);
  set_needs_update_draw_properties();

  // Viewport scrollbar positions are determined using the viewport bounds
  // delta.
  UpdateViewportScrollbarGeometries();
}

bool LayerTreeImpl::IsRootLayer(const LayerImpl* layer) const {
  return !layer_list_.empty() && layer_list_[0].get() == layer;
}

gfx::PointF LayerTreeImpl::TotalScrollOffset() const {
  gfx::Vector2dF offset;
  const auto& scroll_tree = property_trees()->scroll_tree();

  if (auto* inner_scroll = InnerViewportScrollNode()) {
    offset += scroll_tree.current_scroll_offset(inner_scroll->element_id)
                  .OffsetFromOrigin();
    DCHECK(OuterViewportScrollNode());
    offset +=
        scroll_tree.current_scroll_offset(OuterViewportScrollNode()->element_id)
            .OffsetFromOrigin();
  }

  return gfx::PointAtOffsetFromOrigin(offset);
}

gfx::PointF LayerTreeImpl::TotalMaxScrollOffset() const {
  gfx::Vector2dF offset;
  const auto& scroll_tree = property_trees()->scroll_tree();

  if (viewport_property_ids_.inner_scroll != kInvalidPropertyNodeId) {
    offset += scroll_tree.MaxScrollOffset(viewport_property_ids_.inner_scroll)
                  .OffsetFromOrigin();
  }

  if (viewport_property_ids_.outer_scroll != kInvalidPropertyNodeId) {
    offset += scroll_tree.MaxScrollOffset(viewport_property_ids_.outer_scroll)
                  .OffsetFromOrigin();
  }

  return gfx::PointAtOffsetFromOrigin(offset);
}

OwnedLayerImplList LayerTreeImpl::DetachLayers() {
  render_surface_list_.clear();
  set_needs_update_draw_properties();
  OwnedLayerImplList result = std::move(layer_list_);
  // TODO(crbug.com/40778609): remove diagnostic CHECK
  CHECK(!layer_list_.size());
  return result;
}

OwnedLayerImplList LayerTreeImpl::DetachLayersKeepingRootLayerForTesting() {
  auto layers = DetachLayers();
  SetRootLayerForTesting(std::move(layers[0]));
  return layers;
}

void LayerTreeImpl::SetPropertyTrees(PropertyTrees& property_trees,
                                     bool preserve_change_tracking) {
  PropertyTreesChangeState change_state;
  property_trees.GetChangeState(change_state);
  SetPropertyTrees(property_trees, change_state, preserve_change_tracking);
}

void LayerTreeImpl::SetPropertyTrees(const PropertyTrees& property_trees,
                                     PropertyTreesChangeState& change_state,
                                     bool preserve_change_tracking) {
  // Updating the scroll tree shouldn't clobber the currently scrolling node so
  // stash it and restore it at the end of this method.  To maintain the
  // current scrolling node we need to use element ids which are stable across
  // the property tree update in SetPropertyTrees.
  ElementId scrolling_element_id;
  if (IsActiveTree()) {
    if (ScrollNode* scrolling_node = CurrentlyScrollingNode())
      scrolling_element_id = scrolling_node->element_id;
  }

  std::vector<std::unique_ptr<RenderSurfaceImpl>> old_render_surfaces;
  property_trees_.effect_tree_mutable().TakeRenderSurfaces(
      &old_render_surfaces);

  if (preserve_change_tracking) {
    change_state.full_tree_damaged |= property_trees_.full_tree_damaged();
    property_trees_.GetChangedNodes(change_state.changed_effect_nodes,
                                    change_state.changed_transform_nodes);
  }

  property_trees_ = property_trees;

  property_trees_.ApplyChangedNodes(change_state.changed_effect_nodes,
                                    change_state.changed_transform_nodes);
  property_trees_.set_changed(change_state.changed);
  property_trees_.set_needs_rebuild(change_state.needs_rebuild);
  property_trees_.set_full_tree_damaged(change_state.full_tree_damaged);
  property_trees_.effect_tree_mutable().ApplyRenderSurfaceChangedFlags(
      change_state.surface_property_changed_flags);
  bool render_surfaces_changed =
      property_trees_.effect_tree_mutable().CreateOrReuseRenderSurfaces(
          &old_render_surfaces, this);
  if (render_surfaces_changed)
    set_needs_update_draw_properties();
  property_trees_.effect_tree_mutable().PullCopyRequestsFrom(
      change_state.effect_tree_copy_requests);
  property_trees_.set_is_main_thread(false);
  property_trees_.set_is_active(IsActiveTree());
  // The value of some effect node properties (like is_drawn) depends on
  // whether we are on the active tree or not. So, we need to update the
  // effect tree.
  if (IsActiveTree())
    property_trees_.effect_tree_mutable().set_needs_update(true);

  const ScrollNode* scrolling_node = nullptr;
  if (scrolling_element_id) {
    auto& scroll_tree = property_trees_.scroll_tree();
    scrolling_node = scroll_tree.FindNodeFromElementId(scrolling_element_id);
  }
  SetCurrentlyScrollingNode(scrolling_node);
}

void LayerTreeImpl::PullPropertiesFrom(
    CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  lifecycle().AdvanceTo(LayerTreeLifecycle::kBeginningSync);

  if (commit_state.next_commit_forces_redraw)
    ForceRedrawNextActivation();
  if (commit_state.next_commit_forces_recalculate_raster_scales)
    ForceRecalculateRasterScales();
  AddPresentationCallbacks(
      std::move(commit_state.pending_presentation_callbacks));
  AddSuccessfulPresentationCallbacks(
      std::move(commit_state.pending_successful_presentation_callbacks));

  if (commit_state.needs_full_tree_sync)
    TreeSynchronizer::SynchronizeTrees(commit_state, unsafe_state, this);

  if (commit_state.clear_caches_on_next_commit) {
    host_impl_->ClearHistory();
    host_impl_->ClearCaches();
  }

  TRACE_EVENT0("cc", "LayerTreeImpl::PullProperties");

  PullPropertyTreesFrom(commit_state, unsafe_state);
  lifecycle().AdvanceTo(LayerTreeLifecycle::kSyncedPropertyTrees);

  if (commit_state.needs_surface_ranges_sync) {
    ClearSurfaceRanges();
    SetSurfaceRanges(commit_state.SurfaceRanges());
  }
  TreeSynchronizer::PushLayerProperties(commit_state, unsafe_state, this);
  lifecycle().AdvanceTo(LayerTreeLifecycle::kSyncedLayerProperties);

  for (const ElementId& id : commit_state.scrollers_clobbering_active_value) {
    property_trees()->scroll_tree_mutable().SetScrollOffsetClobberActiveValue(
        id);
  }

  // This must happen after synchronizing property trees and after pushing
  // properties,  which updates the clobber_active_value flag (specifically in
  // Layer::PushPropertiesTo).
  // TODO(pdr): Enforce this comment with DCHECKS and a lifecycle state.
  property_trees()->scroll_tree_mutable().PushScrollUpdatesFromMainThread(
      unsafe_state.property_trees, this,
      settings().commit_fractional_scroll_deltas);
  // This must be after scroll updates because the discardable image map may
  // depend on raster-inducing scroll offsets.
  for (auto& layer : picture_layers()) {
    layer->RegenerateDiscardableImageMapIfNeeded();
  }

  PullLayerTreePropertiesFrom(commit_state);

  PassSwapPromises(std::move(commit_state.swap_promises));
  AppendEventsMetricsFromMainThread(std::move(commit_state.event_metrics));

  set_ui_resource_request_queue(commit_state.ui_resource_request_queue);

  // This must happen after synchronizing property trees and after push
  // properties, which updates property tree indices, but before animation
  // host pushes properties as animation host push properties can change
  // KeyframeModel::InEffect and we want the old InEffect value for updating
  // property tree scrolling and animation.
  // TODO(pdr): Enforce this comment with DCHECKS and a lifecycle state.
  UpdatePropertyTreeAnimationFromMainThread();

  TRACE_EVENT0("cc", "LayerTreeHost::AnimationHost::PushProperties");
  DCHECK(mutator_host());
  unsafe_state.mutator_host->PushPropertiesTo(mutator_host(),
                                              unsafe_state.property_trees);

  // Make sure that property tree based changes are moved to layers
  // and draw properties are invalidated.
  MoveChangeTrackingToLayers();

  UpdateAllScrollbarGeometries();

  lifecycle().AdvanceTo(LayerTreeLifecycle::kNotSyncing);
}

void LayerTreeImpl::PullPropertyTreesFrom(
    CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  // Property trees may store damage status. We preserve the sync tree damage
  // status by pushing the damage status from sync tree property trees to main
  // thread property trees or by moving it onto the layers.
  bool preserve_change_tracking = false;
  if (unsafe_state.root_layer && IsActiveTree() && property_trees_.changed()) {
    if (unsafe_state.property_trees.sequence_number() ==
        property_trees_.sequence_number()) {
      preserve_change_tracking = true;
    } else {
      MoveChangeTrackingToLayers();
    }
  }

  SetPropertyTrees(unsafe_state.property_trees,
                   commit_state.property_trees_change_state,
                   preserve_change_tracking);
}

void LayerTreeImpl::PullLayerTreePropertiesFrom(CommitState& commit_state) {
  set_needs_full_tree_sync(commit_state.needs_full_tree_sync);

  if (commit_state.hud_layer_id != Layer::INVALID_ID) {
    LayerImpl* hud_impl = LayerById(commit_state.hud_layer_id);
    set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    set_hud_layer(nullptr);
  }

  set_background_color(commit_state.background_color);
  set_have_scroll_event_handlers(commit_state.have_scroll_event_handlers);
  set_event_listener_properties(EventListenerClass::kTouchStartOrMove,
                                commit_state.GetEventListenerProperties(
                                    EventListenerClass::kTouchStartOrMove));
  set_event_listener_properties(
      EventListenerClass::kMouseWheel,
      commit_state.GetEventListenerProperties(EventListenerClass::kMouseWheel));
  set_event_listener_properties(EventListenerClass::kTouchEndOrCancel,
                                commit_state.GetEventListenerProperties(
                                    EventListenerClass::kTouchEndOrCancel));

  SetViewportPropertyIds(commit_state.viewport_property_ids);

  RegisterSelection(commit_state.selection);

  PushPageScaleFromMainThread(commit_state.page_scale_factor,
                              commit_state.min_page_scale_factor,
                              commit_state.max_page_scale_factor);

  SetBrowserControlsParams(commit_state.browser_controls_params);
  set_overscroll_behavior(commit_state.overscroll_behavior);
  PushBrowserControlsFromMainThread(commit_state.top_controls_shown_ratio,
                                    commit_state.bottom_controls_shown_ratio);
  elastic_overscroll()->PushMainToPending(commit_state.elastic_overscroll);
  if (IsActiveTree())
    elastic_overscroll()->PushPendingToActive();

  SetDisplayColorSpaces(commit_state.display_color_spaces);
  SetExternalPageScaleFactor(commit_state.external_page_scale_factor);

  set_painted_device_scale_factor(commit_state.painted_device_scale_factor);
  SetDeviceScaleFactor(commit_state.device_scale_factor);
  SetDeviceViewportRect(commit_state.device_viewport_rect);

  if (commit_state.new_local_surface_id_request)
    RequestNewLocalSurfaceId();

  if (!commit_state.screenshot_destination_token.is_empty()) {
    SetScreenshotDestinationToken(commit_state.screenshot_destination_token);
  }

  set_primary_main_frame_item_sequence_number(
      commit_state.primary_main_frame_item_sequence_number);

  SetLocalSurfaceIdFromParent(commit_state.local_surface_id_from_parent);

  if (commit_state.pending_page_scale_animation) {
    SetPendingPageScaleAnimation(
        std::move(commit_state.pending_page_scale_animation));
  }

  if (commit_state.force_send_metadata_request)
    RequestForceSendMetadata();

  set_display_transform_hint(commit_state.display_transform_hint);

  if (commit_state.delegated_ink_metadata)
    set_delegated_ink_metadata(std::move(commit_state.delegated_ink_metadata));
  else
    delegated_ink_metadata_.reset();

  // Transfer page transition directives.
  for (auto& request : commit_state.view_transition_requests)
    AddViewTransitionRequest(std::move(request));
}

void LayerTreeImpl::PushPropertyTreesTo(LayerTreeImpl* target_tree) {
  TRACE_EVENT0("cc", "LayerTreeImpl::PushPropertyTreesTo");
  // Property trees may store damage status. We preserve the active tree
  // damage status by pushing the damage status from active tree property
  // trees to pending tree property trees or by moving it onto the layers.
  bool preserve_change_tracking = false;
  if (target_tree->property_trees()->changed()) {
    if (property_trees()->sequence_number() ==
        target_tree->property_trees()->sequence_number()) {
      preserve_change_tracking = true;
    } else {
      target_tree->MoveChangeTrackingToLayers();
    }
  }

  target_tree->SetPropertyTrees(property_trees_, preserve_change_tracking);

  EventMetrics::List events_metrics;
  events_metrics.swap(events_metrics_from_main_thread_);
  target_tree->AppendEventsMetricsFromMainThread(std::move(events_metrics));
}

void LayerTreeImpl::PushSurfaceRangesTo(LayerTreeImpl* target_tree) {
  if (needs_surface_ranges_sync()) {
    target_tree->ClearSurfaceRanges();
    target_tree->SetSurfaceRanges(SurfaceRanges());
    // Reset for next update
    set_needs_surface_ranges_sync(false);
  }
}

void LayerTreeImpl::PushPropertiesTo(LayerTreeImpl* target_tree) {
  TRACE_EVENT0("cc", "LayerTreeImpl::PushPropertiesTo");
  // The request queue should have been processed and does not require a push.
  DCHECK_EQ(ui_resource_request_queue_.size(), 0u);

  PushSurfaceRangesTo(target_tree);
  target_tree->property_trees()
      ->scroll_tree_mutable()
      .PushScrollUpdatesFromPendingTree(&property_trees_, target_tree);

  if (next_activation_forces_redraw_) {
    target_tree->ForceRedrawNextActivation();
    next_activation_forces_redraw_ = false;
  }

  target_tree->PassSwapPromises(std::move(swap_promise_list_));
  swap_promise_list_.clear();

  // The page scale factor update can affect scrolling which requires that
  // these ids are set, so this must be before PushPageScaleFactorAndLimits.
  // Setting browser controls below also needs viewport scroll properties.
  target_tree->SetViewportPropertyIds(viewport_property_ids_);

  // Active tree already shares the page_scale_factor object with pending
  // tree so only the limits need to be provided.
  target_tree->PushPageScaleFactorAndLimits(nullptr, min_page_scale_factor(),
                                            max_page_scale_factor());
  target_tree->SetExternalPageScaleFactor(external_page_scale_factor_);

  target_tree->SetBrowserControlsParams(browser_controls_params_);
  target_tree->PushBrowserControls(nullptr, nullptr);

  target_tree->set_overscroll_behavior(overscroll_behavior_);

  target_tree->SetDisplayColorSpaces(display_color_spaces_);
  target_tree->elastic_overscroll()->PushPendingToActive();

  target_tree->set_painted_device_scale_factor(painted_device_scale_factor());
  target_tree->SetDeviceScaleFactor(device_scale_factor());
  target_tree->SetDeviceViewportRect(device_viewport_rect_);

  if (TakeNewLocalSurfaceIdRequest())
    target_tree->RequestNewLocalSurfaceId();
  target_tree->SetLocalSurfaceIdFromParent(local_surface_id_from_parent());

  if (auto token = TakeScreenshotDestinationToken(); !token.is_empty()) {
    target_tree->SetScreenshotDestinationToken(std::move(token));
  }

  target_tree->set_primary_main_frame_item_sequence_number(
      primary_main_frame_item_sequence_number());

  target_tree->pending_page_scale_animation_ =
      std::move(pending_page_scale_animation_);

  if (TakeForceSendMetadataRequest())
    target_tree->RequestForceSendMetadata();

  target_tree->RegisterSelection(selection_);

  // This should match the property synchronization in
  // LayerTreeHost::finishCommitOnImplThread().
  target_tree->set_source_frame_number(source_frame_number());
  target_tree->set_trace_id(trace_id());
  target_tree->set_background_color(background_color());
  target_tree->set_have_scroll_event_handlers(have_scroll_event_handlers());
  target_tree->set_event_listener_properties(
      EventListenerClass::kTouchStartOrMove,
      event_listener_properties(EventListenerClass::kTouchStartOrMove));
  target_tree->set_event_listener_properties(
      EventListenerClass::kMouseWheel,
      event_listener_properties(EventListenerClass::kMouseWheel));
  target_tree->set_event_listener_properties(
      EventListenerClass::kTouchEndOrCancel,
      event_listener_properties(EventListenerClass::kTouchEndOrCancel));

  if (hud_layer())
    target_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(
        target_tree->LayerById(hud_layer()->id())));
  else
    target_tree->set_hud_layer(nullptr);

  // Note: this needs to happen after SetPropertyTrees.
  target_tree->HandleTickmarksVisibilityChange();
  target_tree->UpdateAllScrollbarGeometries();
  target_tree->HandleScrollbarShowRequests();
  target_tree->AddPresentationCallbacks(std::move(presentation_callbacks_));
  presentation_callbacks_.clear();
  target_tree->AddSuccessfulPresentationCallbacks(
      std::move(successful_presentation_callbacks_));
  successful_presentation_callbacks_.clear();

  if (delegated_ink_metadata_) {
    TRACE_EVENT_WITH_FLOW1("delegated_ink_trails",
                           "Delegated ink metadata pushed to tree",
                           TRACE_ID_GLOBAL(delegated_ink_metadata_->trace_id()),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "metadata", delegated_ink_metadata_->ToString());
    target_tree->set_delegated_ink_metadata(std::move(delegated_ink_metadata_));
  } else if (target_tree->delegated_ink_metadata()) {
    target_tree->clear_delegated_ink_metadata();
  }

  for (auto& request : TakeViewTransitionRequests())
    target_tree->AddViewTransitionRequest(std::move(request));

  // Make sure that property tree based changes are moved to layers
  // and draw properties are invalidated.
  target_tree->MoveChangeTrackingToLayers();
}

void LayerTreeImpl::HandleTickmarksVisibilityChange() {
  if (!host_impl_->OuterViewportScrollNode())
    return;

  ScrollbarAnimationController* controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          host_impl_->OuterViewportScrollNode()->element_id);

  if (!controller)
    return;

  for (ScrollbarLayerImplBase* scrollbar : controller->Scrollbars()) {
    if (scrollbar->orientation() != ScrollbarOrientation::kVertical) {
      continue;
    }

    // Android Overlay Scrollbar don't have FindInPage Tickmarks.
    if (scrollbar->GetScrollbarAnimator() != LayerTreeSettings::AURA_OVERLAY)
      DCHECK(!scrollbar->has_find_in_page_tickmarks());

    controller->UpdateTickmarksVisibility(
        scrollbar->has_find_in_page_tickmarks());
  }
}

void LayerTreeImpl::RequestShowScrollbars(ElementId scroll_element_id) {
  if (IsActiveTree()) {
    show_scrollbar_requests_.insert(scroll_element_id);
  }
}

void LayerTreeImpl::HandleScrollbarShowRequests() {
  for (ElementId scroll_element_id : show_scrollbar_requests_) {
    if (ScrollbarAnimationController* controller =
            host_impl_->ScrollbarAnimationControllerForElementId(
                scroll_element_id)) {
      controller->DidRequestShow();
    }
  }
  show_scrollbar_requests_.clear();
}

void LayerTreeImpl::MoveChangeTrackingToLayers() {
  // We need to update the change tracking on property trees before we move it
  // onto the layers.
  property_trees_.UpdateChangeTracking();
  for (auto* layer : *this) {
    if (layer->LayerPropertyChanged()) {
      if (layer->LayerPropertyChangedFromPropertyTrees()) {
        layer->NoteLayerPropertyChangedFromPropertyTrees();
      }
    }
  }
  EffectTree& effect_tree = property_trees_.effect_tree_mutable();
  for (int id = kContentsRootPropertyNodeId;
       id < static_cast<int>(effect_tree.size()); ++id) {
    RenderSurfaceImpl* render_surface = effect_tree.GetRenderSurface(id);
    if (render_surface && render_surface->AncestorPropertyChanged())
      render_surface->NoteAncestorPropertyChanged();
  }
}

void LayerTreeImpl::ForceRecalculateRasterScales() {
  for (PictureLayerImpl* layer : picture_layers_) {
    layer->ResetRasterScale();
  }
}

bool LayerTreeImpl::IsElementInPropertyTree(ElementId element_id) const {
  return property_trees()->HasElement(element_id);
}

ElementListType LayerTreeImpl::GetElementTypeForAnimation() const {
  return IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
}

void LayerTreeImpl::SetTransformMutated(ElementId element_id,
                                        const gfx::Transform& transform) {
  DCHECK_EQ(1u,
            property_trees()->transform_tree().element_id_to_node_index().count(
                element_id));
  if (!base::FeatureList::IsEnabled(features::kNoPreserveLastMutation) &&
      (IsSyncTree() || IsRecycleTree())) {
    element_id_to_transform_animations_[element_id] = transform;
  }
  if (property_trees()->transform_tree_mutable().OnTransformAnimated(element_id,
                                                                     transform))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::SetOpacityMutated(ElementId element_id, float opacity) {
  DCHECK_EQ(1u,
            property_trees()->effect_tree().element_id_to_node_index().count(
                element_id));
  if (!base::FeatureList::IsEnabled(features::kNoPreserveLastMutation) &&
      (IsSyncTree() || IsRecycleTree())) {
    element_id_to_opacity_animations_[element_id] = opacity;
  }
  if (property_trees()->effect_tree_mutable().OnOpacityAnimated(element_id,
                                                                opacity))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::SetFilterMutated(ElementId element_id,
                                     const FilterOperations& filters) {
  DCHECK_EQ(1u,
            property_trees()->effect_tree().element_id_to_node_index().count(
                element_id));
  if (!base::FeatureList::IsEnabled(features::kNoPreserveLastMutation) &&
      (IsSyncTree() || IsRecycleTree())) {
    element_id_to_filter_animations_[element_id] = filters;
  }
  if (property_trees()->effect_tree_mutable().OnFilterAnimated(element_id,
                                                               filters))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::SetBackdropFilterMutated(
    ElementId element_id,
    const FilterOperations& backdrop_filters) {
  DCHECK_EQ(1u,
            property_trees()->effect_tree().element_id_to_node_index().count(
                element_id));
  if (!base::FeatureList::IsEnabled(features::kNoPreserveLastMutation) &&
      (IsSyncTree() || IsRecycleTree())) {
    element_id_to_backdrop_filter_animations_[element_id] = backdrop_filters;
  }
  if (property_trees()->effect_tree_mutable().OnBackdropFilterAnimated(
          element_id, backdrop_filters))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::AddPresentationCallbacks(
    std::vector<PresentationTimeCallbackBuffer::Callback> callbacks) {
  base::ranges::move(callbacks, std::back_inserter(presentation_callbacks_));
}

std::vector<PresentationTimeCallbackBuffer::Callback>
LayerTreeImpl::TakePresentationCallbacks() {
  std::vector<PresentationTimeCallbackBuffer::Callback> callbacks;
  callbacks.swap(presentation_callbacks_);
  return callbacks;
}

void LayerTreeImpl::AddSuccessfulPresentationCallbacks(
    std::vector<PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
        callbacks) {
  base::ranges::move(callbacks,
                     std::back_inserter(successful_presentation_callbacks_));
}

std::vector<PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
LayerTreeImpl::TakeSuccessfulPresentationCallbacks() {
  std::vector<PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
      callbacks;
  callbacks.swap(successful_presentation_callbacks_);
  return callbacks;
}

LayerImpl* LayerTreeImpl::InnerViewportScrollLayerForTesting() const {
  if (auto* scroll_node = InnerViewportScrollNode())
    return LayerByElementId(scroll_node->element_id);
  return nullptr;
}

LayerImpl* LayerTreeImpl::OuterViewportScrollLayerForTesting() const {
  if (auto* scroll_node = OuterViewportScrollNode())
    return LayerByElementId(scroll_node->element_id);
  return nullptr;
}

ScrollNode* LayerTreeImpl::CurrentlyScrollingNode() {
  DCHECK(IsActiveTree());
  return property_trees_.scroll_tree_mutable().CurrentlyScrollingNode();
}

const ScrollNode* LayerTreeImpl::CurrentlyScrollingNode() const {
  return property_trees_.scroll_tree().CurrentlyScrollingNode();
}

int LayerTreeImpl::LastScrolledScrollNodeIndex() const {
  return last_scrolled_scroll_node_index_;
}

void LayerTreeImpl::SetCurrentlyScrollingNode(const ScrollNode* node) {
  if (node)
    last_scrolled_scroll_node_index_ = node->id;

  ScrollTree& scroll_tree = property_trees()->scroll_tree_mutable();
  ScrollNode* old_node = scroll_tree.CurrentlyScrollingNode();

  ElementId old_element_id = old_node ? old_node->element_id : ElementId();
  ElementId new_element_id = node ? node->element_id : ElementId();
  if (old_element_id == new_element_id)
    return;

  scroll_tree.set_currently_scrolling_node(node ? node->id
                                                : kInvalidPropertyNodeId);
}

void LayerTreeImpl::ClearCurrentlyScrollingNode() {
  SetCurrentlyScrollingNode(nullptr);
}

float LayerTreeImpl::ClampPageScaleFactorToLimits(
    float page_scale_factor) const {
  if (min_page_scale_factor_ && page_scale_factor < min_page_scale_factor_)
    page_scale_factor = min_page_scale_factor_;
  else if (max_page_scale_factor_ && page_scale_factor > max_page_scale_factor_)
    page_scale_factor = max_page_scale_factor_;
  return page_scale_factor;
}

void LayerTreeImpl::UpdatePropertyTreeAnimationFromMainThread() {
  // TODO(enne): This should get replaced by pulling out animations into their
  // own trees.  Then animations would have their own ways of synchronizing
  // across commits.  This occurs to push updates from animations that have
  // ticked since begin frame to a newly-committed property tree.
  if (layer_list_.empty())
    return;

  // Note we lazily delete element ids from the |element_id_to_xxx|
  // maps below if we find they have no node present in their
  // respective tree. This can be the case if the layer associated
  // with that element id has been removed.

  // This code is assumed to only run on the sync tree; the node updates are
  // then synced when the tree is activated. See http://crbug.com/916512
  DCHECK(IsSyncTree());
  auto& transform_tree = property_trees_.transform_tree_mutable();

  if (!base::FeatureList::IsEnabled(features::kNoPreserveLastMutation)) {
    auto& effect_tree = property_trees_.effect_tree_mutable();
    auto element_id_to_opacity = element_id_to_opacity_animations_.begin();
    while (element_id_to_opacity != element_id_to_opacity_animations_.end()) {
      const ElementId id = element_id_to_opacity->first;
      EffectNode* node = effect_tree.FindNodeFromElementId(id);
      if (!node || !node->is_currently_animating_opacity ||
          node->opacity == element_id_to_opacity->second) {
        element_id_to_opacity_animations_.erase(element_id_to_opacity++);
        continue;
      }
      node->opacity = element_id_to_opacity->second;
      effect_tree.set_needs_update(true);
      ++element_id_to_opacity;
    }

    auto element_id_to_filter = element_id_to_filter_animations_.begin();
    while (element_id_to_filter != element_id_to_filter_animations_.end()) {
      const ElementId id = element_id_to_filter->first;
      EffectNode* node = effect_tree.FindNodeFromElementId(id);
      if (!node || !node->is_currently_animating_filter ||
          node->filters == element_id_to_filter->second) {
        element_id_to_filter_animations_.erase(element_id_to_filter++);
        continue;
      }
      node->filters = element_id_to_filter->second;
      effect_tree.set_needs_update(true);
      ++element_id_to_filter;
    }

    auto element_id_to_backdrop_filter =
        element_id_to_backdrop_filter_animations_.begin();
    while (element_id_to_backdrop_filter !=
           element_id_to_backdrop_filter_animations_.end()) {
      const ElementId id = element_id_to_backdrop_filter->first;
      EffectNode* node = effect_tree.FindNodeFromElementId(id);
      if (!node || !node->is_currently_animating_backdrop_filter ||
          node->backdrop_filters == element_id_to_backdrop_filter->second) {
        element_id_to_backdrop_filter_animations_.erase(
            element_id_to_backdrop_filter++);
        continue;
      }
      node->backdrop_filters = element_id_to_backdrop_filter->second;
      effect_tree.set_needs_update(true);
      ++element_id_to_backdrop_filter;
    }

    auto element_id_to_transform = element_id_to_transform_animations_.begin();
    while (element_id_to_transform !=
           element_id_to_transform_animations_.end()) {
      const ElementId id = element_id_to_transform->first;
      TransformNode* node = transform_tree.FindNodeFromElementId(id);
      if (!node || !node->is_currently_animating ||
          node->local == element_id_to_transform->second) {
        element_id_to_transform_animations_.erase(element_id_to_transform++);
        continue;
      }
      node->local = element_id_to_transform->second;
      node->needs_local_transform_update = true;
      transform_tree.set_needs_update(true);
      ++element_id_to_transform;
    }
  }

  for (auto iter : transform_tree.element_id_to_node_index())
    UpdateTransformAnimation(iter.first, iter.second);
}

void LayerTreeImpl::UpdateTransformAnimation(ElementId element_id,
                                             int transform_node_index) {
  // This includes all animations, even those that are finished but
  // haven't yet been deleted.

  // A given ElementId should be associated with only a single transform
  // property.  However, the ElementId is opaque to cc.  (If it comes from
  // blink, it was constructed with a CompositorElementIdNamespace specific to
  // the correct property.  Otherwise, only the transform property should be
  // used.)
  const TargetProperty::Type transform_properties[] = {
      TargetProperty::TRANSFORM, TargetProperty::SCALE, TargetProperty::ROTATE,
      TargetProperty::TRANSLATE};
#if DCHECK_IS_ON()
  unsigned property_count = 0u;
#endif

  for (TargetProperty::Type property : transform_properties) {
    if (mutator_host()->HasAnyAnimationTargetingProperty(element_id,
                                                         property)) {
#if DCHECK_IS_ON()
      ++property_count;
#endif
      TransformTree& transform_tree =
          property_trees()->transform_tree_mutable();
      if (TransformNode* node = transform_tree.Node(transform_node_index)) {
        ElementListType list_type = GetElementTypeForAnimation();
        bool has_potential_animation =
            mutator_host()->HasPotentiallyRunningAnimationForProperty(
                element_id, list_type, property);
        if (node->has_potential_animation != has_potential_animation) {
          node->has_potential_animation = has_potential_animation;
          node->maximum_animation_scale =
              mutator_host()->MaximumScale(element_id, list_type);
          transform_tree.set_needs_update(true);
          set_needs_update_draw_properties();
        }
      }
    }
  }
#if DCHECK_IS_ON()
  DCHECK_LE(property_count, 1u);
#endif
}

void LayerTreeImpl::UpdatePageScaleNode() {
  if (!PageScaleTransformNode()) {
    DCHECK(layer_list_.empty() || current_page_scale_factor() == 1);
    return;
  }
  draw_property_utils::UpdatePageScaleFactor(
      property_trees(), PageScaleTransformNode(), current_page_scale_factor());
}

void LayerTreeImpl::SetPageScaleOnActiveTree(float active_page_scale) {
  DCHECK(IsActiveTree());
  DCHECK(lifecycle().AllowsPropertyTreeAccess());
  float clamped_page_scale = ClampPageScaleFactorToLimits(active_page_scale);
  // Temporary crash logging for https://crbug.com/845097.
  static bool has_dumped_without_crashing = false;
  if (!host_impl_->settings().is_for_scalable_page &&
      clamped_page_scale != 1.f && !has_dumped_without_crashing) {
    has_dumped_without_crashing = true;
    static auto* psf_oopif_error = base::debug::AllocateCrashKeyString(
        "psf_oopif_error", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        psf_oopif_error, base::StringPrintf("%f", clamped_page_scale));
    base::debug::DumpWithoutCrashing();
  }
  if (page_scale_factor()->SetCurrent(clamped_page_scale))
    DidUpdatePageScale();
}

void LayerTreeImpl::PushPageScaleFromMainThread(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  PushPageScaleFactorAndLimits(&page_scale_factor, min_page_scale_factor,
                               max_page_scale_factor);
}

void LayerTreeImpl::PushPageScaleFactorAndLimits(const float* page_scale_factor,
                                                 float min_page_scale_factor,
                                                 float max_page_scale_factor) {
  DCHECK(page_scale_factor || IsActiveTree());
  bool changed_page_scale = false;

  changed_page_scale |=
      SetPageScaleFactorLimits(min_page_scale_factor, max_page_scale_factor);

  if (page_scale_factor) {
    DCHECK(!IsActiveTree() || !host_impl_->pending_tree());
    changed_page_scale |=
        page_scale_factor_->PushMainToPending(*page_scale_factor);
  }

  if (IsActiveTree()) {
    changed_page_scale |= page_scale_factor_->PushPendingToActive();
  }

  if (changed_page_scale)
    DidUpdatePageScale();
}

void LayerTreeImpl::SetBrowserControlsParams(
    const BrowserControlsParams& params) {
  if (browser_controls_params_ == params)
    return;

  browser_controls_params_ = params;
  UpdateViewportContainerSizes();

  if (IsActiveTree()) {
    host_impl_->browser_controls_manager()->OnBrowserControlsParamsChanged(
        params.animate_browser_controls_height_changes);
  }
}

void LayerTreeImpl::set_overscroll_behavior(
    const OverscrollBehavior& behavior) {
  overscroll_behavior_ = behavior;
}

bool LayerTreeImpl::ClampTopControlsShownRatio() {
  float ratio = top_controls_shown_ratio_->Current(true);
  auto range = std::make_pair(0.f, 1.f);
  if (IsActiveTree()) {
    // BCOM might need to set ratios outside the [0, 1] range (e.g. animation
    // running). So, use the values it provides instead of clamping to [0, 1].
    range =
        host_impl_->browser_controls_manager()->TopControlsShownRatioRange();
  }
  return top_controls_shown_ratio_->SetCurrent(
      std::clamp(ratio, range.first, range.second));
}

bool LayerTreeImpl::ClampBottomControlsShownRatio() {
  float ratio = bottom_controls_shown_ratio_->Current(true);
  auto range = std::make_pair(0.f, 1.f);
  if (IsActiveTree()) {
    // BCOM might need to set ratios outside the [0, 1] range (e.g. animation
    // running). So, use the values it provides instead of clamping to [0, 1].
    range =
        host_impl_->browser_controls_manager()->BottomControlsShownRatioRange();
  }
  return bottom_controls_shown_ratio_->SetCurrent(
      std::clamp(ratio, range.first, range.second));
}

bool LayerTreeImpl::SetCurrentBrowserControlsShownRatio(float top_ratio,
                                                        float bottom_ratio) {
  TRACE_EVENT2("cc", "LayerTreeImpl::SetCurrentBrowserControlsShownRatio",
               "top_ratio", top_ratio, "bottom_ratio", bottom_ratio);
  bool changed = top_controls_shown_ratio_->SetCurrent(top_ratio);
  changed |= ClampTopControlsShownRatio();
  changed |= bottom_controls_shown_ratio_->SetCurrent(bottom_ratio);
  changed |= ClampBottomControlsShownRatio();
  return changed;
}

void LayerTreeImpl::PushBrowserControlsFromMainThread(
    float top_controls_shown_ratio,
    float bottom_controls_shown_ratio) {
  PushBrowserControls(&top_controls_shown_ratio, &bottom_controls_shown_ratio);
}

void LayerTreeImpl::PushBrowserControls(
    const float* top_controls_shown_ratio,
    const float* bottom_controls_shown_ratio) {
  DCHECK(top_controls_shown_ratio || bottom_controls_shown_ratio ||
         IsActiveTree());
  DCHECK(!top_controls_shown_ratio || bottom_controls_shown_ratio);
  DCHECK(top_controls_shown_ratio || IsActiveTree());

  if (top_controls_shown_ratio) {
    DCHECK(!IsActiveTree() || !host_impl_->pending_tree());
    top_controls_shown_ratio_->PushMainToPending(*top_controls_shown_ratio);
    bottom_controls_shown_ratio_->PushMainToPending(
        *bottom_controls_shown_ratio);
    if (!IsActiveTree())
      UpdateViewportContainerSizes();
  }
  if (IsActiveTree()) {
    bool changed_active = top_controls_shown_ratio_->PushPendingToActive();
    changed_active |= ClampTopControlsShownRatio();
    changed_active |= bottom_controls_shown_ratio_->PushPendingToActive();
    changed_active |= ClampBottomControlsShownRatio();
    if (changed_active)
      host_impl_->DidChangeBrowserControlsPosition();
  }
}

bool LayerTreeImpl::SetPageScaleFactorLimits(float min_page_scale_factor,
                                             float max_page_scale_factor) {
  if (min_page_scale_factor == min_page_scale_factor_ &&
      max_page_scale_factor == max_page_scale_factor_)
    return false;

  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;

  return true;
}

void LayerTreeImpl::DidUpdatePageScale() {
  if (IsActiveTree()) {
    page_scale_factor()->SetCurrent(
        ClampPageScaleFactorToLimits(current_page_scale_factor()));

    // Ensure the other trees are kept in sync.
    if (host_impl_->pending_tree())
      host_impl_->pending_tree()->DidUpdatePageScale();
    if (host_impl_->recycle_tree())
      host_impl_->recycle_tree()->DidUpdatePageScale();

    if (settings().scrollbar_flash_after_any_scroll_update) {
      host_impl_->FlashAllScrollbars(true);
    } else if (auto* scroll_node = host_impl_->OuterViewportScrollNode()) {
      if (ScrollbarAnimationController* controller =
              host_impl_->ScrollbarAnimationControllerForElementId(
                  scroll_node->element_id))
        controller->DidScrollUpdate();
    }
  }

  DCHECK(lifecycle().AllowsPropertyTreeAccess());
  UpdatePageScaleNode();

  set_needs_update_draw_properties();

  // Viewport scrollbar sizes depend on the page scale factor.
  UpdateViewportScrollbarGeometries();
}

void LayerTreeImpl::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  set_needs_update_draw_properties();
  if (IsActiveTree())
    host_impl_->SetViewportDamage(GetDeviceViewport());
}

void LayerTreeImpl::SetLocalSurfaceIdFromParent(
    const viz::LocalSurfaceId& local_surface_id_from_parent) {
  local_surface_id_from_parent_ = local_surface_id_from_parent;
}

void LayerTreeImpl::RequestNewLocalSurfaceId() {
  new_local_surface_id_request_ = true;
}

bool LayerTreeImpl::TakeNewLocalSurfaceIdRequest() {
  bool new_local_surface_id_request = new_local_surface_id_request_;
  new_local_surface_id_request_ = false;
  return new_local_surface_id_request;
}

void LayerTreeImpl::SetScreenshotDestinationToken(
    base::UnguessableToken destination_token) {
  screenshot_destination_ = std::move(destination_token);
}

base::UnguessableToken LayerTreeImpl::TakeScreenshotDestinationToken() {
  base::UnguessableToken token = std::move(screenshot_destination_);
  screenshot_destination_ = base::UnguessableToken();
  return token;
}

void LayerTreeImpl::SetDeviceViewportRect(
    const gfx::Rect& device_viewport_rect) {
  if (device_viewport_rect == device_viewport_rect_)
    return;
  device_viewport_rect_ = device_viewport_rect;
  device_viewport_rect_changed_ = true;

  set_needs_update_draw_properties();
  if (!IsActiveTree())
    return;

  UpdateViewportContainerSizes();
  host_impl_->OnCanDrawStateChangedForTree();
  host_impl_->SetViewportDamage(GetDeviceViewport());
}

gfx::Rect LayerTreeImpl::GetDeviceViewport() const {
  // TODO(fsamuel): We should plumb |external_viewport| similar to the
  // way we plumb |device_viewport_rect_|.
  const gfx::Rect& external_viewport = host_impl_->external_viewport();
  if (external_viewport.IsEmpty())
    return device_viewport_rect_;
  return external_viewport;
}

void LayerTreeImpl::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  if (display_color_spaces_ == display_color_spaces)
    return;
  display_color_spaces_ = display_color_spaces;
}

void LayerTreeImpl::SetExternalPageScaleFactor(
    float external_page_scale_factor) {
  if (external_page_scale_factor_ == external_page_scale_factor)
    return;

  external_page_scale_factor_ = external_page_scale_factor;
  DidUpdatePageScale();
}

SyncedScale* LayerTreeImpl::page_scale_factor() {
  return page_scale_factor_.get();
}

const SyncedScale* LayerTreeImpl::page_scale_factor() const {
  return page_scale_factor_.get();
}

gfx::SizeF LayerTreeImpl::ScrollableViewportSize() const {
  if (!InnerViewportScrollNode())
    return gfx::SizeF();

  gfx::Size container_bounds = property_trees()->scroll_tree().container_bounds(
      viewport_property_ids_.inner_scroll);
  return gfx::ScaleSize(gfx::SizeF(container_bounds),
                        1.0f / page_scale_factor_for_scroll());
}

gfx::Rect LayerTreeImpl::RootScrollLayerDeviceViewportBounds() const {
  const ScrollNode* root_scroll_node = OuterViewportScrollNode();
  if (!root_scroll_node) {
    DCHECK(!InnerViewportScrollNode());
    return gfx::Rect();
  }
  return MathUtil::MapEnclosingClippedRect(
      property_trees()->transform_tree().ToScreen(
          root_scroll_node->transform_id),
      gfx::Rect(root_scroll_node->bounds));
}

void LayerTreeImpl::ApplySentScrollAndScaleDeltasFromAbortedCommit(
    bool next_bmf,
    bool main_frame_applied_deltas) {
  DCHECK(IsActiveTree());

  page_scale_factor()->AbortCommit(next_bmf, main_frame_applied_deltas);
  top_controls_shown_ratio()->AbortCommit(next_bmf, main_frame_applied_deltas);
  bottom_controls_shown_ratio()->AbortCommit(next_bmf,
                                             main_frame_applied_deltas);
  elastic_overscroll()->AbortCommit(next_bmf, main_frame_applied_deltas);

  if (layer_list_.empty())
    return;

  property_trees()
      ->scroll_tree_mutable()
      .ApplySentScrollDeltasFromAbortedCommit(next_bmf,
                                              main_frame_applied_deltas);
}

void LayerTreeImpl::SetViewportPropertyIds(const ViewportPropertyIds& ids) {
  viewport_property_ids_ = ids;
  // Outer viewport properties exist only if inner viewport property exists.
  DCHECK(ids.inner_scroll != kInvalidPropertyNodeId ||
         (ids.outer_scroll == kInvalidPropertyNodeId &&
          ids.outer_clip == kInvalidPropertyNodeId));

  if (auto* inner_scroll = InnerViewportScrollNode()) {
    if (auto* inner_scroll_layer = LayerByElementId(inner_scroll->element_id))
      inner_scroll_layer->set_is_inner_viewport_scroll_layer();
  }
}

const TransformNode* LayerTreeImpl::OverscrollElasticityTransformNode() const {
  return property_trees()->transform_tree().Node(
      viewport_property_ids_.overscroll_elasticity_transform);
}

const TransformNode* LayerTreeImpl::PageScaleTransformNode() const {
  return property_trees()->transform_tree().Node(
      viewport_property_ids_.page_scale_transform);
}

const ScrollNode* LayerTreeImpl::InnerViewportScrollNode() const {
  return property_trees()->scroll_tree().Node(
      viewport_property_ids_.inner_scroll);
}

const ClipNode* LayerTreeImpl::OuterViewportClipNode() const {
  return property_trees()->clip_tree().Node(viewport_property_ids_.outer_clip);
}

const ScrollNode* LayerTreeImpl::OuterViewportScrollNode() const {
  return property_trees()->scroll_tree().Node(
      viewport_property_ids_.outer_scroll);
}

// For unit tests, we use the layer's id as its element id.
void LayerTreeImpl::SetElementIdsForTesting() {
  for (auto* layer : *this) {
    if (!layer->element_id())
      layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
  }
}

bool LayerTreeImpl::UpdateDrawProperties(
    bool update_tiles,
    bool update_image_animation_controller,
    LayerImplList* output_update_layer_list_for_testing) {
  if (!needs_update_draw_properties_)
    return true;

  TRACE_EVENT0("cc,benchmark", "LayerTreeImpl::UpdateDrawProperties");

  // Calling UpdateDrawProperties must clear this flag, so there can be no
  // early outs before this.
  needs_update_draw_properties_ = false;
  needs_update_tiles_ = false;

  // For max_texture_size. When a new output surface is received the needs
  // update draw properties flag is set again.
  if (!host_impl_->layer_tree_frame_sink())
    return false;

  // Clear this after the renderer early out, as it should still be
  // possible to hit test even without a renderer.
  render_surface_list_.clear();

  if (layer_list_.empty())
    return false;

  {
    base::ElapsedTimer timer;
    TRACE_EVENT2("cc,benchmark",
                 "LayerTreeImpl::UpdateDrawProperties::CalculateDrawProperties",
                 "IsActive", IsActiveTree(), "SourceFrameNumber",
                 source_frame_number_);
    // We verify visible rect calculations whenever we verify clip tree
    // calculations except when this function is explicitly passed a flag asking
    // us to skip it.
    draw_property_utils::CalculateDrawProperties(
        this, &render_surface_list_, output_update_layer_list_for_testing);

    if (settings().single_thread_proxy_scheduler) {
      // This metric is only recorded for the Browser.
      UMA_HISTOGRAM_COUNTS_1M(
          "Compositing.Browser.LayerTreeImpl.CalculateDrawPropertiesUs",
          timer.Elapsed().InMicroseconds());
    } else {
      // This metric is only recorded for the Renderer.
      UMA_HISTOGRAM_COUNTS_100(
          "Compositing.Renderer.NumRenderSurfaces",
          base::saturated_cast<int>(render_surface_list_.size()));
    }
  }

  TRACE_EVENT2("cc,benchmark", "LayerTreeImpl::UpdateDrawProperties::Occlusion",
               "IsActive", IsActiveTree(), "SourceFrameNumber",
               source_frame_number_);
  OcclusionTracker occlusion_tracker(RootRenderSurface()->content_rect());
  occlusion_tracker.set_minimum_tracking_size(
      settings().minimum_occlusion_tracking_size);

  for (EffectTreeLayerListIterator it(this);
       it.state() != EffectTreeLayerListIterator::State::kEnd; ++it) {
    occlusion_tracker.EnterLayer(it);

    if (it.state() == EffectTreeLayerListIterator::State::kLayer) {
      LayerImpl* layer = it.current_layer();
      layer->draw_properties().occlusion_in_content_space =
          occlusion_tracker.GetCurrentOcclusionForLayer(layer->DrawTransform());
    }

    // TODO(khushalsagar) : Skip computing occlusion for shared elements. See
    // crbug.com/1258058.
    if (it.state() ==
        EffectTreeLayerListIterator::State::kContributingSurface) {
      const RenderSurfaceImpl* occlusion_surface =
          occlusion_tracker.OcclusionSurfaceForContributingSurface();
      gfx::Transform draw_transform;
      RenderSurfaceImpl* render_surface = it.current_render_surface();
      if (occlusion_surface) {
        // We are calculating transform between two render surfaces. So, we
        // need to apply the surface contents scale at target and remove the
        // surface contents scale at source.
        property_trees()->GetToTarget(render_surface->TransformTreeIndex(),
                                      occlusion_surface->EffectTreeIndex(),
                                      &draw_transform);
        const EffectNode* effect_node = property_trees()->effect_tree().Node(
            render_surface->EffectTreeIndex());
        draw_property_utils::ConcatInverseSurfaceContentsScale(effect_node,
                                                               &draw_transform);
      }

      Occlusion occlusion =
          occlusion_tracker.GetCurrentOcclusionForContributingSurface(
              draw_transform);
      render_surface->set_occlusion_in_content_space(occlusion);
    }

    occlusion_tracker.LeaveLayer(it);
  }

  unoccluded_screen_space_region_ =
      occlusion_tracker.ComputeVisibleRegionInScreen(this);

  // Resourceless draw do not need tiles and should not affect existing tile
  // priorities.
  if (!is_in_resourceless_software_draw_mode()) {
    needs_update_tiles_ = true;
    bool tile_priorities_may_be_updated = true;
    if (update_tiles) {
      tile_priorities_may_be_updated = UpdateTiles();
    }
    if (tile_priorities_may_be_updated) {
      DidModifyTilePriorities(/*pending_update_tiles=*/!update_tiles);
    }
  }

  if (update_image_animation_controller && image_animation_controller()) {
    image_animation_controller()->UpdateStateFromDrivers();
  }

  device_viewport_rect_changed_ = false;

  DCHECK(!needs_update_draw_properties_)
      << "CalcDrawProperties should not set_needs_update_draw_properties()";
  return true;
}

bool LayerTreeImpl::UpdateTiles() {
  if (!needs_update_tiles_) {
    return false;
  }
  needs_update_tiles_ = false;

  TRACE_EVENT_BEGIN2("cc,benchmark", "LayerTreeImpl::UpdateTiles", "IsActive",
                     IsActiveTree(), "SourceFrameNumber", source_frame_number_);
  size_t layers_updated_count = 0;
  bool tile_priorities_updated = false;
  const bool release_tile_resources_for_hidden_layers =
      settings().release_tile_resources_for_hidden_layers;
  for (PictureLayerImpl* layer : picture_layers_) {
    if (!layer->HasValidTilePriorities()) {
      if (release_tile_resources_for_hidden_layers) {
        layer->ReleaseResources();
      }
      continue;
    }
    ++layers_updated_count;
    tile_priorities_updated |= layer->UpdateTiles();
  }

  TRACE_EVENT_END1("cc,benchmark",
                   "LayerTreeImpl::UpdateDrawProperties::UpdateTiles",
                   "layers_updated_count", layers_updated_count);
  return tile_priorities_updated;
}

const RenderSurfaceList& LayerTreeImpl::GetRenderSurfaceList() const {
  // If this assert triggers, then the list is dirty.
  DCHECK(!needs_update_draw_properties_);
  return render_surface_list_;
}

const Region& LayerTreeImpl::UnoccludedScreenSpaceRegion() const {
  // If this assert triggers, then the render_surface_list_ is dirty, so the
  // unoccluded_screen_space_region_ is not valid anymore.
  DCHECK(!needs_update_draw_properties_);
  return unoccluded_screen_space_region_;
}

gfx::SizeF LayerTreeImpl::ScrollableSize() const {
  auto* scroll_node = OuterViewportScrollNode();
  if (!scroll_node) {
    DCHECK(!InnerViewportScrollNode());
    return gfx::SizeF();
  }
  const auto& scroll_tree = property_trees()->scroll_tree();
  auto size = scroll_tree.scroll_bounds(scroll_node->id);
  size.SetToMax(gfx::SizeF(scroll_tree.container_bounds(scroll_node->id)));
  return size;
}

LayerImpl* LayerTreeImpl::LayerById(int id) const {
  auto iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

// TODO(masonf): If this shows up on profiles, this could use
// a layer_element_map_ approach similar to LayerById().
LayerImpl* LayerTreeImpl::LayerByElementId(ElementId element_id) const {
  auto it = base::ranges::find(base::Reversed(*this), element_id,
                               &LayerImpl::element_id);
  if (it == rend())
    return nullptr;
  return *it;
}

void LayerTreeImpl::SetSurfaceRanges(
    base::flat_set<viz::SurfaceRange> surface_ranges) {
  DCHECK(surface_layer_ranges_.empty());
  surface_layer_ranges_ = std::move(surface_ranges);
  needs_surface_ranges_sync_ = true;
}

const base::flat_set<viz::SurfaceRange>& LayerTreeImpl::SurfaceRanges() const {
  return surface_layer_ranges_;
}

void LayerTreeImpl::ClearSurfaceRanges() {
  surface_layer_ranges_.clear();
  needs_surface_ranges_sync_ = true;
}

void LayerTreeImpl::AddLayerShouldPushProperties(LayerImpl* layer) {
  // When pushing from pending to active tree, PictureLayerImpls should only go
  // into this set when always_push_properties_on_picture_layers() is disabled.
  DCHECK(!always_push_properties_on_picture_layers() ||
         !base::Contains(picture_layers_, layer) ||
         (IsActiveTree() && settings().UseLayerContextForDisplay()));
  layers_that_should_push_properties_.insert(layer);
}

void LayerTreeImpl::ClearLayersThatShouldPushProperties() {
  layers_that_should_push_properties_.clear();
}

void LayerTreeImpl::RegisterLayer(LayerImpl* layer) {
  DCHECK(!LayerById(layer->id()));
  layer_id_map_[layer->id()] = layer;
}

void LayerTreeImpl::UnregisterLayer(LayerImpl* layer) {
  DCHECK(LayerById(layer->id()));
  layers_that_should_push_properties_.erase(layer);
  layer_id_map_.erase(layer->id());
}

void LayerTreeImpl::AddLayer(std::unique_ptr<LayerImpl> layer) {
  DCHECK(layer);
  DCHECK(!base::Contains(layer_list_, layer));
  layer_list_.push_back(std::move(layer));
  set_needs_update_draw_properties();
}

size_t LayerTreeImpl::NumLayers() {
  return layer_id_map_.size();
}

void LayerTreeImpl::DidBecomeActive() {
  if (next_activation_forces_redraw_) {
    host_impl_->SetViewportDamage(GetDeviceViewport());
    next_activation_forces_redraw_ = false;
  }

  // Always reset this flag on activation, as we would only have activated
  // if we were in a good state.
  host_impl_->ResetRequiresHighResToDraw();

  for (auto* layer : *this)
    layer->DidBecomeActive();

  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->DidActivate();
  devtools_instrumentation::DidActivateLayerTree(host_impl_->id(),
                                                 source_frame_number_);
}

bool LayerTreeImpl::RequiresHighResToDraw() const {
  return host_impl_->RequiresHighResToDraw();
}

TaskRunnerProvider* LayerTreeImpl::task_runner_provider() const {
  return host_impl_->task_runner_provider();
}

LayerTreeFrameSink* LayerTreeImpl::layer_tree_frame_sink() {
  return host_impl_->layer_tree_frame_sink();
}

int LayerTreeImpl::max_texture_size() const {
  return host_impl_->max_texture_size();
}

const LayerTreeSettings& LayerTreeImpl::settings() const {
  return host_impl_->settings();
}

const LayerTreeDebugState& LayerTreeImpl::debug_state() const {
  return host_impl_->debug_state();
}

viz::RasterContextProvider* LayerTreeImpl::context_provider() const {
  return host_impl_->layer_tree_frame_sink()->context_provider();
}

viz::ClientResourceProvider* LayerTreeImpl::resource_provider() const {
  return host_impl_->resource_provider();
}

TileManager* LayerTreeImpl::tile_manager() const {
  return host_impl_->tile_manager();
}

ImageDecodeCache* LayerTreeImpl::image_decode_cache() const {
  return host_impl_->GetImageDecodeCache();
}

ImageAnimationController* LayerTreeImpl::image_animation_controller() const {
  return host_impl_->image_animation_controller();
}

DroppedFrameCounter* LayerTreeImpl::dropped_frame_counter() const {
  return host_impl_->dropped_frame_counter();
}

MemoryHistory* LayerTreeImpl::memory_history() const {
  return host_impl_->memory_history();
}

DebugRectHistory* LayerTreeImpl::debug_rect_history() const {
  return host_impl_->debug_rect_history();
}

bool LayerTreeImpl::IsActiveTree() const {
  return host_impl_->active_tree() == this;
}

bool LayerTreeImpl::IsPendingTree() const {
  return host_impl_->pending_tree() == this;
}

bool LayerTreeImpl::IsRecycleTree() const {
  return host_impl_->recycle_tree() == this;
}

bool LayerTreeImpl::IsSyncTree() const {
  return host_impl_->sync_tree() == this;
}

bool LayerTreeImpl::HasPendingTree() const {
  return host_impl_->pending_tree() != nullptr;
}

LayerImpl* LayerTreeImpl::FindActiveTreeLayerById(int id) {
  LayerTreeImpl* tree = host_impl_->active_tree();
  if (!tree)
    return nullptr;
  return tree->LayerById(id);
}

LayerImpl* LayerTreeImpl::FindPendingTreeLayerById(int id) {
  LayerTreeImpl* tree = host_impl_->pending_tree();
  if (!tree)
    return nullptr;
  return tree->LayerById(id);
}

bool LayerTreeImpl::PinchGestureActive() const {
  return host_impl_->IsPinchGestureActive();
}

const viz::BeginFrameArgs& LayerTreeImpl::CurrentBeginFrameArgs() const {
  return host_impl_->CurrentBeginFrameArgs();
}

base::TimeDelta LayerTreeImpl::CurrentBeginFrameInterval() const {
  return host_impl_->CurrentBeginFrameInterval();
}

const gfx::Rect LayerTreeImpl::ViewportRectForTilePriority() const {
  const gfx::Rect& viewport_rect_for_tile_priority =
      host_impl_->viewport_rect_for_tile_priority();
  return viewport_rect_for_tile_priority.IsEmpty()
             ? GetDeviceViewport()
             : viewport_rect_for_tile_priority;
}

std::unique_ptr<ScrollbarAnimationController>
LayerTreeImpl::CreateScrollbarAnimationController(ElementId scroll_element_id,
                                                  float initial_opacity) {
  base::TimeDelta fade_delay = settings().scrollbar_fade_delay;
  base::TimeDelta fade_duration = settings().scrollbar_fade_duration;
  switch (settings().scrollbar_animator) {
    case LayerTreeSettings::ANDROID_OVERLAY: {
      return ScrollbarAnimationController::
          CreateScrollbarAnimationControllerAndroid(
              scroll_element_id, host_impl_, fade_delay, fade_duration,
              initial_opacity);
    }
    case LayerTreeSettings::AURA_OVERLAY: {
      base::TimeDelta thinning_duration =
          settings().scrollbar_thinning_duration;
      float idle_thickness_scale = settings().idle_thickness_scale;
      return ScrollbarAnimationController::
          CreateScrollbarAnimationControllerAuraOverlay(
              scroll_element_id, host_impl_, fade_delay, fade_duration,
              thinning_duration, initial_opacity, idle_thickness_scale);
    }
    case LayerTreeSettings::NO_ANIMATOR:
      NOTREACHED();
  }
  return nullptr;
}

void LayerTreeImpl::DidAnimateScrollOffset() {
  host_impl_->DidAnimateScrollOffset();
}

bool LayerTreeImpl::use_gpu_rasterization() const {
  return host_impl_->use_gpu_rasterization();
}

bool LayerTreeImpl::create_low_res_tiling() const {
  return host_impl_->create_low_res_tiling();
}

void LayerTreeImpl::SetNeedsRedraw() {
  host_impl_->SetNeedsRedraw();
}

void LayerTreeImpl::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
  for (auto* layer : base::Reversed(*this)) {
    if (!layer->contributes_to_drawn_render_surface())
      continue;
    layer->GetAllPrioritizedTilesForTracing(prioritized_tiles);
  }
}

void LayerTreeImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  viz::TracedValue::MakeDictIntoImplicitSnapshot(state, "cc::LayerTreeImpl",
                                                 this);
  state->SetInteger("source_frame_number", source_frame_number_);

  state->BeginArray("render_surface_layer_list");
  for (auto* layer : base::Reversed(*this)) {
    if (layer->contributes_to_drawn_render_surface())
      continue;
    viz::TracedValue::AppendIDRef(layer, state);
  }
  state->EndArray();

  state->BeginArray("swap_promise_trace_ids");
  for (const auto& swap_promise : swap_promise_list_)
    state->AppendDouble(swap_promise->GetTraceId());
  state->EndArray();

  state->BeginArray("pinned_swap_promise_trace_ids");
  for (const auto& swap_promise : pinned_swap_promise_list_)
    state->AppendDouble(swap_promise->GetTraceId());
  state->EndArray();

  state->BeginArray("layers");
  for (auto* layer : *this) {
    state->BeginDictionary();
    layer->AsValueInto(state);
    state->EndDictionary();
  }
  state->EndArray();
}

void LayerTreeImpl::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  DCHECK(swap_promise);
  swap_promise_list_.push_back(std::move(swap_promise));
}

void LayerTreeImpl::QueuePinnedSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  DCHECK(IsActiveTree());
  DCHECK(swap_promise);
  pinned_swap_promise_list_.push_back(std::move(swap_promise));
}

void LayerTreeImpl::PassSwapPromises(
    std::vector<std::unique_ptr<SwapPromise>> new_swap_promises) {
  base::TimeTicks timestamp = base::TimeTicks::Now();
  for (auto& swap_promise : swap_promise_list_) {
    if (swap_promise->DidNotSwap(SwapPromise::SWAP_FAILS, timestamp) ==
        SwapPromise::DidNotSwapAction::KEEP_ACTIVE) {
      // |swap_promise| must remain active, so place it in |new_swap_promises|
      // in order to keep it alive and active.
      new_swap_promises.push_back(std::move(swap_promise));
    }
  }
  swap_promise_list_.clear();
  swap_promise_list_.swap(new_swap_promises);
}

void LayerTreeImpl::AppendSwapPromises(
    std::vector<std::unique_ptr<SwapPromise>> new_swap_promises) {
  std::move(new_swap_promises.begin(), new_swap_promises.end(),
            std::back_inserter(swap_promise_list_));
  new_swap_promises.clear();
}

void LayerTreeImpl::FinishSwapPromises(viz::CompositorFrameMetadata* metadata) {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->WillSwap(metadata);
  for (const auto& swap_promise : pinned_swap_promise_list_)
    swap_promise->WillSwap(metadata);
}

void LayerTreeImpl::ClearSwapPromises() {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->DidSwap();
  swap_promise_list_.clear();
  for (const auto& swap_promise : pinned_swap_promise_list_)
    swap_promise->DidSwap();
  pinned_swap_promise_list_.clear();
}

void LayerTreeImpl::BreakSwapPromises(SwapPromise::DidNotSwapReason reason) {
  base::TimeTicks invocation_timestamp = base::TimeTicks::Now();
  {
    std::vector<std::unique_ptr<SwapPromise>> persistent_swap_promises;
    for (auto& swap_promise : swap_promise_list_) {
      if (swap_promise->DidNotSwap(reason, invocation_timestamp) ==
          SwapPromise::DidNotSwapAction::KEEP_ACTIVE) {
        persistent_swap_promises.push_back(std::move(swap_promise));
      }
    }
    // |persistent_swap_promises| must remain active even when swap fails.
    swap_promise_list_ = std::move(persistent_swap_promises);
  }

  {
    std::vector<std::unique_ptr<SwapPromise>> persistent_swap_promises;
    for (auto& swap_promise : pinned_swap_promise_list_) {
      if (swap_promise->DidNotSwap(reason, invocation_timestamp) ==
          SwapPromise::DidNotSwapAction::KEEP_ACTIVE) {
        persistent_swap_promises.push_back(std::move(swap_promise));
      }
    }

    // |persistent_swap_promises| must remain active even when swap fails.
    pinned_swap_promise_list_ = std::move(persistent_swap_promises);
  }
}

void LayerTreeImpl::DidModifyTilePriorities(bool pending_update_tiles) {
  host_impl_->DidModifyTilePriorities(pending_update_tiles);
}

void LayerTreeImpl::set_ui_resource_request_queue(
    UIResourceRequestQueue queue) {
  ui_resource_request_queue_ = std::move(queue);
}

viz::ResourceId LayerTreeImpl::ResourceIdForUIResource(UIResourceId uid) const {
  return host_impl_->ResourceIdForUIResource(uid);
}

bool LayerTreeImpl::IsUIResourceOpaque(UIResourceId uid) const {
  return host_impl_->IsUIResourceOpaque(uid);
}

void LayerTreeImpl::ProcessUIResourceRequestQueue() {
  TRACE_EVENT1("cc", "ProcessUIResourceRequestQueue", "queue_size",
               ui_resource_request_queue_.size());
  for (const auto& req : ui_resource_request_queue_) {
    switch (req.GetType()) {
      case UIResourceRequest::Type::kCreate:
        host_impl_->CreateUIResource(req.GetId(), req.GetBitmap());
        break;
      case UIResourceRequest::Type::kDelete:
        host_impl_->DeleteUIResource(req.GetId());
        break;
    }
  }
  ui_resource_request_queue_.clear();

  // If all UI resource evictions were not recreated by processing this queue,
  // then another commit is required.
  if (host_impl_->EvictedUIResourcesExist())
    host_impl_->SetNeedsCommit();
}

void LayerTreeImpl::RegisterPictureLayerImpl(PictureLayerImpl* layer) {
  DCHECK(!base::Contains(picture_layers_, layer));
  picture_layers_.push_back(layer);
}

void LayerTreeImpl::UnregisterPictureLayerImpl(PictureLayerImpl* layer) {
  auto it = base::ranges::find(picture_layers_, layer);
  CHECK(it != picture_layers_.end(), base::NotFatalUntil::M130);
  picture_layers_.erase(it);

  // Make sure that |picture_layers_with_paint_worklets_| doesn't get left with
  // dead layers. They should already have been removed (via calling
  // NotifyLayerHasPaintWorkletsChanged) before the layer was unregistered.
  DCHECK(!picture_layers_with_paint_worklets_.contains(layer));
}

void LayerTreeImpl::NotifyLayerHasPaintWorkletsChanged(PictureLayerImpl* layer,
                                                       bool has_worklets) {
  if (has_worklets) {
    auto insert_pair = picture_layers_with_paint_worklets_.insert(layer);
    DCHECK(insert_pair.second);
  } else {
    auto it = picture_layers_with_paint_worklets_.find(layer);
    CHECK(it != picture_layers_with_paint_worklets_.end(),
          base::NotFatalUntil::M130);
    picture_layers_with_paint_worklets_.erase(it);
  }
}

void LayerTreeImpl::RegisterScrollbar(ScrollbarLayerImplBase* scrollbar_layer) {
  ElementId scroll_element_id = scrollbar_layer->scroll_element_id();
  if (!scroll_element_id)
    return;

  auto* scrollbar_ids = &element_id_to_scrollbar_layer_ids_[scroll_element_id];
  int* scrollbar_layer_id =
      scrollbar_layer->orientation() == ScrollbarOrientation::kHorizontal
          ? &scrollbar_ids->horizontal
          : &scrollbar_ids->vertical;

  // We used to DCHECK this was not the case but this can occur on Android: as
  // the visual viewport supplies scrollbars for the outer viewport, if the
  // outer viewport is changed, we race between updating the visual viewport
  // scrollbars and registering new scrollbars on the old outer viewport. It'd
  // be nice if we could fix this to be cleaner but its harmless to just
  // unregister here.
  if (*scrollbar_layer_id != Layer::INVALID_ID) {
    UnregisterScrollbar(scrollbar_layer);

    // The scrollbar_ids could have been erased above so get it again.
    scrollbar_ids = &element_id_to_scrollbar_layer_ids_[scroll_element_id];
    scrollbar_layer_id =
        scrollbar_layer->orientation() == ScrollbarOrientation::kHorizontal
            ? &scrollbar_ids->horizontal
            : &scrollbar_ids->vertical;
  }

  *scrollbar_layer_id = scrollbar_layer->id();

  if (IsActiveTree()) {
    host_impl_->DidRegisterScrollbarLayer(scroll_element_id,
                                          scrollbar_layer->orientation());
  }

  if (IsActiveTree() && scrollbar_layer->is_overlay_scrollbar() &&
      scrollbar_layer->GetScrollbarAnimator() !=
          LayerTreeSettings::NO_ANIMATOR) {
    // Fluent overlay scrollbars are invisible until the DidRequestShow gets
    // called.
    if (scrollbar_layer->IsFluentOverlayScrollbarEnabled()) {
      scrollbar_layer->SetOverlayScrollbarLayerOpacityAnimated(
          0.f, /*fade_out_animation=*/false);
    }
    host_impl_->RegisterScrollbarAnimationController(
        scroll_element_id, scrollbar_layer->Opacity());
  }
}

void LayerTreeImpl::UnregisterScrollbar(
    ScrollbarLayerImplBase* scrollbar_layer) {
  ElementId scroll_element_id = scrollbar_layer->scroll_element_id();
  if (!scroll_element_id)
    return;

  auto& scrollbar_ids = element_id_to_scrollbar_layer_ids_[scroll_element_id];
  if (scrollbar_layer->orientation() == ScrollbarOrientation::kHorizontal) {
    scrollbar_ids.horizontal = Layer::INVALID_ID;
  } else {
    scrollbar_ids.vertical = Layer::INVALID_ID;
  }

  if (scrollbar_ids.horizontal == Layer::INVALID_ID &&
      scrollbar_ids.vertical == Layer::INVALID_ID) {
    element_id_to_scrollbar_layer_ids_.erase(scroll_element_id);
  }

  if (IsActiveTree()) {
    host_impl_->DidUnregisterScrollbarLayer(scroll_element_id,
                                            scrollbar_layer->orientation());
  }
}

ScrollbarSet LayerTreeImpl::ScrollbarsFor(ElementId scroll_element_id) const {
  ScrollbarSet scrollbars;
  auto it = element_id_to_scrollbar_layer_ids_.find(scroll_element_id);
  if (it != element_id_to_scrollbar_layer_ids_.end()) {
    const ScrollbarLayerIds& layer_ids = it->second;
    if (layer_ids.horizontal != Layer::INVALID_ID)
      scrollbars.insert(ToScrollbarLayer(LayerById(layer_ids.horizontal)));
    if (layer_ids.vertical != Layer::INVALID_ID)
      scrollbars.insert(ToScrollbarLayer(LayerById(layer_ids.vertical)));
  }
  return scrollbars;
}

static bool PointHitsRect(
    const gfx::PointF& screen_space_point,
    const gfx::Transform& local_space_to_screen_space_transform,
    const gfx::Rect& local_space_rect,
    float* distance_to_camera) {
  // If the transform is not invertible, then assume that this point doesn't hit
  // this rect.
  gfx::Transform inverse_local_space_to_screen_space;
  if (!local_space_to_screen_space_transform.GetInverse(
          &inverse_local_space_to_screen_space))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given rect.
  bool clipped = false;
  gfx::Point3F planar_point = MathUtil::ProjectPoint3D(
      inverse_local_space_to_screen_space, screen_space_point, &clipped);
  gfx::PointF hit_test_point_in_local_space =
      gfx::PointF(planar_point.x(), planar_point.y());

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this rect.
  if (clipped)
    return false;

  if (!gfx::RectF(local_space_rect).Contains(hit_test_point_in_local_space))
    return false;

  if (distance_to_camera) {
    // To compute the distance to the camera, we have to take the planar point
    // and pull it back to world space and compute the displacement along the
    // z-axis.
    gfx::Point3F planar_point_in_screen_space =
        local_space_to_screen_space_transform.MapPoint(planar_point);
    *distance_to_camera = planar_point_in_screen_space.z();
  }

  return true;
}

static bool PointIsClippedByAncestorClipNode(
    const gfx::PointF& screen_space_point,
    const LayerImpl* layer) {
  // We need to visit all ancestor clip nodes to check this. Checking with just
  // the combined clip stored at a clip node is not enough because parent
  // combined clip can sometimes be smaller than current combined clip. This can
  // happen when we have transforms like rotation that inflate the combined
  // clip's bounds. Also, the point can be clipped by the content rect of an
  // ancestor render surface.

  // We first check if the point is clipped by viewport.
  const PropertyTrees* property_trees =
      layer->layer_tree_impl()->property_trees();
  const ClipTree& clip_tree = property_trees->clip_tree();
  const TransformTree& transform_tree = property_trees->transform_tree();
  gfx::Rect clip = gfx::ToEnclosingRect(clip_tree.Node(1)->clip);
  if (!PointHitsRect(screen_space_point, gfx::Transform(), clip, nullptr))
    return true;

  for (const ClipNode* clip_node = clip_tree.Node(layer->clip_tree_index());
       clip_node->id > kViewportPropertyNodeId;
       clip_node = clip_tree.parent(clip_node)) {
    if (clip_node->AppliesLocalClip()) {
      clip = gfx::ToEnclosingRect(clip_node->clip);

      gfx::Transform screen_space_transform =
          transform_tree.ToScreen(clip_node->transform_id);
      if (!PointHitsRect(screen_space_point, screen_space_transform, clip,
                         nullptr)) {
        return true;
      }
    }
  }
  return false;
}

static bool PointIsClippedBySurfaceOrClipRect(
    const gfx::PointF& screen_space_point,
    const LayerImpl* layer) {
  // Walk up the layer tree and hit-test any render_surfaces and any layer
  // clip rects that are active.
  return PointIsClippedByAncestorClipNode(screen_space_point, layer);
}

static bool PointHitsRegion(const gfx::PointF& screen_space_point,
                            const gfx::Transform& screen_space_transform,
                            const Region& layer_space_region,
                            const LayerImpl* layer_impl) {
  if (layer_space_region.IsEmpty())
    return false;

  // If the transform is not invertible, then assume that this point doesn't hit
  // this region.
  gfx::Transform inverse_screen_space_transform;
  if (!screen_space_transform.GetInverse(&inverse_screen_space_transform))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given region.
  bool clipped = false;
  gfx::PointF hit_test_point_in_layer_space = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_point, &clipped);

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this region.
  if (clipped)
    return false;

  // We need to walk up the parents to ensure that the layer is not clipped in
  // such a way that it is impossible for the point to hit the layer.
  if (layer_impl &&
      PointIsClippedBySurfaceOrClipRect(screen_space_point, layer_impl))
    return false;

  return layer_space_region.Contains(
      gfx::ToRoundedPoint(hit_test_point_in_layer_space));
}

static bool PointHitsLayer(const LayerImpl* layer,
                           const gfx::PointF& screen_space_point,
                           float* distance_to_intersection) {
  gfx::Rect content_rect(layer->bounds());
  if (!PointHitsRect(screen_space_point, layer->ScreenSpaceTransform(),
                     content_rect, distance_to_intersection)) {
    return false;
  }

  // At this point, we think the point does hit the layer, but we need to walk
  // up the parents to ensure that the layer was not clipped in such a way
  // that the hit point actually should not hit the layer.
  if (PointIsClippedBySurfaceOrClipRect(screen_space_point, layer))
    return false;

  // Skip the HUD layer.
  if (layer == layer->layer_tree_impl()->hud_layer())
    return false;

  return true;
}

struct FindClosestMatchingLayerState {
  STACK_ALLOCATED();

 public:
  FindClosestMatchingLayerState()
      : closest_match(nullptr),
        closest_distance(-std::numeric_limits<float>::infinity()) {}
  LayerImpl* closest_match = nullptr;
  // Note that the positive z-axis points towards the camera, so bigger means
  // closer in this case, counterintuitively.
  float closest_distance;
};

template <typename Functor>
static void FindClosestMatchingLayer(const gfx::PointF& screen_space_point,
                                     LayerImpl* root_layer,
                                     const Functor& func,
                                     FindClosestMatchingLayerState* state) {
  // We want to iterate from front to back when hit testing.
  for (auto* layer : base::Reversed(*root_layer->layer_tree_impl())) {
    if (!func(layer))
      continue;

    float distance_to_intersection = 0.f;
    bool hit = false;
    if (layer->Is3dSorted()) {
      hit =
          PointHitsLayer(layer, screen_space_point, &distance_to_intersection);
    } else {
      hit = PointHitsLayer(layer, screen_space_point, nullptr);
    }

    if (!hit)
      continue;

    bool in_front_of_previous_candidate =
        state->closest_match &&
        layer->GetSortingContextId() ==
            state->closest_match->GetSortingContextId() &&
        distance_to_intersection >
            state->closest_distance + std::numeric_limits<float>::epsilon();

    if (!state->closest_match || in_front_of_previous_candidate) {
      state->closest_distance = distance_to_intersection;
      state->closest_match = layer;
    }
  }
}

struct HitTestVisibleScrollableOrTouchableFunctor {
  bool operator()(LayerImpl* layer) const { return layer->HitTestable(); }
};

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  if (layer_list_.empty())
    return nullptr;
  bool update_tiles = !features::IsCCSlimmingEnabled();
  if (!UpdateDrawProperties(update_tiles,
                            /*update_image_animation_controller=*/true)) {
    return nullptr;
  }
  FindClosestMatchingLayerState state;
  FindClosestMatchingLayer(screen_space_point, layer_list_[0].get(),
                           HitTestVisibleScrollableOrTouchableFunctor(),
                           &state);
  return state.closest_match;
}

struct FindTouchEventLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    if (!layer->has_touch_action_regions())
      return false;
    return PointHitsRegion(screen_space_point, layer->ScreenSpaceTransform(),
                           layer->GetAllTouchActionRegions(), layer);
  }
  const gfx::PointF screen_space_point;
};

struct FindWheelEventHandlerLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    return PointHitsRegion(screen_space_point, layer->ScreenSpaceTransform(),
                           layer->wheel_event_handler_region(), layer);
  }
  const gfx::PointF screen_space_point;
};

template <typename Functor>
LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPointInEventHandlerRegion(
    const gfx::PointF& screen_space_point,
    const Functor& func) {
  if (layer_list_.empty())
    return nullptr;
  bool update_tiles = !features::IsCCSlimmingEnabled();
  if (!UpdateDrawProperties(update_tiles,
                            /*update_image_animation_controller=*/true)) {
    return nullptr;
  }
  FindClosestMatchingLayerState state;
  FindClosestMatchingLayer(screen_space_point, layer_list_[0].get(), func,
                           &state);
  return state.closest_match;
}

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPointInTouchHandlerRegion(
    const gfx::PointF& screen_space_point) {
  FindTouchEventLayerFunctor func = {screen_space_point};
  return FindLayerThatIsHitByPointInEventHandlerRegion(screen_space_point,
                                                       func);
}

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPointInWheelEventHandlerRegion(
    const gfx::PointF& screen_space_point) {
  FindWheelEventHandlerLayerFunctor func = {screen_space_point};
  return FindLayerThatIsHitByPointInEventHandlerRegion(screen_space_point,
                                                       func);
}

std::vector<const LayerImpl*>
LayerTreeImpl::FindLayersUpToFirstScrollableOrOpaqueToHitTest(
    const gfx::PointF& screen_space_point) {
  std::vector<const LayerImpl*> layers;
  if (layer_list_.empty())
    return layers;
  bool update_tiles = !features::IsCCSlimmingEnabled();
  if (!UpdateDrawProperties(update_tiles,
                            /*update_image_animation_controller=*/true)) {
    return layers;
  }

  // If we hit a layer in a 3d context we can't rely on layer orders, we need
  // to sort the layers by distance to hit. This is used only if the first_hit
  // layer is in a 3d rendering context.
  std::vector<std::pair<const LayerImpl*, float>> layers_3d;

  const LayerImpl* first_hit = nullptr;

  // We want to iterate from front to back when hit testing.
  LayerImpl* root_layer = layer_list_[0].get();
  for (const auto* layer : base::Reversed(*root_layer->layer_tree_impl())) {
    if (!layer->HitTestable())
      continue;

    if (first_hit && layer->Is3dSorted() &&
        layer->GetSortingContextId() != first_hit->GetSortingContextId()) {
      // The intention here is to skip over any layers that belong to a
      // different 3d sorting context than the first_hit layer.
      //
      // TODO(crbug.com/40887983): This code is kind of broken for the case of a
      // scroller inside a preserve-3d: we assign a sorting_context_id to the
      // scroller's main layer, which is marked as scrollable, but not its
      // scrolling-contents layer, which is first_hit.  Currently we rely on
      // InputHandler::IsInitialScrollHitTestReliable to discover this situation
      // and send us into the main thread hit test.
      continue;
    }

    float distance_to_intersection = 0.f;
    bool hit = false;
    if (layer->Is3dSorted()) {
      hit =
          PointHitsLayer(layer, screen_space_point, &distance_to_intersection);
    } else {
      hit = PointHitsLayer(layer, screen_space_point, nullptr);
    }

    if (!hit)
      continue;

    if (!first_hit)
      first_hit = layer;

    if (first_hit->Is3dSorted() && layer->Is3dSorted()) {
      layers_3d.emplace_back(
          std::pair<const LayerImpl*, float>(layer, distance_to_intersection));
    } else {
      layers.push_back(layer);
      if (settings().enable_hit_test_opaqueness
              ? layer->OpaqueToHitTest()
              : layer->IsScrollerOrScrollbar()) {
        break;
      }
    }
  }

  if (!first_hit) {
    DCHECK(layers.empty());
    DCHECK(layers_3d.empty());
    return layers;
  }

  if (first_hit->Is3dSorted()) {
    std::vector<const LayerImpl*> result;
    DCHECK(!layers_3d.empty());

    // Since we hit a layer in a rendering context, we need to sort the layers
    // based on their distance then add all until the first scrollable one to
    // the return vector.
    std::sort(layers_3d.begin(), layers_3d.end(),
              [](const std::pair<const LayerImpl*, float>& a,
                 const std::pair<const LayerImpl*, float>& b) {
                return a.second > b.second;
              });

    for (const auto& pair : layers_3d) {
      const LayerImpl* layer = pair.first;

      result.push_back(layer);
      if (settings().enable_hit_test_opaqueness
              ? layer->OpaqueToHitTest()
              : layer->IsScrollerOrScrollbar()) {
        return result;
      }
    }
    // Append 2D layers if none of the 3D layers were scrollable.
    result.insert(result.end(), layers.begin(), layers.end());
    return result;
  } else {
    DCHECK(!layers.empty());
    DCHECK(layers_3d.empty());
  }

  return layers;
}

bool LayerTreeImpl::PointHitsMainThreadScrollHitTestRegion(
    const gfx::PointF& screen_space_point,
    const LayerImpl& layer) const {
  // We assume the layer has already been hit tested.
  DCHECK(PointHitsLayer(&layer, screen_space_point, nullptr));

  if (layer.main_thread_scroll_hit_test_region().IsEmpty()) {
    return false;
  }

  return PointHitsRegion(screen_space_point, layer.ScreenSpaceTransform(),
                         layer.main_thread_scroll_hit_test_region(), &layer);
}

ElementId LayerTreeImpl::PointHitsNonCompositedScroll(
    const gfx::PointF& screen_space_point,
    const LayerImpl& layer) const {
  const std::vector<ScrollHitTestRect>* hit_test_rects =
      layer.non_composited_scroll_hit_test_rects();
  if (!hit_test_rects) {
    return ElementId();
  }
  for (const ScrollHitTestRect& rect : base::Reversed(*hit_test_rects)) {
    if (PointHitsRect(screen_space_point, layer.ScreenSpaceTransform(),
                      rect.hit_test_rect, /*distance_to_camera=*/nullptr)) {
      return rect.scroll_element_id;
    }
  }
  return ElementId();
}

static ElementId GetFrameElementIdForLayer(const LayerImpl* layer) {
  const auto& transform_tree =
      layer->layer_tree_impl()->property_trees()->transform_tree();
  const auto* node = transform_tree.Node(layer->transform_tree_index());
  while (node && !node->visible_frame_element_id) {
    node = transform_tree.Node(node->parent_frame_id);
  }
  return node ? node->visible_frame_element_id : ElementId();
}

static void FindClosestMatchingLayerForAttribution(
    const gfx::PointF& screen_space_point,
    const LayerImpl* root_layer,
    FindClosestMatchingLayerState* state) {
  std::unordered_set<ElementId, ElementIdHash> hit_visible_frame_element_ids;
  // We want to iterate from front to back when hit testing.
  for (auto* layer : base::Reversed(*root_layer->layer_tree_impl())) {
    if (!layer->HitTestable())
      continue;

    float distance_to_intersection = 0.f;
    bool hit = false;
    if (layer->Is3dSorted()) {
      hit =
          PointHitsLayer(layer, screen_space_point, &distance_to_intersection);
    } else {
      hit = PointHitsLayer(layer, screen_space_point, nullptr);
    }

    if (!hit)
      continue;

    bool in_front_of_previous_candidate =
        state->closest_match &&
        layer->GetSortingContextId() ==
            state->closest_match->GetSortingContextId() &&
        distance_to_intersection >
            state->closest_distance + std::numeric_limits<float>::epsilon();

    if (!state->closest_match || in_front_of_previous_candidate) {
      state->closest_distance = distance_to_intersection;
      state->closest_match = layer;
    }

    ElementId visible_frame_element_id = GetFrameElementIdForLayer(layer);
    hit_visible_frame_element_ids.insert(visible_frame_element_id);
  }

  // Iterate through the transform tree of the hit layer in order to derive the
  // frame path. If we hit any frame layer in our hit testing that belonged to
  // a frame outside of this hierarchy, bail out.
  //
  // We explicitly allow occluding layers whose frames are parents of the
  // targeted frame so that we can properly attribute the (common) parent ->
  // child frame relationship. This is made possible since we can accurately
  // hit test within layerized subframes, but not for all occluders.
  if (auto* layer = state->closest_match) {
    const auto& transform_tree =
        layer->layer_tree_impl()->property_trees()->transform_tree();
    for (const auto* node = transform_tree.Node(layer->transform_tree_index());
         node; node = transform_tree.Node(node->parent_frame_id)) {
      hit_visible_frame_element_ids.erase(node->visible_frame_element_id);
      if (hit_visible_frame_element_ids.size() == 0)
        break;
    }

    if (hit_visible_frame_element_ids.size() > 0) {
      state->closest_distance = 0.f;
      state->closest_match = nullptr;
    }
  }
}

ElementId LayerTreeImpl::FindFrameElementIdAtPoint(
    const gfx::PointF& screen_space_point) {
  if (layer_list_.empty())
    return {};
  bool update_tiles = !features::IsCCSlimmingEnabled();
  if (!UpdateDrawProperties(update_tiles,
                            /*update_image_animation_controller=*/true)) {
    return {};
  }
  FindClosestMatchingLayerState state;
  FindClosestMatchingLayerForAttribution(screen_space_point,
                                         layer_list_[0].get(), &state);

  if (const auto* layer = state.closest_match) {
    // TODO(crbug.com/40121347): Permit hit testing only if the framed
    // element hit has a simple mask/clip. We don't have enough information
    // about complex masks/clips on the impl-side to do accurate hit testing.
    bool layer_hit_test_region_is_masked =
        property_trees()->effect_tree().HitTestMayBeAffectedByMask(
            layer->effect_tree_index());

    if (!layer_hit_test_region_is_masked)
      return GetFrameElementIdForLayer(layer);
  }

  return {};
}

void LayerTreeImpl::RegisterSelection(const LayerSelection& selection) {
  if (selection_ == selection)
    return;

  handle_visibility_changed_ = true;
  selection_ = selection;
}

void LayerTreeImpl::ResetHandleVisibilityChanged() {
  handle_visibility_changed_ = false;
}

static gfx::SelectionBound ComputeViewportSelectionBound(
    const LayerSelectionBound& layer_bound,
    LayerImpl* layer,
    float device_scale_factor) {
  gfx::SelectionBound viewport_bound;
  viewport_bound.set_type(layer_bound.type);

  if (!layer || layer_bound.type == gfx::SelectionBound::EMPTY)
    return viewport_bound;

  auto layer_start = gfx::PointF(layer_bound.edge_start);
  auto layer_end = gfx::PointF(layer_bound.edge_end);
  gfx::Transform screen_space_transform = layer->ScreenSpaceTransform();

  bool clipped = false;
  gfx::PointF screen_start =
      MathUtil::MapPoint(screen_space_transform, layer_start, &clipped);
  gfx::PointF screen_end =
      MathUtil::MapPoint(screen_space_transform, layer_end, &clipped);

  const float inv_scale = 1.f / device_scale_factor;
  viewport_bound.SetEdgeStart(gfx::ScalePoint(screen_start, inv_scale));
  viewport_bound.SetEdgeEnd(gfx::ScalePoint(screen_end, inv_scale));

  // If |layer_bound| is already hidden due to being occluded by painted content
  // within the layer, it must remain hidden. Otherwise, check whether its
  // position is outside the bounds of the layer.
  if (layer_bound.hidden) {
    viewport_bound.set_visible(false);
  } else {
    // The bottom edge point is used for visibility testing as it is the logical
    // focal point for bound selection handles (this may change in the future).
    // Shifting the visibility point fractionally inward ensures that
    // neighboring or logically coincident layers aligned to integral DPI
    // coordinates will not spuriously occlude the bound.
    gfx::Vector2dF visibility_offset = layer_start - layer_end;
    visibility_offset.Scale(device_scale_factor / visibility_offset.Length());
    gfx::PointF visibility_point = layer_end + visibility_offset;
    if (visibility_point.x() < 0)
      visibility_point.set_x(visibility_point.x() + device_scale_factor);
    visibility_point =
        MathUtil::MapPoint(screen_space_transform, visibility_point, &clipped);

    float intersect_distance = 0.f;
    viewport_bound.set_visible(
        PointHitsLayer(layer, visibility_point, &intersect_distance));
  }

  if (viewport_bound.visible()) {
    viewport_bound.SetVisibleEdge(viewport_bound.edge_start(),
                                  viewport_bound.edge_end());
  } else {
    // The |layer_start| and |layer_end| might be clipped.
    gfx::RectF visible_layer_rect(layer->visible_layer_rect());
    auto visible_layer_start = layer_start;
    auto visible_layer_end = layer_end;
    if (!visible_layer_rect.Contains(visible_layer_start) &&
        !visible_layer_rect.Contains(visible_layer_end))
      std::tie(visible_layer_start, visible_layer_end) =
          GetVisibleSelectionEndPoints(visible_layer_rect, layer_start,
                                       layer_end);

    gfx::PointF visible_screen_start = MathUtil::MapPoint(
        screen_space_transform, visible_layer_start, &clipped);
    gfx::PointF visible_screen_end =
        MathUtil::MapPoint(screen_space_transform, visible_layer_end, &clipped);

    viewport_bound.SetVisibleEdgeStart(
        gfx::ScalePoint(visible_screen_start, inv_scale));
    viewport_bound.SetVisibleEdgeEnd(
        gfx::ScalePoint(visible_screen_end, inv_scale));
  }

  return viewport_bound;
}

void LayerTreeImpl::GetViewportSelection(
    viz::Selection<gfx::SelectionBound>* selection) {
  DCHECK(selection);

  selection->start = ComputeViewportSelectionBound(
      selection_.start,
      selection_.start.layer_id ? LayerById(selection_.start.layer_id)
                                : nullptr,
      device_scale_factor() * painted_device_scale_factor());
  if (selection->start.type() == gfx::SelectionBound::CENTER ||
      selection->start.type() == gfx::SelectionBound::HIDDEN ||
      selection->start.type() == gfx::SelectionBound::EMPTY) {
    selection->end = selection->start;
  } else {
    selection->end = ComputeViewportSelectionBound(
        selection_.end,
        selection_.end.layer_id ? LayerById(selection_.end.layer_id) : nullptr,
        device_scale_factor() * painted_device_scale_factor());
  }
}

bool LayerTreeImpl::SmoothnessTakesPriority() const {
  return host_impl_->GetTreePriority() == SMOOTHNESS_TAKES_PRIORITY;
}

VideoFrameControllerClient* LayerTreeImpl::GetVideoFrameControllerClient()
    const {
  return host_impl_;
}

void LayerTreeImpl::UpdateImageDecodingHints(
    base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
        decoding_mode_map) {
  host_impl_->UpdateImageDecodingHints(std::move(decoding_mode_map));
}

int LayerTreeImpl::GetMSAASampleCountForRaster(
    const DisplayItemList& display_list) const {
  return host_impl_->GetMSAASampleCountForRaster(display_list);
}

TargetColorParams LayerTreeImpl::GetTargetColorParams(
    gfx::ContentColorUsage content_color_usage) const {
  return host_impl_->GetTargetColorParams(content_color_usage);
}

void LayerTreeImpl::SetPendingPageScaleAnimation(
    std::unique_ptr<PendingPageScaleAnimation> pending_animation) {
  pending_page_scale_animation_ = std::move(pending_animation);
}

std::unique_ptr<PendingPageScaleAnimation>
LayerTreeImpl::TakePendingPageScaleAnimation() {
  return std::move(pending_page_scale_animation_);
}

void LayerTreeImpl::AppendEventsMetricsFromMainThread(
    EventMetrics::List events_metrics) {
  events_metrics_from_main_thread_.reserve(
      events_metrics_from_main_thread_.size() + events_metrics.size());
  events_metrics_from_main_thread_.insert(
      events_metrics_from_main_thread_.end(),
      std::make_move_iterator(events_metrics.begin()),
      std::make_move_iterator(events_metrics.end()));
}

EventMetrics::List LayerTreeImpl::TakeEventsMetrics() {
  EventMetrics::List main_event_metrics_result;
  main_event_metrics_result.swap(events_metrics_from_main_thread_);
  return main_event_metrics_result;
}

bool LayerTreeImpl::TakeForceSendMetadataRequest() {
  bool force_send_metadata_request = force_send_metadata_request_;
  force_send_metadata_request_ = false;
  return force_send_metadata_request;
}

void LayerTreeImpl::ResetAllChangeTracking() {
  layers_that_should_push_properties_.clear();
  // Iterate over all layers, including masks.
  for (auto* layer : *this)
    layer->ResetChangeTracking();
  property_trees_.ResetAllChangeTracking();
}

std::string LayerTreeImpl::LayerListAsJson() const {
  base::trace_event::TracedValueJSON value;
  value.BeginArray("LayerTreeImpl");
  for (auto* layer : *this) {
    value.BeginDictionary();
    layer->AsValueInto(&value);
    value.EndDictionary();
  }
  value.EndArray();
  return value.ToFormattedJSON();
}

void LayerTreeImpl::AddViewTransitionRequest(
    std::unique_ptr<ViewTransitionRequest> request) {
  if (IsActiveTree() && request->type() == ViewTransitionRequest::Type::kSave) {
    // If the next frame will capture view transition snapshots, the main
    // thread will have already computed all transforms based on the current
    // location. Prevent any browser controls animation from ticking which
    // would make the transition state inconsistent with what is visually
    // displayed.
    host_impl_->browser_controls_manager()->ResetAnimations();
  }
  view_transition_requests_.push_back(std::move(request));
  // We need to send the request to viz.
  SetNeedsRedraw();
}

std::vector<std::unique_ptr<ViewTransitionRequest>>
LayerTreeImpl::TakeViewTransitionRequests() {
  return std::move(view_transition_requests_);
}

bool LayerTreeImpl::HasViewTransitionRequests() const {
  return !view_transition_requests_.empty();
}

bool LayerTreeImpl::HasViewTransitionSaveRequest() const {
  for (const auto& request : view_transition_requests_) {
    if (request->type() == ViewTransitionRequest::Type::kSave) {
      return true;
    }
  }

  return false;
}

bool LayerTreeImpl::IsReadyToActivate() const {
  return host_impl_->IsReadyToActivate();
}

void LayerTreeImpl::RequestImplSideInvalidationForRerasterTiling() {
  host_impl_->RequestImplSideInvalidationForRerasterTiling();
}

}  // namespace cc
