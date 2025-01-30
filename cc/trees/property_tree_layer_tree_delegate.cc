// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_layer_tree_delegate.h"

#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/property_tree_builder.h"

namespace cc {

void PropertyTreeLayerTreeDelegate::SetLayerTreeHost(LayerTreeHost* host) {
  host_ = host;
}

LayerTreeHost* PropertyTreeLayerTreeDelegate::host() {
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

}  // namespace cc
