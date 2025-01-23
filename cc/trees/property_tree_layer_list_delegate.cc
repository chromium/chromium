// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_layer_list_delegate.h"

#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

void PropertyTreeLayerListDelegate::UpdatePropertyTreesIfNeeded(
    LayerTreeHost* host) {
  // The property trees are already up-to-date, but the HUD layer is managed
  // outside the layer list sent to the LayerTreeHost and needs to have its
  // property tree state set.
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                       "PropertyTreeLayerListDelegate::"
                       "UpdatePropertyTreesIfNeeded_ReceivedPropertyTrees",
                       TRACE_EVENT_SCOPE_THREAD, "property_trees",
                       host->property_trees()->AsTracedValue());

  // Note that we can't cache the root_layer object because it's not
  // threadsafe to do so.
  if (HeadsUpDisplayLayer* hud_layer = host->hud_layer();
      hud_layer && host->root_layer()) {
    hud_layer->SetTransformTreeIndex(
        host->root_layer()->transform_tree_index());
    hud_layer->SetEffectTreeIndex(host->root_layer()->effect_tree_index());
    hud_layer->SetClipTreeIndex(host->root_layer()->clip_tree_index());
    hud_layer->SetScrollTreeIndex(host->root_layer()->scroll_tree_index());
    hud_layer->set_property_tree_sequence_number(
        host->root_layer()->property_tree_sequence_number());
  }
}

}  // namespace cc
