// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_layer_tree_delegate.h"

#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/property_tree_builder.h"

namespace cc {

void PropertyTreeLayerTreeDelegate::SetLayerTreeHost(LayerTreeHost* host) {
  host_ = host;
}

LayerTreeHost* PropertyTreeLayerTreeDelegate::host() {
  return host_;
}

const LayerTreeHost* PropertyTreeLayerTreeDelegate::host() const {
  return host_;
}

void PropertyTreeLayerTreeDelegate::UpdatePropertyTreesIfNeeded() {
  TRACE_EVENT0("cc",
               "PropertyTreeLayerTreeDelegate::UpdatePropertyTreesIfNeeded");
  PropertyTreeBuilder::BuildPropertyTrees(host());
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                       "PropertyTreeLayerTreeDelegate::"
                       "UpdatePropertyTreesIfNeeded_BuiltPropertyTrees",
                       TRACE_EVENT_SCOPE_THREAD, "property_trees",
                       host()->property_trees()->AsTracedValue());
}

void PropertyTreeLayerTreeDelegate::UpdateScrollOffsetFromImpl(
    const ElementId& id,
    const gfx::Vector2dF& delta,
    ScrollSourceType type,
    const std::optional<TargetSnapAreaElementIds>& snap_target_ids) {
  if (Layer* layer = host()->LayerByElementId(id)) {
    layer->SetScrollOffsetFromImplSide(layer->scroll_offset() + delta);
    host()->SetNeedsUpdateLayers();
  }
}

void PropertyTreeLayerTreeDelegate::OnAnimateLayers() {
  // Animation state changes will require rebuilding property trees to
  // track them.
  host()->property_trees()->set_needs_rebuild(true);
}

void PropertyTreeLayerTreeDelegate::RegisterViewportPropertyIds(
    const ViewportPropertyIds& ids) {
  // This is a no-op in layer tree mode.
}

void PropertyTreeLayerTreeDelegate::OnUnregisterElement(ElementId element_id) {
  host()->mutator_host()->RemoveElementId(element_id);
}

bool PropertyTreeLayerTreeDelegate::IsElementInPropertyTrees(
    ElementId element_id,
    ElementListType list_type) const {
  return list_type == ElementListType::ACTIVE &&
         host_->LayerByElementId(element_id);
}

void PropertyTreeLayerTreeDelegate::OnElementFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& filters) {
  Layer* layer = host()->LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnFilterAnimated(filters);
}

void PropertyTreeLayerTreeDelegate::OnElementBackdropFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& backdrop_filters) {
  Layer* layer = host()->LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnBackdropFilterAnimated(backdrop_filters);
}

void PropertyTreeLayerTreeDelegate::OnElementOpacityMutated(
    ElementId element_id,
    ElementListType list_type,
    float opacity) {
  Layer* layer = host()->LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnOpacityAnimated(opacity);

  if (EffectNode* node = host()->property_trees()->effect_tree_mutable().Node(
          layer->effect_tree_index())) {
    DCHECK_EQ(layer->effect_tree_index(), node->id);
    if (node->opacity == opacity) {
      return;
    }

    node->opacity = opacity;
    host()->property_trees()->effect_tree_mutable().set_needs_update(true);
  }

  host()->SetNeedsUpdateLayers();
}

void PropertyTreeLayerTreeDelegate::OnElementTransformMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::Transform& transform) {
  Layer* layer = host()->LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnTransformAnimated(transform);

  if (layer->has_transform_node()) {
    TransformNode* node =
        host()->property_trees()->transform_tree_mutable().Node(
            layer->transform_tree_index());
    if (node->local == transform) {
      return;
    }

    node->local = transform;
    node->needs_local_transform_update = true;
    node->has_potential_animation = true;
    host()->property_trees()->transform_tree_mutable().set_needs_update(true);
  }

  host()->SetNeedsUpdateLayers();
}

}  // namespace cc
