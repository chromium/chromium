// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/tree_synchronizer.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {
#if DCHECK_IS_ON()
void AssertValidPropertyTreeIndices(const Layer* layer,
                                    const PropertyTrees& property_trees) {
  DCHECK(layer);
  DCHECK(layer->transform_tree_index_is_valid(property_trees));
  DCHECK(layer->effect_tree_index_is_valid(property_trees));
  DCHECK(layer->clip_tree_index_is_valid(property_trees));
  DCHECK(layer->scroll_tree_index_is_valid(property_trees));
}

static void AssertValidPropertyTreeIndices(const LayerImpl* layer) {
  DCHECK(layer);
  DCHECK_NE(layer->transform_tree_index(), kInvalidPropertyNodeId);
  DCHECK_NE(layer->effect_tree_index(), kInvalidPropertyNodeId);
  DCHECK_NE(layer->clip_tree_index(), kInvalidPropertyNodeId);
  DCHECK_NE(layer->scroll_tree_index(), kInvalidPropertyNodeId);
}

static bool LayerHasValidPropertyTreeIndices(const LayerImpl* layer) {
  DCHECK(layer);
  return layer->transform_tree_index() != kInvalidPropertyNodeId &&
         layer->effect_tree_index() != kInvalidPropertyNodeId &&
         layer->clip_tree_index() != kInvalidPropertyNodeId &&
         layer->scroll_tree_index() != kInvalidPropertyNodeId;
}

static bool LayerWillPushProperties(const LayerTreeImpl* tree,
                                    const LayerImpl* layer) {
  return std::ranges::contains(tree->LayersThatShouldPushProperties(), layer) ||
         // TODO(crbug.com/40335690): Stop always pushing PictureLayerImpl
         // properties.
         std::ranges::contains(tree->picture_layers(), layer);
}
#endif

}  // namespace

void TreeSynchronizer::SynchronizeTrees(
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state,
    LayerTreeImpl* pending_tree) {
#if DCHECK_IS_ON()
  // Every Layer should have valid property tree indices
  for (const auto* layer : unsafe_state) {
    AssertValidPropertyTreeIndices(layer, commit_state.property_trees);
  }
#endif

  TRACE_EVENT0("cc", "TreeSynchronizer::SynchronizeTrees");
  OwnedLayerImplList recycle_layer_list = pending_tree->DetachLayers();
  OwnedLayerImplList new_layer_list;
  new_layer_list.reserve(unsafe_state.num_layers());
  for (const auto* sync_layer : unsafe_state) {
    std::unique_ptr<LayerImpl> layer_impl =
        recycle_layer_list.ReleaseLayer(sync_layer->id());
    if (!layer_impl) {
      layer_impl = sync_layer->CreateLayerImpl(pending_tree);
    }
    new_layer_list.push_back(std::move(layer_impl));
  }
  pending_tree->SwapLayers(std::move(new_layer_list));

#if DCHECK_IS_ON()
  // Every LayerImpl should have valid property tree indices or be marked for
  // property update.
  for (const auto* layer_impl : *pending_tree) {
    DCHECK(LayerHasValidPropertyTreeIndices(layer_impl) ||
           commit_state.layer_ids_that_should_push_properties.contains(
               layer_impl->id()));
  }
#endif
}

void TreeSynchronizer::SynchronizeTrees(LayerTreeImpl* pending_tree,
                                        LayerTreeImpl* active_tree) {
#if DCHECK_IS_ON()
  // Every Layer should have valid property tree indices
  for (const auto* layer : *pending_tree) {
    AssertValidPropertyTreeIndices(layer);
  }
#endif

  TRACE_EVENT0("cc", "TreeSynchronizer::SynchronizeTrees");
  OwnedLayerImplList recycle_layer_list = active_tree->DetachLayers();
  OwnedLayerImplList new_layer_list;

  new_layer_list.reserve(pending_tree->num_layers());
  for (const auto* sync_layer : *pending_tree) {
    std::unique_ptr<LayerImpl> layer_impl =
        recycle_layer_list.ReleaseLayer(sync_layer->id());
    if (!layer_impl) {
      layer_impl = sync_layer->CreateLayerImpl(active_tree);
    }
    new_layer_list.push_back(std::move(layer_impl));
  }
  active_tree->SwapLayers(std::move(new_layer_list));

#if DCHECK_IS_ON()
  // Every active tree layer should have valid property tree indices or be
  // marked for property update.
  for (const auto* active_layer : *active_tree) {
    DCHECK(LayerHasValidPropertyTreeIndices(active_layer) ||
           LayerWillPushProperties(
               pending_tree, pending_tree->LayerById(active_layer->id())));
  }
#endif
}

void TreeSynchronizer::PushLayerProperties(LayerTreeImpl* pending_tree,
                                           LayerTreeImpl* active_tree) {
  auto layers = pending_tree->LayersThatShouldPushProperties();
  const size_t push_count = layers.size();
  TRACE_EVENT1("cc", "TreeSynchronizer::PushLayerPropertiesTo.Active",
               "layer_count", push_count);

  for (auto* source_layer : layers) {
    LayerImpl* target_layer = active_tree->LayerById(source_layer->id());
    DCHECK(target_layer);
    source_layer->MovePropertiesToActiveLayer(target_layer);
    // Reset any state that should be cleared for the next update.
    source_layer->ResetChangeTracking();
  }

  pending_tree->ClearLayersThatShouldPushProperties();
}

void TreeSynchronizer::PushLayerProperties(
    CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state,
    LayerTreeImpl* impl_tree) {
  TRACE_EVENT1("cc", "TreeSynchronizer::PushLayerPropertiesTo.Main",
               "layer_count",
               commit_state.layer_ids_that_should_push_properties.size());
  for (int layer_id : commit_state.layer_ids_that_should_push_properties) {
    Layer* source_layer = unsafe_state.LayerById(layer_id);
    CHECK(source_layer);
    LayerImpl* target_layer = impl_tree->LayerById(source_layer->id());
    CHECK(target_layer);
    source_layer->PushPropertiesTo(target_layer, commit_state);
  }
}

base::expected<void, std::string> TreeSynchronizer::SynchronizeLayerOrder(
    const std::vector<int32_t>& layer_order,
    LayerTreeImpl& tree) {
  OwnedLayerImplList recycle_layer_list = tree.DetachLayers();
  OwnedLayerImplList new_layer_list;
  new_layer_list.reserve(layer_order.size());
  for (auto id : layer_order) {
    std::unique_ptr<LayerImpl> layer_impl = recycle_layer_list.ReleaseLayer(id);
    if (!layer_impl) {
      return base::unexpected("Invalid or duplicate layer ID");
    }
    new_layer_list.push_back(std::move(layer_impl));
  }
  tree.SwapLayers(std::move(new_layer_list));
  return base::ok();
}

}  // namespace cc
