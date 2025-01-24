// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_layer_tree_delegate.h"

#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/property_tree_builder.h"

namespace cc {

void PropertyTreeLayerTreeDelegate::UpdatePropertyTreesIfNeeded(
    LayerTreeHost* host) {
  TRACE_EVENT0("cc",
               "PropertyTreeLayerTreeDelegate::UpdatePropertyTreesIfNeeded");
  PropertyTreeBuilder::BuildPropertyTrees(host);
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                       "PropertyTreeLayerTreeDelegate::"
                       "UpdatePropertyTreesIfNeeded_BuiltPropertyTrees",
                       TRACE_EVENT_SCOPE_THREAD, "property_trees",
                       host->property_trees()->AsTracedValue());
}

void PropertyTreeLayerTreeDelegate::UpdateScrollOffsetFromImpl(
    LayerTreeHost* host,
    const ElementId& id,
    const gfx::Vector2dF& delta,
    const std::optional<TargetSnapAreaElementIds>& snap_target_ids) {
  if (Layer* layer = host->LayerByElementId(id)) {
    layer->SetScrollOffsetFromImplSide(layer->scroll_offset() + delta);
    host->SetNeedsUpdateLayers();
  }
}

}  // namespace cc
