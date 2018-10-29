// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <set>

#include "base/containers/adapters.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/base/synced_property.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/layers/effect_tree_layer_list_iterator.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/resources/ui_resource_request.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/traced_value.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

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
    LayerTreeHostImpl* host_impl,
    scoped_refptr<SyncedProperty<ScaleGroup>> page_scale_factor,
    scoped_refptr<SyncedBrowserControls> top_controls_shown_ratio,
    scoped_refptr<SyncedElasticOverscroll> elastic_overscroll)
    : host_impl_(host_impl),
      source_frame_number_(-1),
      is_first_frame_after_commit_tracker_(-1),
      root_layer_for_testing_(nullptr),
      hud_layer_(nullptr),
      background_color_(0),
      last_scrolled_scroll_node_index_(ScrollTree::kInvalidNodeId),
      page_scale_factor_(page_scale_factor),
      min_page_scale_factor_(0),
      max_page_scale_factor_(0),
      device_scale_factor_(1.f),
      painted_device_scale_factor_(1.f),
      content_source_id_(0),
      elastic_overscroll_(elastic_overscroll),
      layers_(new OwnedLayerImplList),
      needs_update_draw_properties_(true),
      scrollbar_geometries_need_update_(false),
      needs_full_tree_sync_(true),
      needs_surface_ranges_sync_(false),
      next_activation_forces_redraw_(false),
      has_ever_been_drawn_(false),
      handle_visibility_changed_(false),
      have_scroll_event_handlers_(false),
      event_listener_properties_(),
      browser_controls_shrink_blink_size_(false),
      top_controls_height_(0),
      bottom_controls_height_(0),
      top_controls_shown_ratio_(top_controls_shown_ratio) {
  property_trees()->is_main_thread = false;
}

LayerTreeImpl::~LayerTreeImpl() {
  // Need to explicitly clear the tree prior to destroying this so that
  // the LayerTreeImpl pointer is still valid in the LayerImpl dtor.
  DCHECK(LayerListIsEmpty());
  DCHECK(layers_->empty());
}

void LayerTreeImpl::Shutdown() {
  DetachLayers();
  BreakSwapPromises(IsActiveTree() ? SwapPromise::SWAP_FAILS
                                   : SwapPromise::ACTIVATION_FAILS);
  DCHECK(LayerListIsEmpty());
}

void LayerTreeImpl::ReleaseResources() {
#if DCHECK_IS_ON()
  // These DCHECKs catch tests that add layers to the tree but fail to build the
  // layer list afterward.
  LayerListIterator<LayerImpl> it(root_layer_for_testing_);
  size_t i = 0;
  for (; it != LayerListIterator<LayerImpl>(nullptr); ++it, ++i) {
    DCHECK_LT(i, layer_list_.size());
    DCHECK_EQ(layer_list_[i], *it);
  }
#endif

  if (!LayerListIsEmpty()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        this, [](LayerImpl* layer) { layer->ReleaseResources(); });
  }
}

void LayerTreeImpl::OnPurgeMemory() {
  if (!LayerListIsEmpty()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        this, [](LayerImpl* layer) { layer->OnPurgeMemory(); });
  }
}

void LayerTreeImpl::ReleaseTileResources() {
  if (!LayerListIsEmpty()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        this, [](LayerImpl* layer) { layer->ReleaseTileResources(); });
  }
}

void LayerTreeImpl::RecreateTileResources() {
  if (!LayerListIsEmpty()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        this, [](LayerImpl* layer) { layer->RecreateTileResources(); });
  }
}

void LayerTreeImpl::DidUpdateScrollOffset(ElementId id) {
  // Scrollbar positions depend on the current scroll offset.
  SetScrollbarGeometriesNeedUpdate();

  DCHECK(lifecycle().AllowsPropertyTreeAccess());
  ScrollTree& scroll_tree = property_trees()->scroll_tree;
  const auto* scroll_node = scroll_tree.FindNodeFromElementId(id);

  if (!scroll_node) {
    // A scroll node should always exist on the active tree but may not exist
    // if we're updating the other trees from the active tree. This can occur
    // when the pending tree represents a different page, for example.
    DCHECK(!IsActiveTree());
    return;
  }

  DCHECK(scroll_node->transform_id != TransformTree::kInvalidNodeId);
  TransformTree& transform_tree = property_trees()->transform_tree;
  auto* transform_node = transform_tree.Node(scroll_node->transform_id);
  if (transform_node->scroll_offset != scroll_tree.current_scroll_offset(id)) {
    transform_node->scroll_offset = scroll_tree.current_scroll_offset(id);
    transform_node->needs_local_transform_update = true;
    transform_tree.set_needs_update(true);
  }
  transform_node->transform_changed = true;
  property_trees()->changed = true;
  set_needs_update_draw_properties();

  if (IsActiveTree()) {
    // Ensure the other trees are kept in sync.
    if (host_impl_->pending_tree())
      host_impl_->pending_tree()->DidUpdateScrollOffset(id);
    if (host_impl_->recycle_tree())
      host_impl_->recycle_tree()->DidUpdateScrollOffset(id);
  }
}

