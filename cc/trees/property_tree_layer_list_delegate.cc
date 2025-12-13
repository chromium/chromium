// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_layer_list_delegate.h"

#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host_client.h"

namespace cc {

void PropertyTreeLayerListDelegate::SetLayerTreeHost(LayerTreeHost* host) {
  host_ = host;
}

LayerTreeHost* PropertyTreeLayerListDelegate::host() {
  return host_;
}

const LayerTreeHost* PropertyTreeLayerListDelegate::host() const {
  return host_;
}

void PropertyTreeLayerListDelegate::UpdatePropertyTreesIfNeeded() {
  // The property trees are already up-to-date, but the HUD layer is managed
  // outside the layer list sent to the LayerTreeHost and needs to have its
  // property tree state set.
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                       "PropertyTreeLayerListDelegate::"
                       "UpdatePropertyTreesIfNeeded_ReceivedPropertyTrees",
                       TRACE_EVENT_SCOPE_THREAD, "property_trees",
                       host()->property_trees()->AsTracedValue());

  // Note that we can't cache the root_layer object because it's not
  // threadsafe to do so.
  if (HeadsUpDisplayLayer* hud_layer = host()->hud_layer();
      hud_layer && host()->root_layer()) {
    hud_layer->SetTransformTreeIndex(
        host()->root_layer()->transform_tree_index());
    hud_layer->SetEffectTreeIndex(host()->root_layer()->effect_tree_index());
    hud_layer->SetClipTreeIndex(host()->root_layer()->clip_tree_index());
    hud_layer->SetScrollTreeIndex(host()->root_layer()->scroll_tree_index());
    hud_layer->set_property_tree_sequence_number(
        host()->root_layer()->property_tree_sequence_number());
  }
}

void PropertyTreeLayerListDelegate::UpdateScrollOffsetFromImpl(
    const ElementId& id,
    const gfx::Vector2dF& delta,
    ScrollSourceType type,
    const std::optional<TargetSnapAreaElementIds>& snap_target_ids) {
  auto& scroll_tree = host()->property_trees()->scroll_tree_mutable();
  auto new_offset = scroll_tree.current_scroll_offset(id) + delta;
  TRACE_EVENT_INSTANT2("cc", "NotifyDidScroll", TRACE_EVENT_SCOPE_THREAD,
                       "cur_y", scroll_tree.current_scroll_offset(id).y(),
                       "delta", delta.y());
  if (auto* scroll_node = scroll_tree.FindNodeFromElementId(id)) {
    // This update closely follows
    // blink::PropertyTreeManager::DirectlyUpdateScrollOffsetTransform.

    scroll_tree.SetScrollOffset(id, new_offset);
    // |blink::PropertyTreeManager::DirectlySetScrollOffset| (called from
    // |blink::PropertyTreeManager::DirectlyUpdateScrollOffsetTransform|)
    // marks the layer as needing to push properties in order to clobber
    // animations, but that is not needed for an impl-side scroll.

    // Update the offset in the transform node.
    TransformTree& transform_tree =
        host()->property_trees()->transform_tree_mutable();
    auto* transform_node = transform_tree.Node(scroll_node->transform_id);
    if (transform_node && transform_node->scroll_offset() != new_offset) {
      transform_node->SetScrollOffset(new_offset, DamageReason::kUntracked);
      transform_node->needs_local_transform_update = true;
      transform_node->SetTransformChanged(DamageReason::kUntracked);
      transform_tree.set_needs_update(true);

      // If the scroll was realized on the compositor, then its transform node
      // is already updated (see LayerTreeImpl::DidUpdateScrollOffset) and we
      // are now "catching up" to it on main, so we don't need a commit.
      //
      // But if the scroll should be realized on the main thread, we need a
      // commit to push the transform change.
      if (scroll_tree.ShouldRealizeScrollsOnMain(*scroll_node)) {
        host()->SetNeedsCommit();
      }
    }

    // The transform tree has been modified which requires a call to
    // |LayerTreeHost::UpdateLayers| to update the property trees.
    host()->SetNeedsUpdateLayers();
  }

  scroll_tree.NotifyDidCompositorScroll(id, new_offset, type, snap_target_ids);
}

void PropertyTreeLayerListDelegate::OnAnimateLayers() {
  // This is a no-op in layer list mode.
}

void PropertyTreeLayerListDelegate::RegisterViewportPropertyIds(
    const ViewportPropertyIds& ids) {
  host()->SetViewportPropertyIds(ids);
  // Outer viewport properties exist only if inner viewport property exists.
  DCHECK(ids.inner_scroll != kInvalidPropertyNodeId ||
         (ids.outer_scroll == kInvalidPropertyNodeId &&
          ids.outer_clip == kInvalidPropertyNodeId));
}

void PropertyTreeLayerListDelegate::OnUnregisterElement(ElementId element_id) {
  // This is a no-op in layer list mode.
}

bool PropertyTreeLayerListDelegate::IsElementInPropertyTrees(
    ElementId element_id,
    ElementListType list_type) const {
  return list_type == ElementListType::ACTIVE &&
         host()->property_trees()->HasElement(element_id);
}

void PropertyTreeLayerListDelegate::OnElementFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& filters) {
  // In BlinkGenPropertyTrees/CompositeAfterPaint we always have property
  // tree nodes and can set the filter directly on the effect node.
  host()->property_trees()->effect_tree_mutable().OnFilterAnimated(element_id,
                                                                   filters);
}

void PropertyTreeLayerListDelegate::OnElementBackdropFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& backdrop_filters) {
  // In BlinkGenPropertyTrees/CompositeAfterPaint we always have property
  // tree nodes and can set the backdrop_filter directly on the effect node.
  host()->property_trees()->effect_tree_mutable().OnBackdropFilterAnimated(
      element_id, backdrop_filters);
}

void PropertyTreeLayerListDelegate::OnElementOpacityMutated(
    ElementId element_id,
    ElementListType list_type,
    float opacity) {
  host()->property_trees()->effect_tree_mutable().OnOpacityAnimated(element_id,
                                                                    opacity);
  return;
}

void PropertyTreeLayerListDelegate::OnElementTransformMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::Transform& transform) {
  host()->property_trees()->transform_tree_mutable().OnTransformAnimated(
      element_id, transform);
  return;
}

}  // namespace cc