void LayerTreeImpl::UpdateScrollbarGeometries() {
  if (!IsActiveTree())
    return;

  DCHECK(lifecycle().AllowsPropertyTreeAccess());

  // Layer properties such as bounds should be up-to-date.
  DCHECK(lifecycle().AllowsLayerPropertyAccess());

  if (!scrollbar_geometries_need_update_)
    return;

  for (auto& pair : element_id_to_scrollbar_layer_ids_) {
    ElementId scrolling_element_id = pair.first;

    auto& scroll_tree = property_trees()->scroll_tree;
    auto* scroll_node = scroll_tree.FindNodeFromElementId(scrolling_element_id);
    if (!scroll_node)
      continue;
    gfx::ScrollOffset current_offset =
        scroll_tree.current_scroll_offset(scrolling_element_id);
    gfx::SizeF scrolling_size(scroll_node->bounds);
    gfx::Size bounds_size(scroll_tree.container_bounds(scroll_node->id));

    bool is_viewport_scrollbar = scroll_node->scrolls_inner_viewport ||
                                 scroll_node->scrolls_outer_viewport;
    if (is_viewport_scrollbar) {
      gfx::SizeF viewport_bounds(bounds_size);
      if (scroll_node->scrolls_inner_viewport && OuterViewportScrollLayer()) {
        // Add offset and bounds contribution of outer viewport.
        current_offset += OuterViewportScrollLayer()->CurrentScrollOffset();
        gfx::SizeF outer_viewport_bounds(scroll_tree.container_bounds(
            OuterViewportScrollLayer()->scroll_tree_index()));
        viewport_bounds.SetToMin(outer_viewport_bounds);

        // The scrolling size is only determined by the outer viewport.
        scroll_node = scroll_tree.FindNodeFromElementId(
            OuterViewportScrollLayer()->element_id());
        scrolling_size = gfx::SizeF(scroll_node->bounds);
      } else {
        // Add offset and bounds contribution of inner viewport.
        current_offset += scroll_tree.current_scroll_offset(
            InnerViewportScrollNode()->element_id);
        gfx::SizeF inner_viewport_bounds(
            scroll_tree.container_bounds(InnerViewportScrollNode()->id));
        viewport_bounds.SetToMin(inner_viewport_bounds);
      }
      viewport_bounds.Scale(1 / current_page_scale_factor());
      bounds_size = ToCeiledSize(viewport_bounds);
    }

    for (auto* scrollbar : ScrollbarsFor(scrolling_element_id)) {
      if (scrollbar->orientation() == HORIZONTAL) {
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

  scrollbar_geometries_need_update_ = false;
}

const RenderSurfaceImpl* LayerTreeImpl::RootRenderSurface() const {
  return property_trees_.effect_tree.GetRenderSurface(
      EffectTree::kContentsRootNodeId);
}

bool LayerTreeImpl::LayerListIsEmpty() const {
  return layer_list_.empty();
}

void LayerTreeImpl::SetRootLayerForTesting(std::unique_ptr<LayerImpl> layer) {
  if (root_layer_for_testing_ && layer.get() != root_layer_for_testing_)
    RemoveLayer(root_layer_for_testing_->id());
  root_layer_for_testing_ = layer.get();
  ClearLayerList();
  if (layer) {
    AddLayer(std::move(layer));
    BuildLayerListForTesting();
  }
  host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::OnCanDrawStateChangedForTree() {
  host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::AddToLayerList(LayerImpl* layer) {
  layer_list_.push_back(layer);
}

void LayerTreeImpl::ClearLayerList() {
  layer_list_.clear();
}

void LayerTreeImpl::BuildLayerListForTesting() {
  ClearLayerList();
  LayerListIterator<LayerImpl> it(root_layer_for_testing_);
  for (; it != LayerListIterator<LayerImpl>(nullptr); ++it) {
    AddToLayerList(*it);
  }
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
    for (auto* picture_layer : picture_layers_) {
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

bool LayerTreeImpl::IsRootLayer(const LayerImpl* layer) const {
  return layer_list_.empty() ? false : layer_list_[0] == layer;
}

gfx::ScrollOffset LayerTreeImpl::TotalScrollOffset() const {
  gfx::ScrollOffset offset;
  auto& scroll_tree = property_trees()->scroll_tree;

  if (InnerViewportScrollNode()) {
    offset += scroll_tree.current_scroll_offset(
        InnerViewportScrollNode()->element_id);
  }

  if (OuterViewportScrollLayer())
    offset += OuterViewportScrollLayer()->CurrentScrollOffset();

  return offset;
}

gfx::ScrollOffset LayerTreeImpl::TotalMaxScrollOffset() const {
  gfx::ScrollOffset offset;
  const ScrollTree& scroll_tree = property_trees()->scroll_tree;

  if (auto* inner_node = InnerViewportScrollNode())
    offset += scroll_tree.MaxScrollOffset(inner_node->id);

  if (auto* outer_node = OuterViewportScrollNode())
    offset += scroll_tree.MaxScrollOffset(outer_node->id);

  return offset;
}

std::unique_ptr<OwnedLayerImplList> LayerTreeImpl::DetachLayers() {
  root_layer_for_testing_ = nullptr;
  layer_list_.clear();
  render_surface_list_.clear();
  set_needs_update_draw_properties();
  std::unique_ptr<OwnedLayerImplList> ret = std::move(layers_);
  layers_.reset(new OwnedLayerImplList);
  return ret;
}

void LayerTreeImpl::SetPropertyTrees(PropertyTrees* property_trees) {
  std::vector<std::unique_ptr<RenderSurfaceImpl>> old_render_surfaces;
  property_trees_.effect_tree.TakeRenderSurfaces(&old_render_surfaces);
  property_trees_ = *property_trees;
  bool render_surfaces_changed =
      property_trees_.effect_tree.CreateOrReuseRenderSurfaces(
          &old_render_surfaces, this);
  if (render_surfaces_changed)
    set_needs_update_draw_properties();
  property_trees->effect_tree.PushCopyRequestsTo(&property_trees_.effect_tree);
  property_trees_.is_main_thread = false;
  property_trees_.is_active = IsActiveTree();
  property_trees_.transform_tree.set_source_to_parent_updates_allowed(false);
  // The value of some effect node properties (like is_drawn) depends on
  // whether we are on the active tree or not. So, we need to update the
  // effect tree.
  if (IsActiveTree())
    property_trees_.effect_tree.set_needs_update(true);
}

void LayerTreeImpl::PushPropertyTreesTo(LayerTreeImpl* target_tree) {
  TRACE_EVENT0("cc", "LayerTreeImpl::PushPropertyTreesTo");
  // Property trees may store damage status. We preserve the active tree
  // damage status by pushing the damage status from active tree property
  // trees to pending tree property trees or by moving it onto the layers.
  if (target_tree->property_trees()->changed) {
    if (property_trees()->sequence_number ==
        target_tree->property_trees()->sequence_number)
      target_tree->property_trees()->PushChangeTrackingTo(property_trees());
    else
      target_tree->MoveChangeTrackingToLayers();
  }

  // To maintain the current scrolling node we need to use element ids which
  // are stable across the property tree update in SetPropertyTrees.
  ElementId scrolling_element_id;
  if (ScrollNode* scrolling_node = target_tree->CurrentlyScrollingNode())
    scrolling_element_id = scrolling_node->element_id;

  target_tree->SetPropertyTrees(&property_trees_);

  const ScrollNode* scrolling_node = nullptr;
  if (scrolling_element_id) {
    auto& scroll_tree = target_tree->property_trees()->scroll_tree;
    scrolling_node = scroll_tree.FindNodeFromElementId(scrolling_element_id);
  }
  target_tree->SetCurrentlyScrollingNode(scrolling_node);
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
  target_tree->property_trees()->scroll_tree.PushScrollUpdatesFromPendingTree(
      &property_trees_, target_tree);

  if (next_activation_forces_redraw_) {
    target_tree->ForceRedrawNextActivation();
    next_activation_forces_redraw_ = false;
  }

  target_tree->PassSwapPromises(std::move(swap_promise_list_));
  swap_promise_list_.clear();

  target_tree->set_browser_controls_shrink_blink_size(
      browser_controls_shrink_blink_size_);
  target_tree->SetTopControlsHeight(top_controls_height_);
  target_tree->SetBottomControlsHeight(bottom_controls_height_);
  target_tree->PushBrowserControls(nullptr);

  target_tree->set_overscroll_behavior(overscroll_behavior_);

  // The page scale factor update can affect scrolling which requires that
  // these ids are set, so this must be before PushPageScaleFactorAndLimits.
  target_tree->SetViewportLayersFromIds(viewport_layer_ids_);

  // Active tree already shares the page_scale_factor object with pending
  // tree so only the limits need to be provided.
  target_tree->PushPageScaleFactorAndLimits(nullptr, min_page_scale_factor(),
                                            max_page_scale_factor());

  target_tree->SetRasterColorSpace(raster_color_space_id_, raster_color_space_);
  target_tree->elastic_overscroll()->PushPendingToActive();

  target_tree->set_content_source_id(content_source_id());

  target_tree->set_painted_device_scale_factor(painted_device_scale_factor());
  target_tree->SetDeviceScaleFactor(device_scale_factor());
  target_tree->SetDeviceViewportSize(device_viewport_size_);
  target_tree->SetViewportVisibleRect(viewport_visible_rect_);

  if (TakeNewLocalSurfaceIdRequest())
    target_tree->RequestNewLocalSurfaceId();
  target_tree->SetLocalSurfaceIdFromParent(
      local_surface_id_from_parent(),
      local_surface_id_allocation_time_from_parent_);

  target_tree->pending_page_scale_animation_ =
      std::move(pending_page_scale_animation_);

  target_tree->RegisterSelection(selection_);

  // This should match the property synchronization in
  // LayerTreeHost::finishCommitOnImplThread().
  target_tree->set_source_frame_number(source_frame_number());
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

  target_tree->has_ever_been_drawn_ = false;

  // Note: this needs to happen after SetPropertyTrees.
  target_tree->HandleTickmarksVisibilityChange();
  target_tree->HandleScrollbarShowRequestsFromMain();
  target_tree->AddPresentationCallbacks(std::move(presentation_callbacks_));
}

void LayerTreeImpl::HandleTickmarksVisibilityChange() {
  if (!host_impl_->ViewportMainScrollLayer())
    return;

  ScrollbarAnimationController* controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          OuterViewportScrollLayer()->element_id());

  if (!controller)
    return;

  for (ScrollbarLayerImplBase* scrollbar : controller->Scrollbars()) {
    if (scrollbar->orientation() != VERTICAL)
      continue;

    // Android Overlay Scrollbar don't have FindInPage Tickmarks.
    if (scrollbar->GetScrollbarAnimator() != LayerTreeSettings::AURA_OVERLAY)
      DCHECK(!scrollbar->HasFindInPageTickmarks());

    controller->UpdateTickmarksVisibility(scrollbar->HasFindInPageTickmarks());
  }
}

void LayerTreeImpl::HandleScrollbarShowRequestsFromMain() {
  LayerTreeHostCommon::CallFunctionForEveryLayer(this, [this](
                                                           LayerImpl* layer) {
    if (!layer->needs_show_scrollbars())
      return;
    ScrollbarAnimationController* controller =
        host_impl_->ScrollbarAnimationControllerForElementId(
            layer->element_id());
    if (controller) {
      controller->DidRequestShowFromMainThread();
      layer->set_needs_show_scrollbars(false);
    }
  });
}

void LayerTreeImpl::MoveChangeTrackingToLayers() {
  // We need to update the change tracking on property trees before we move it
  // onto the layers.
  property_trees_.UpdateChangeTracking();
  for (auto* layer : *this) {
    if (layer->LayerPropertyChangedFromPropertyTrees())
      layer->NoteLayerPropertyChangedFromPropertyTrees();
  }
  EffectTree& effect_tree = property_trees_.effect_tree;
  for (int id = EffectTree::kContentsRootNodeId;
       id < static_cast<int>(effect_tree.size()); ++id) {
    RenderSurfaceImpl* render_surface = effect_tree.GetRenderSurface(id);
    if (render_surface && render_surface->AncestorPropertyChanged())
      render_surface->NoteAncestorPropertyChanged();
  }
}

void LayerTreeImpl::ForceRecalculateRasterScales() {
  for (auto* layer : picture_layers_)
    layer->ResetRasterScale();
}

LayerImplList::const_iterator LayerTreeImpl::begin() const {
  return layer_list_.cbegin();
}

LayerImplList::const_iterator LayerTreeImpl::end() const {
  return layer_list_.cend();
}

LayerImplList::const_reverse_iterator LayerTreeImpl::rbegin() const {
  return layer_list_.crbegin();
}

LayerImplList::const_reverse_iterator LayerTreeImpl::rend() const {
  return layer_list_.crend();
}

LayerImplList::reverse_iterator LayerTreeImpl::rbegin() {
  return layer_list_.rbegin();
}

LayerImplList::reverse_iterator LayerTreeImpl::rend() {
  return layer_list_.rend();
}

bool LayerTreeImpl::IsElementInLayerList(ElementId element_id) const {
  return elements_in_layer_list_.count(element_id);
}

ElementListType LayerTreeImpl::GetElementTypeForAnimation() const {
  return IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
}

void LayerTreeImpl::AddToElementLayerList(ElementId element_id,
                                          LayerImpl* layer) {
  DCHECK(layer);
  DCHECK(layer->element_id() == element_id);

  if (!element_id)
    return;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("layer-element"),
               "LayerTreeImpl::AddToElementLayerList", "element",
               element_id.AsValue().release());

#if DCHECK_IS_ON()
  bool element_id_collision_detected =
      elements_in_layer_list_.count(element_id);

  DCHECK(!element_id_collision_detected);
#endif

  elements_in_layer_list_.insert(element_id);

  host_impl_->mutator_host()->RegisterElement(element_id,
                                              GetElementTypeForAnimation());

  if (layer->scrollable())
    AddScrollableLayer(layer);
}

void LayerTreeImpl::RemoveFromElementLayerList(ElementId element_id) {
  if (!element_id)
    return;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("layer-element"),
               "LayerTreeImpl::RemoveFromElementLayerList", "element",
               element_id.AsValue().release());

  host_impl_->mutator_host()->UnregisterElement(element_id,
                                                GetElementTypeForAnimation());

  elements_in_layer_list_.erase(element_id);
  element_id_to_scrollable_layer_.erase(element_id);
}

void LayerTreeImpl::AddScrollableLayer(LayerImpl* layer) {
  DCHECK(layer);
  DCHECK(layer->scrollable());

  if (!layer->element_id())
    return;

  DCHECK(!element_id_to_scrollable_layer_.count(layer->element_id()));
  element_id_to_scrollable_layer_.insert(
      std::make_pair(layer->element_id(), layer));
}

void LayerTreeImpl::SetTransformMutated(ElementId element_id,
                                        const gfx::Transform& transform) {
  DCHECK_EQ(1u, property_trees()->element_id_to_transform_node_index.count(
                    element_id));
  element_id_to_transform_animations_[element_id] = transform;
  if (property_trees()->transform_tree.OnTransformAnimated(element_id,
                                                           transform))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::SetOpacityMutated(ElementId element_id, float opacity) {
  DCHECK_EQ(
      1u, property_trees()->element_id_to_effect_node_index.count(element_id));
  element_id_to_opacity_animations_[element_id] = opacity;
  if (property_trees()->effect_tree.OnOpacityAnimated(element_id, opacity))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::SetFilterMutated(ElementId element_id,
                                     const FilterOperations& filters) {
  DCHECK_EQ(
      1u, property_trees()->element_id_to_effect_node_index.count(element_id));
  element_id_to_filter_animations_[element_id] = filters;
  if (property_trees()->effect_tree.OnFilterAnimated(element_id, filters))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::AddPresentationCallbacks(
    std::vector<LayerTreeHost::PresentationTimeCallback> callbacks) {
  std::copy(std::make_move_iterator(callbacks.begin()),
            std::make_move_iterator(callbacks.end()),
            std::back_inserter(presentation_callbacks_));
}

std::vector<LayerTreeHost::PresentationTimeCallback>
LayerTreeImpl::TakePresentationCallbacks() {
  std::vector<LayerTreeHost::PresentationTimeCallback> callbacks;
  callbacks.swap(presentation_callbacks_);
  return callbacks;
}

ScrollNode* LayerTreeImpl::CurrentlyScrollingNode() {
  DCHECK(IsActiveTree());
  return property_trees_.scroll_tree.CurrentlyScrollingNode();
}

const ScrollNode* LayerTreeImpl::CurrentlyScrollingNode() const {
  return property_trees_.scroll_tree.CurrentlyScrollingNode();
}

int LayerTreeImpl::LastScrolledScrollNodeIndex() const {
  return last_scrolled_scroll_node_index_;
}

void LayerTreeImpl::SetCurrentlyScrollingNode(const ScrollNode* node) {
  if (node)
    last_scrolled_scroll_node_index_ = node->id;

  ScrollTree& scroll_tree = property_trees()->scroll_tree;
  ScrollNode* old_node = scroll_tree.CurrentlyScrollingNode();

  ElementId old_element_id = old_node ? old_node->element_id : ElementId();
  ElementId new_element_id = node ? node->element_id : ElementId();
  if (old_element_id == new_element_id)
    return;

  scroll_tree.set_currently_scrolling_node(node ? node->id
                                                : ScrollTree::kInvalidNodeId);
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

  auto element_id_to_opacity = element_id_to_opacity_animations_.begin();
  while (element_id_to_opacity != element_id_to_opacity_animations_.end()) {
    const ElementId id = element_id_to_opacity->first;
    EffectNode* node = property_trees_.effect_tree.FindNodeFromElementId(id);
    if (!node || !node->is_currently_animating_opacity ||
        node->opacity == element_id_to_opacity->second) {
      element_id_to_opacity_animations_.erase(element_id_to_opacity++);
      continue;
    }
    node->opacity = element_id_to_opacity->second;
    property_trees_.effect_tree.set_needs_update(true);
    ++element_id_to_opacity;
  }

  auto element_id_to_filter = element_id_to_filter_animations_.begin();
  while (element_id_to_filter != element_id_to_filter_animations_.end()) {
    const ElementId id = element_id_to_filter->first;
    EffectNode* node = property_trees_.effect_tree.FindNodeFromElementId(id);
    if (!node || !node->is_currently_animating_filter ||
        node->filters == element_id_to_filter->second) {
      element_id_to_filter_animations_.erase(element_id_to_filter++);
      continue;
    }
    node->filters = element_id_to_filter->second;
    property_trees_.effect_tree.set_needs_update(true);
    ++element_id_to_filter;
  }

  auto element_id_to_transform = element_id_to_transform_animations_.begin();
  while (element_id_to_transform != element_id_to_transform_animations_.end()) {
    const ElementId id = element_id_to_transform->first;
    TransformNode* node =
        property_trees_.transform_tree.FindNodeFromElementId(id);
    if (!node || !node->is_currently_animating ||
        node->local == element_id_to_transform->second) {
      element_id_to_transform_animations_.erase(element_id_to_transform++);
      continue;
    }
    node->local = element_id_to_transform->second;
    node->needs_local_transform_update = true;
    property_trees_.transform_tree.set_needs_update(true);
    ++element_id_to_transform;
  }

  for (auto transform_it : property_trees()->element_id_to_transform_node_index)
    UpdateTransformAnimation(transform_it.first, transform_it.second);
}

void LayerTreeImpl::UpdateTransformAnimation(ElementId element_id,
                                             int transform_node_index) {
  // This includes all animations, even those that are finished but
  // haven't yet been deleted.
  if (mutator_host()->HasAnyAnimationTargetingProperty(
          element_id, TargetProperty::TRANSFORM)) {
    TransformTree& transform_tree = property_trees()->transform_tree;
    if (TransformNode* node = transform_tree.Node(transform_node_index)) {
      ElementListType list_type = GetElementTypeForAnimation();
      bool has_potential_animation =
          mutator_host()->HasPotentiallyRunningTransformAnimation(element_id,
                                                                  list_type);
      if (node->has_potential_animation != has_potential_animation) {
        node->has_potential_animation = has_potential_animation;
        node->has_only_translation_animations =
            mutator_host()->HasOnlyTranslationTransforms(element_id, list_type);
        transform_tree.set_needs_update(true);
        set_needs_update_draw_properties();
      }
    }
  }
}

TransformNode* LayerTreeImpl::PageScaleTransformNode() {
  auto* page_scale = PageScaleLayer();
  if (!page_scale)
    return nullptr;

  return property_trees()->transform_tree.Node(
      page_scale->transform_tree_index());
}

void LayerTreeImpl::UpdatePageScaleNode() {
  if (!PageScaleTransformNode()) {
    DCHECK(layer_list_.empty() || current_page_scale_factor() == 1);
    return;
  }

  // When the page scale layer is also the root layer (this happens in the UI
  // compositor), the node should also store the combined scale factor and not
  // just the page scale factor.
  // TODO(bokan): Need to implement this behavior for
  // BlinkGeneratedPropertyTrees. i.e. (no page scale layer).
  float device_scale_factor_for_page_scale_layer = 1.f;
  gfx::Transform device_transform_for_page_scale_layer;
  if (IsRootLayer(PageScaleLayer())) {
    DCHECK(!settings().use_layer_lists);
    device_transform_for_page_scale_layer = host_impl_->DrawTransform();
    device_scale_factor_for_page_scale_layer = device_scale_factor();
  }

  draw_property_utils::UpdatePageScaleFactor(
      property_trees(), PageScaleTransformNode(), current_page_scale_factor(),
      device_scale_factor_for_page_scale_layer,
      device_transform_for_page_scale_layer);
}

void LayerTreeImpl::SetPageScaleOnActiveTree(float active_page_scale) {
  DCHECK(IsActiveTree());
  DCHECK(lifecycle().AllowsPropertyTreeAccess());
  float clamped_page_scale = ClampPageScaleFactorToLimits(active_page_scale);
  // Temporary crash logging for https://crbug.com/845097.
  static bool has_dumped_without_crashing = false;
  if (host_impl_->settings().is_layer_tree_for_subframe &&
      clamped_page_scale != 1.f && !has_dumped_without_crashing) {
    has_dumped_without_crashing = true;
    static auto* psf_oopif_error = base::debug::AllocateCrashKeyString(
        "psf_oopif_error", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        psf_oopif_error, base::StringPrintf("%f", clamped_page_scale));
    base::debug::DumpWithoutCrashing();
  }
  if (page_scale_factor()->SetCurrent(clamped_page_scale)) {
    DidUpdatePageScale();
    UpdatePageScaleNode();
  }
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

  DCHECK(lifecycle().AllowsPropertyTreeAccess());
  if (page_scale_factor)
    UpdatePageScaleNode();
}

void LayerTreeImpl::set_browser_controls_shrink_blink_size(bool shrink) {
  if (browser_controls_shrink_blink_size_ == shrink)
    return;

  browser_controls_shrink_blink_size_ = shrink;
  if (IsActiveTree())
    host_impl_->UpdateViewportContainerSizes();
}

void LayerTreeImpl::SetTopControlsHeight(float top_controls_height) {
  if (top_controls_height_ == top_controls_height)
    return;

  top_controls_height_ = top_controls_height;
  if (IsActiveTree())
    host_impl_->UpdateViewportContainerSizes();
}

void LayerTreeImpl::SetBottomControlsHeight(float bottom_controls_height) {
  if (bottom_controls_height_ == bottom_controls_height)
    return;

  bottom_controls_height_ = bottom_controls_height;
  if (IsActiveTree())
    host_impl_->UpdateViewportContainerSizes();
}

void LayerTreeImpl::set_overscroll_behavior(
    const OverscrollBehavior& behavior) {
  overscroll_behavior_ = behavior;
}

bool LayerTreeImpl::ClampBrowserControlsShownRatio() {
  float ratio = top_controls_shown_ratio_->Current(true);
  ratio = std::max(ratio, 0.f);
  ratio = std::min(ratio, 1.f);
  return top_controls_shown_ratio_->SetCurrent(ratio);
}

bool LayerTreeImpl::SetCurrentBrowserControlsShownRatio(float ratio) {
  bool changed = top_controls_shown_ratio_->SetCurrent(ratio);
  changed |= ClampBrowserControlsShownRatio();
  return changed;
}

void LayerTreeImpl::PushBrowserControlsFromMainThread(
    float top_controls_shown_ratio) {
  PushBrowserControls(&top_controls_shown_ratio);
}

void LayerTreeImpl::PushBrowserControls(const float* top_controls_shown_ratio) {
  DCHECK(top_controls_shown_ratio || IsActiveTree());

  if (top_controls_shown_ratio) {
    DCHECK(!IsActiveTree() || !host_impl_->pending_tree());
    top_controls_shown_ratio_->PushMainToPending(*top_controls_shown_ratio);
  }
  if (IsActiveTree()) {
    bool changed_active = top_controls_shown_ratio_->PushPendingToActive();
    changed_active |= ClampBrowserControlsShownRatio();
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
  if (IsActiveTree())
    page_scale_factor()->SetCurrent(
        ClampPageScaleFactorToLimits(current_page_scale_factor()));

  set_needs_update_draw_properties();

  // Viewport scrollbar sizes depend on the page scale factor.
  SetScrollbarGeometriesNeedUpdate();

  if (IsActiveTree()) {
    if (settings().scrollbar_flash_after_any_scroll_update) {
      host_impl_->FlashAllScrollbars(true);
      return;
    }
    if (host_impl_->ViewportMainScrollLayer()) {
      if (ScrollbarAnimationController* controller =
              host_impl_->ScrollbarAnimationControllerForElementId(
                  OuterViewportScrollLayer()->element_id()))
        controller->DidScrollUpdate();
    }
  }
}

void LayerTreeImpl::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  set_needs_update_draw_properties();
  if (IsActiveTree())
    host_impl_->SetViewportDamage(GetDeviceViewport());
  host_impl_->SetNeedUpdateGpuRasterizationStatus();
}

void LayerTreeImpl::SetLocalSurfaceIdFromParent(
    const viz::LocalSurfaceId& local_surface_id_from_parent,
    base::TimeTicks local_surface_id_allocation_time_from_parent) {
  local_surface_id_from_parent_ = local_surface_id_from_parent;
  local_surface_id_allocation_time_from_parent_ =
      local_surface_id_allocation_time_from_parent;
}

void LayerTreeImpl::RequestNewLocalSurfaceId() {
  new_local_surface_id_request_ = true;
}

bool LayerTreeImpl::TakeNewLocalSurfaceIdRequest() {
  bool new_local_surface_id_request = new_local_surface_id_request_;
  new_local_surface_id_request_ = false;
  return new_local_surface_id_request;
}

void LayerTreeImpl::SetDeviceViewportSize(
    const gfx::Size& device_viewport_size) {
  if (device_viewport_size == device_viewport_size_)
    return;
  device_viewport_size_ = device_viewport_size;

  set_needs_update_draw_properties();
  if (!IsActiveTree())
    return;

  host_impl_->UpdateViewportContainerSizes();
  host_impl_->OnCanDrawStateChangedForTree();
  host_impl_->SetViewportDamage(GetDeviceViewport());
}

void LayerTreeImpl::SetViewportVisibleRect(const gfx::Rect& visible_rect) {
  if (visible_rect == viewport_visible_rect_)
    return;

  viewport_visible_rect_ = visible_rect;

  set_needs_update_draw_properties();
  if (IsActiveTree())
    host_impl_->SetViewportDamage(GetDeviceViewport());
}

gfx::Rect LayerTreeImpl::GetDeviceViewport() const {
  // TODO(fsamuel): We should plumb |external_viewport| similar to the
  // way we plumb |device_viewport_size_|.
  const gfx::Rect& external_viewport = host_impl_->external_viewport();
  if (external_viewport.IsEmpty())
    return gfx::Rect(device_viewport_size_);
  return external_viewport;
}

void LayerTreeImpl::SetRasterColorSpace(
    int raster_color_space_id,
    const gfx::ColorSpace& raster_color_space) {
  if (raster_color_space == raster_color_space_)
    return;
  raster_color_space_id_ = raster_color_space_id;
  raster_color_space_ = raster_color_space;
}

SyncedProperty<ScaleGroup>* LayerTreeImpl::page_scale_factor() {
  return page_scale_factor_.get();
}

const SyncedProperty<ScaleGroup>* LayerTreeImpl::page_scale_factor() const {
  return page_scale_factor_.get();
}

gfx::SizeF LayerTreeImpl::ScrollableViewportSize() const {
  auto* inner_node = InnerViewportScrollNode();
  if (!inner_node)
    return gfx::SizeF();

  return gfx::ScaleSize(gfx::SizeF(inner_node->container_bounds),
                        1.0f / current_page_scale_factor());
}

gfx::Rect LayerTreeImpl::RootScrollLayerDeviceViewportBounds() const {
  LayerImpl* root_scroll_layer = OuterViewportScrollLayer()
                                     ? OuterViewportScrollLayer()
                                     : InnerViewportScrollLayer();
  if (!root_scroll_layer)
    return gfx::Rect();
  return MathUtil::MapEnclosingClippedRect(
      root_scroll_layer->ScreenSpaceTransform(),
      gfx::Rect(root_scroll_layer->bounds()));
}

void LayerTreeImpl::ApplySentScrollAndScaleDeltasFromAbortedCommit() {
  DCHECK(IsActiveTree());

  page_scale_factor()->AbortCommit();
  top_controls_shown_ratio()->AbortCommit();
  elastic_overscroll()->AbortCommit();

  if (layer_list_.empty())
    return;

  property_trees()->scroll_tree.ApplySentScrollDeltasFromAbortedCommit();
}

void LayerTreeImpl::SetViewportLayersFromIds(const ViewportLayerIds& ids) {
  if (viewport_layer_ids_ == ids)
    return;

  viewport_layer_ids_ = ids;

  // Set the viewport layer types.
  if (auto* inner_container = InnerViewportContainerLayer())
    inner_container->SetViewportLayerType(INNER_VIEWPORT_CONTAINER);
  if (auto* inner_scroll = InnerViewportScrollLayer())
    inner_scroll->SetViewportLayerType(INNER_VIEWPORT_SCROLL);
  if (auto* outer_container = OuterViewportContainerLayer())
    outer_container->SetViewportLayerType(OUTER_VIEWPORT_CONTAINER);
  if (auto* outer_scroll = OuterViewportScrollLayer())
    outer_scroll->SetViewportLayerType(OUTER_VIEWPORT_SCROLL);
}

void LayerTreeImpl::ClearViewportLayers() {
  SetViewportLayersFromIds(ViewportLayerIds());
}

const ScrollNode* LayerTreeImpl::InnerViewportScrollNode() const {
  auto* inner_scroll = InnerViewportScrollLayer();
  if (!inner_scroll)
    return nullptr;

  return property_trees()->scroll_tree.Node(inner_scroll->scroll_tree_index());
}

const ScrollNode* LayerTreeImpl::OuterViewportScrollNode() const {
  if (!OuterViewportScrollLayer())
    return nullptr;
  return property_trees()->scroll_tree.Node(
      OuterViewportScrollLayer()->scroll_tree_index());
}

// For unit tests, we use the layer's id as its element id.
static void SetElementIdForTesting(LayerImpl* layer) {
  layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
}

void LayerTreeImpl::SetElementIdsForTesting() {
  LayerListIterator<LayerImpl> it(root_layer_for_testing_);
  for (; it != LayerListIterator<LayerImpl>(nullptr); ++it) {
    if (!it->element_id())
      SetElementIdForTesting(*it);
  }
}

bool LayerTreeImpl::UpdateDrawProperties(
    bool update_image_animation_controller) {
  if (!needs_update_draw_properties_)
    return true;

  TRACE_EVENT0("cc,benchmark", "LayerTreeImpl::UpdateDrawProperties");

  // Ensure the scrollbar geometries are up-to-date for hit testing and quads
  // generation. This may cause damage on the scrollbar layers which is why
  // it occurs before we reset |needs_update_draw_properties_|.
  UpdateScrollbarGeometries();

  // Calling UpdateDrawProperties must clear this flag, so there can be no
  // early outs before this.
  needs_update_draw_properties_ = false;

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
    LayerTreeHostCommon::CalcDrawPropsImplInputs inputs(
        layer_list_[0], GetDeviceViewport().size(), host_impl_->DrawTransform(),
        device_scale_factor(), current_page_scale_factor(), PageScaleLayer(),
        InnerViewportScrollLayer(), OuterViewportScrollLayer(),
        elastic_overscroll()->Current(IsActiveTree()),
        OverscrollElasticityElementId(), max_texture_size(),
        settings().layer_transforms_should_scale_layer_contents,
        &render_surface_list_, &property_trees_, PageScaleTransformNode());
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    if (const char* client_name = GetClientNameForMetrics()) {
      UMA_HISTOGRAM_COUNTS_1M(
          base::StringPrintf(
              "Compositing.%s.LayerTreeImpl.CalculateDrawPropertiesUs",
              client_name),
          timer.Elapsed().InMicroseconds());
      UMA_HISTOGRAM_COUNTS_100(
          base::StringPrintf("Compositing.%s.NumRenderSurfaces", client_name),
          base::saturated_cast<int>(render_surface_list_.size()));
    }
  }

  {
    TRACE_EVENT2("cc,benchmark",
                 "LayerTreeImpl::UpdateDrawProperties::Occlusion", "IsActive",
                 IsActiveTree(), "SourceFrameNumber", source_frame_number_);
    OcclusionTracker occlusion_tracker(RootRenderSurface()->content_rect());
    occlusion_tracker.set_minimum_tracking_size(
        settings().minimum_occlusion_tracking_size);

    for (EffectTreeLayerListIterator it(this);
         it.state() != EffectTreeLayerListIterator::State::END; ++it) {
      occlusion_tracker.EnterLayer(it);

      if (it.state() == EffectTreeLayerListIterator::State::LAYER) {
        LayerImpl* layer = it.current_layer();
        layer->draw_properties().occlusion_in_content_space =
            occlusion_tracker.GetCurrentOcclusionForLayer(
                layer->DrawTransform());
      }

      if (it.state() ==
          EffectTreeLayerListIterator::State::CONTRIBUTING_SURFACE) {
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
          const EffectNode* effect_node = property_trees()->effect_tree.Node(
              render_surface->EffectTreeIndex());
          draw_property_utils::ConcatInverseSurfaceContentsScale(
              effect_node, &draw_transform);
        }

        Occlusion occlusion =
            occlusion_tracker.GetCurrentOcclusionForContributingSurface(
                draw_transform);
        render_surface->set_occlusion_in_content_space(occlusion);
        // Masks are used to draw the contributing surface, so should have
        // the same occlusion as the surface (nothing inside the surface
        // occludes them).
        if (LayerImpl* mask = render_surface->MaskLayer()) {
          mask->draw_properties().occlusion_in_content_space =
              occlusion_tracker.GetCurrentOcclusionForContributingSurface(
                  draw_transform * render_surface->SurfaceScale());
        }
      }

      occlusion_tracker.LeaveLayer(it);
    }

    unoccluded_screen_space_region_ =
        occlusion_tracker.ComputeVisibleRegionInScreen(this);
  }

  // Resourceless draw do not need tiles and should not affect existing tile
  // priorities.
  if (!is_in_resourceless_software_draw_mode()) {
    TRACE_EVENT_BEGIN2(
        "cc,benchmark", "LayerTreeImpl::UpdateDrawProperties::UpdateTiles",
        "IsActive", IsActiveTree(), "SourceFrameNumber", source_frame_number_);
    size_t layers_updated_count = 0;
    bool tile_priorities_updated = false;
    for (PictureLayerImpl* layer : picture_layers_) {
      if (!layer->HasValidTilePriorities())
        continue;
      ++layers_updated_count;
      tile_priorities_updated |= layer->UpdateTiles();
    }

    if (tile_priorities_updated)
      DidModifyTilePriorities();

    TRACE_EVENT_END1("cc,benchmark",
                     "LayerTreeImpl::UpdateDrawProperties::UpdateTiles",
                     "layers_updated_count", layers_updated_count);
  }

  if (update_image_animation_controller && image_animation_controller()) {
    image_animation_controller()->UpdateStateFromDrivers(
        CurrentBeginFrameArgs().frame_time);
  }

  DCHECK(!needs_update_draw_properties_)
      << "CalcDrawProperties should not set_needs_update_draw_properties()";
  return true;
}

void LayerTreeImpl::UpdateCanUseLCDText() {
  // If this is not the sync tree, then it is not safe to update lcd text
  // as it causes invalidations and the tiles may be in use.
  DCHECK(IsSyncTree());
  bool tile_priorities_updated = false;
  for (auto* layer : picture_layers_)
    tile_priorities_updated |= layer->UpdateCanUseLCDTextAfterCommit();
  if (tile_priorities_updated)
    DidModifyTilePriorities();
}

void LayerTreeImpl::BuildLayerListAndPropertyTreesForTesting() {
  BuildLayerListForTesting();
  BuildPropertyTreesForTesting();
}

void LayerTreeImpl::BuildPropertyTreesForTesting() {
  SetElementIdsForTesting();
  property_trees_.needs_rebuild = true;
  property_trees_.transform_tree.set_source_to_parent_updates_allowed(true);
  PropertyTreeBuilder::BuildPropertyTrees(
      layer_list_[0], PageScaleLayer(), InnerViewportScrollLayer(),
      OuterViewportScrollLayer(), OverscrollElasticityElementId(),
      elastic_overscroll()->Current(IsActiveTree()),
      current_page_scale_factor(), device_scale_factor(),
      gfx::Rect(GetDeviceViewport().size()), host_impl_->DrawTransform(),
      &property_trees_);
  property_trees_.transform_tree.set_source_to_parent_updates_allowed(false);
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
  LayerImpl* root_scroll_layer = nullptr;
  LayerImpl* root_container_layer = nullptr;
  if (OuterViewportScrollLayer()) {
    root_scroll_layer = OuterViewportScrollLayer();
    root_container_layer = OuterViewportContainerLayer();
  } else if (InnerViewportScrollLayer()) {
    root_scroll_layer = InnerViewportScrollLayer();
    root_container_layer = InnerViewportContainerLayer();
  }

  if (!root_scroll_layer || !root_container_layer)
    return gfx::SizeF();

  gfx::SizeF content_size = root_scroll_layer->BoundsForScrolling();
  content_size.SetToMax(root_container_layer->BoundsForScrolling());
  return content_size;
}

LayerImpl* LayerTreeImpl::LayerById(int id) const {
  auto iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

LayerImpl* LayerTreeImpl::ScrollableLayerByElementId(
    ElementId element_id) const {
  auto iter = element_id_to_scrollable_layer_.find(element_id);
  return iter != element_id_to_scrollable_layer_.end() ? iter->second : nullptr;
}

void LayerTreeImpl::SetSurfaceRanges(
    const base::flat_set<viz::SurfaceRange> surface_ranges) {
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
  DCHECK(!IsActiveTree()) << "The active tree does not push layer properties";
  // TODO(crbug.com/303943): PictureLayerImpls always push properties so should
  // not go into this set or we'd push them twice.
  DCHECK(!base::ContainsValue(picture_layers_, layer));
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

// These manage ownership of the LayerImpl.
void LayerTreeImpl::AddLayer(std::unique_ptr<LayerImpl> layer) {
  DCHECK(!base::ContainsValue(*layers_, layer));
  DCHECK(layer);
  layers_->push_back(std::move(layer));
  set_needs_update_draw_properties();
}

std::unique_ptr<LayerImpl> LayerTreeImpl::RemoveLayer(int id) {
  for (auto it = layers_->begin(); it != layers_->end(); ++it) {
    if ((*it) && (*it)->id() != id)
      continue;
    std::unique_ptr<LayerImpl> ret = std::move(*it);
    set_needs_update_draw_properties();
    layers_->erase(it);
    return ret;
  }
  return nullptr;
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

  if (!layer_list_.empty()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        this, [](LayerImpl* layer) { layer->DidBecomeActive(); });
  }

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

viz::ContextProvider* LayerTreeImpl::context_provider() const {
  return host_impl_->layer_tree_frame_sink()->context_provider();
}

viz::ClientResourceProvider* LayerTreeImpl::resource_provider() const {
  return host_impl_->resource_provider();
}

TileManager* LayerTreeImpl::tile_manager() const {
  return host_impl_->tile_manager();
}

ImageDecodeCache* LayerTreeImpl::image_decode_cache() const {
  return host_impl_->image_decode_cache();
}

ImageAnimationController* LayerTreeImpl::image_animation_controller() const {
  return host_impl_->image_animation_controller();
}

FrameRateCounter* LayerTreeImpl::frame_rate_counter() const {
  return host_impl_->fps_counter();
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
  return host_impl_->pinch_gesture_active();
}

viz::BeginFrameArgs LayerTreeImpl::CurrentBeginFrameArgs() const {
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
  DCHECK(!settings().scrollbar_fade_delay.is_zero());
  DCHECK(!settings().scrollbar_fade_duration.is_zero());
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
      return ScrollbarAnimationController::
          CreateScrollbarAnimationControllerAuraOverlay(
              scroll_element_id, host_impl_, fade_delay, fade_duration,
              thinning_duration, initial_opacity);
    }
    case LayerTreeSettings::NO_ANIMATOR:
      NOTREACHED();
      break;
  }
  return nullptr;
}

void LayerTreeImpl::DidAnimateScrollOffset() {
  host_impl_->DidAnimateScrollOffset();
}

bool LayerTreeImpl::use_gpu_rasterization() const {
  return host_impl_->use_gpu_rasterization();
}

GpuRasterizationStatus LayerTreeImpl::GetGpuRasterizationStatus() const {
  return host_impl_->gpu_rasterization_status();
}

bool LayerTreeImpl::create_low_res_tiling() const {
  return host_impl_->create_low_res_tiling();
}

void LayerTreeImpl::SetNeedsRedraw() {
  host_impl_->SetNeedsRedraw();
}

void LayerTreeImpl::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
  for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
    LayerImpl* layer_impl = *it;
    if (!layer_impl->contributes_to_drawn_render_surface())
      continue;
    layer_impl->GetAllPrioritizedTilesForTracing(prioritized_tiles);
  }
}

void LayerTreeImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  viz::TracedValue::MakeDictIntoImplicitSnapshot(state, "cc::LayerTreeImpl",
                                                 this);
  state->SetInteger("source_frame_number", source_frame_number_);

  state->BeginArray("render_surface_layer_list");
  for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
    if (!(*it)->contributes_to_drawn_render_surface())
      continue;
    viz::TracedValue::AppendIDRef(*it, state);
  }
  state->EndArray();

  state->BeginArray("swap_promise_trace_ids");
  for (const auto& swap_promise : swap_promise_list_)
    state->AppendDouble(swap_promise->TraceId());
  state->EndArray();

  state->BeginArray("pinned_swap_promise_trace_ids");
  for (const auto& swap_promise : pinned_swap_promise_list_)
    state->AppendDouble(swap_promise->TraceId());
  state->EndArray();

  state->BeginArray("layers");
  for (auto* layer : *this) {
    state->BeginDictionary();
    layer->AsValueInto(state);
    state->EndDictionary();
  }
  state->EndArray();
}

bool LayerTreeImpl::DistributeRootScrollOffset(
    const gfx::ScrollOffset& desired_root_offset) {
  if (!InnerViewportScrollNode() || !OuterViewportScrollLayer())
    return false;

  gfx::ScrollOffset root_offset = desired_root_offset;
  ScrollTree& scroll_tree = property_trees()->scroll_tree;

  // If we get here, we have both inner/outer viewports, and need to distribute
  // the scroll offset between them.
  gfx::ScrollOffset inner_viewport_offset =
      scroll_tree.current_scroll_offset(InnerViewportScrollNode()->element_id);
  gfx::ScrollOffset outer_viewport_offset =
      OuterViewportScrollLayer()->CurrentScrollOffset();
  DCHECK(inner_viewport_offset + outer_viewport_offset == TotalScrollOffset());

  // Setting the root scroll offset is driven by user actions so prevent
  // it if it is not user scrollable in certain directions.
  if (!InnerViewportScrollNode()->user_scrollable_horizontal)
    root_offset.set_x(inner_viewport_offset.x() + outer_viewport_offset.x());

  if (!InnerViewportScrollNode()->user_scrollable_vertical)
    root_offset.set_y(inner_viewport_offset.y() + outer_viewport_offset.y());

  // It may be nothing has changed.
  if (inner_viewport_offset + outer_viewport_offset == root_offset)
    return false;

  gfx::ScrollOffset max_outer_viewport_scroll_offset =
      OuterViewportScrollLayer()->MaxScrollOffset();

  outer_viewport_offset = root_offset - inner_viewport_offset;
  outer_viewport_offset.SetToMin(max_outer_viewport_scroll_offset);
  outer_viewport_offset.SetToMax(gfx::ScrollOffset());

  OuterViewportScrollLayer()->SetCurrentScrollOffset(outer_viewport_offset);
  inner_viewport_offset = root_offset - outer_viewport_offset;
  if (scroll_tree.SetScrollOffset(InnerViewportScrollNode()->element_id,
                                  inner_viewport_offset))
    DidUpdateScrollOffset(InnerViewportScrollNode()->element_id);
  return true;
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
  for (auto& swap_promise : swap_promise_list_)
    swap_promise->DidNotSwap(SwapPromise::SWAP_FAILS);
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
  for (auto& swap_promise : swap_promise_list_)
    swap_promise->DidNotSwap(reason);
  swap_promise_list_.clear();

  for (auto& swap_promise : pinned_swap_promise_list_)
    swap_promise->DidNotSwap(reason);
  pinned_swap_promise_list_.clear();
}

void LayerTreeImpl::DidModifyTilePriorities() {
  host_impl_->DidModifyTilePriorities();
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
  for (const auto& req : ui_resource_request_queue_) {
    switch (req.GetType()) {
      case UIResourceRequest::UI_RESOURCE_CREATE:
        host_impl_->CreateUIResource(req.GetId(), req.GetBitmap());
        break;
      case UIResourceRequest::UI_RESOURCE_DELETE:
        host_impl_->DeleteUIResource(req.GetId());
        break;
      case UIResourceRequest::UI_RESOURCE_INVALID_REQUEST:
        NOTREACHED();
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
  DCHECK(!base::ContainsValue(picture_layers_, layer));
  picture_layers_.push_back(layer);
}

void LayerTreeImpl::UnregisterPictureLayerImpl(PictureLayerImpl* layer) {
  auto it = std::find(picture_layers_.begin(), picture_layers_.end(), layer);
  DCHECK(it != picture_layers_.end());
  picture_layers_.erase(it);
}

void LayerTreeImpl::RegisterScrollbar(ScrollbarLayerImplBase* scrollbar_layer) {
  ElementId scroll_element_id = scrollbar_layer->scroll_element_id();
  if (!scroll_element_id)
    return;

  auto* scrollbar_ids = &element_id_to_scrollbar_layer_ids_[scroll_element_id];
  int* scrollbar_layer_id = scrollbar_layer->orientation() == HORIZONTAL
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
    scrollbar_layer_id = scrollbar_layer->orientation() == HORIZONTAL
                             ? &scrollbar_ids->horizontal
                             : &scrollbar_ids->vertical;
  }

  *scrollbar_layer_id = scrollbar_layer->id();

  if (IsActiveTree() && scrollbar_layer->is_overlay_scrollbar() &&
      scrollbar_layer->GetScrollbarAnimator() !=
          LayerTreeSettings::NO_ANIMATOR) {
    host_impl_->RegisterScrollbarAnimationController(
        scroll_element_id, scrollbar_layer->Opacity());
  }

  // The new scrollbar's geometries need to be initialized.
  SetScrollbarGeometriesNeedUpdate();
}

void LayerTreeImpl::UnregisterScrollbar(
    ScrollbarLayerImplBase* scrollbar_layer) {
  ElementId scroll_element_id = scrollbar_layer->scroll_element_id();
  if (!scroll_element_id)
    return;

  auto& scrollbar_ids = element_id_to_scrollbar_layer_ids_[scroll_element_id];
  if (scrollbar_layer->orientation() == HORIZONTAL)
    scrollbar_ids.horizontal = Layer::INVALID_ID;
  else
    scrollbar_ids.vertical = Layer::INVALID_ID;

  if (scrollbar_ids.horizontal == Layer::INVALID_ID &&
      scrollbar_ids.vertical == Layer::INVALID_ID) {
    element_id_to_scrollbar_layer_ids_.erase(scroll_element_id);
    if (IsActiveTree()) {
      host_impl_->UnregisterScrollbarAnimationController(scroll_element_id);
    }
  }
}

ScrollbarSet LayerTreeImpl::ScrollbarsFor(ElementId scroll_element_id) const {
  ScrollbarSet scrollbars;
  auto it = element_id_to_scrollbar_layer_ids_.find(scroll_element_id);
  if (it != element_id_to_scrollbar_layer_ids_.end()) {
    const ScrollbarLayerIds& layer_ids = it->second;
    if (layer_ids.horizontal != Layer::INVALID_ID)
      scrollbars.insert(LayerById(layer_ids.horizontal)->ToScrollbarLayer());
    if (layer_ids.vertical != Layer::INVALID_ID)
      scrollbars.insert(LayerById(layer_ids.vertical)->ToScrollbarLayer());
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
  gfx::Transform inverse_local_space_to_screen_space(
      gfx::Transform::kSkipInitialization);
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
    gfx::Point3F planar_point_in_screen_space(planar_point);
    local_space_to_screen_space_transform.TransformPoint(
        &planar_point_in_screen_space);
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
  const ClipTree& clip_tree = property_trees->clip_tree;
  const TransformTree& transform_tree = property_trees->transform_tree;
  const ClipNode* clip_node = clip_tree.Node(1);
  gfx::Rect clip = gfx::ToEnclosingRect(clip_node->clip);
  if (!PointHitsRect(screen_space_point, gfx::Transform(), clip, nullptr))
    return true;

  for (const ClipNode* clip_node = clip_tree.Node(layer->clip_tree_index());
       clip_node->id > ClipTree::kViewportNodeId;
       clip_node = clip_tree.parent(clip_node)) {
    if (clip_node->clip_type == ClipNode::ClipType::APPLIES_LOCAL_CLIP) {
      gfx::Rect clip = gfx::ToEnclosingRect(clip_node->clip);

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
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
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
  FindClosestMatchingLayerState()
      : closest_match(nullptr),
        closest_distance(-std::numeric_limits<float>::infinity()) {}
  LayerImpl* closest_match;
  // Note that the positive z-axis points towards the camera, so bigger means
  // closer in this case, counterintuitively.
  float closest_distance;
};

template <typename Functor>
static void FindClosestMatchingLayer(const gfx::PointF& screen_space_point,
                                     LayerImpl* root_layer,
                                     const Functor& func,
                                     FindClosestMatchingLayerState* state) {
  base::ElapsedTimer timer;
  // We want to iterate from front to back when hit testing.
  for (auto* layer : base::Reversed(*root_layer->layer_tree_impl())) {
    if (!func(layer))
      continue;

    float distance_to_intersection = 0.f;
    bool hit = false;
    if (layer->Is3dSorted())
      hit =
          PointHitsLayer(layer, screen_space_point, &distance_to_intersection);
    else
      hit = PointHitsLayer(layer, screen_space_point, nullptr);

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
  if (const char* client_name = GetClientNameForMetrics()) {
    UMA_HISTOGRAM_COUNTS_1M(
        base::StringPrintf("Compositing.%s.HitTestTimeToFindClosestLayer",
                           client_name),
        timer.Elapsed().InMicroseconds());
  }
}

struct FindScrollingLayerOrScrollbarFunctor {
  bool operator()(LayerImpl* layer) const {
    return layer->scrollable() || layer->is_scrollbar();
  }
};

LayerImpl* LayerTreeImpl::FindFirstScrollingLayerOrScrollbarThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  if (layer_list_.empty())
    return nullptr;

  FindClosestMatchingLayerState state;
  LayerImpl* root_layer = layer_list_[0];
  FindClosestMatchingLayer(screen_space_point, root_layer,
                           FindScrollingLayerOrScrollbarFunctor(), &state);
  return state.closest_match;
}

struct HitTestVisibleScrollableOrTouchableFunctor {
  bool operator()(LayerImpl* layer) const {
    return layer->scrollable() || layer->should_hit_test() ||
           !layer->touch_action_region().region().IsEmpty();
  }
};

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  if (layer_list_.empty())
    return nullptr;
  if (!UpdateDrawProperties())
    return nullptr;
  FindClosestMatchingLayerState state;
  FindClosestMatchingLayer(screen_space_point, layer_list_[0],
                           HitTestVisibleScrollableOrTouchableFunctor(),
                           &state);
  return state.closest_match;
}

struct FindTouchEventLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    return PointHitsRegion(screen_space_point, layer->ScreenSpaceTransform(),
                           layer->touch_action_region().region(), layer);
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
  if (!UpdateDrawProperties())
    return nullptr;
  FindClosestMatchingLayerState state;
  FindClosestMatchingLayer(screen_space_point, layer_list_[0], func, &state);
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

  auto layer_top = gfx::PointF(layer_bound.edge_top);
  auto layer_bottom = gfx::PointF(layer_bound.edge_bottom);
  gfx::Transform screen_space_transform = layer->ScreenSpaceTransform();

  bool clipped = false;
  gfx::PointF screen_top =
      MathUtil::MapPoint(screen_space_transform, layer_top, &clipped);
  gfx::PointF screen_bottom =
      MathUtil::MapPoint(screen_space_transform, layer_bottom, &clipped);

  // MapPoint can produce points with NaN components (even when no inputs are
  // NaN). Since consumers of gfx::SelectionBounds may round |edge_top| or
  // |edge_bottom| (and since rounding will crash on NaN), we return an empty
  // bound instead.
  if (std::isnan(screen_top.x()) || std::isnan(screen_top.y()) ||
      std::isnan(screen_bottom.x()) || std::isnan(screen_bottom.y()))
    return gfx::SelectionBound();

  const float inv_scale = 1.f / device_scale_factor;
  viewport_bound.SetEdgeTop(gfx::ScalePoint(screen_top, inv_scale));
  viewport_bound.SetEdgeBottom(gfx::ScalePoint(screen_bottom, inv_scale));

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
    gfx::Vector2dF visibility_offset = layer_top - layer_bottom;
    visibility_offset.Scale(device_scale_factor / visibility_offset.Length());
    gfx::PointF visibility_point = layer_bottom + visibility_offset;
    if (visibility_point.x() <= 0)
      visibility_point.set_x(visibility_point.x() + device_scale_factor);
    visibility_point =
        MathUtil::MapPoint(screen_space_transform, visibility_point, &clipped);

    float intersect_distance = 0.f;
    viewport_bound.set_visible(
        PointHitsLayer(layer, visibility_point, &intersect_distance));
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

bool LayerTreeImpl::IsActivelyScrolling() const {
  return host_impl_->IsActivelyScrolling();
}

void LayerTreeImpl::SetPendingPageScaleAnimation(
    std::unique_ptr<PendingPageScaleAnimation> pending_animation) {
  pending_page_scale_animation_ = std::move(pending_animation);
}

std::unique_ptr<PendingPageScaleAnimation>
LayerTreeImpl::TakePendingPageScaleAnimation() {
  return std::move(pending_page_scale_animation_);
}

void LayerTreeImpl::ResetAllChangeTracking() {
  layers_that_should_push_properties_.clear();
  // Iterate over all layers, including masks.
  for (auto& layer : *layers_)
    layer->ResetChangeTracking();
  property_trees_.ResetAllChangeTracking();
}

}  // namespace cc
