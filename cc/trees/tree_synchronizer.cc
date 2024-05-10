// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/tree_synchronizer.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {
#if DCHECK_IS_ON()
static void AssertValidPropertyTreeIndices(
    const Layer* layer,
    const PropertyTrees& property_trees) {
  DCHECK(layer);
  DCHECK(layer->transform_tree_index_is_valid(property_trees));
  DCHECK(layer->effect_tree_index_is_valid(property_trees));
  DCHECK(layer->clip_tree_index_is_valid(property_trees));
  DCHECK(layer->scroll_tree_index_is_valid(property_trees));
}

static void AssertValidPropertyTreeIndices(const LayerImpl* layer,
                                           const PropertyTrees&) {
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
  return base::Contains(tree->LayersThatShouldPushProperties(), layer) ||
         // TODO(crbug.com/40335690): Stop always pushing PictureLayerImpl
         // properties.
         base::Contains(tree->picture_layers(), layer);
}
#endif

template <typename LayerType>
std::unique_ptr<LayerImpl> ReuseOrCreateLayerImpl(OwnedLayerImplMap* old_layers,
                                                  const LayerType* layer,
                                                  LayerTreeImpl* tree_impl) {
  if (!layer)
    return nullptr;
  std::unique_ptr<LayerImpl> layer_impl = std::move((*old_layers)[layer->id()]);
  if (!layer_impl)
    layer_impl = layer->CreateLayerImpl(tree_impl);
  return layer_impl;
}

void PushLayerList(OwnedLayerImplMap* old_layers,
                   const CommitState& commit_state,
                   const ThreadUnsafeCommitState& unsafe_state,
                   LayerTreeImpl* tree_impl) {
  DCHECK(tree_impl->LayerListIsEmpty());
  for (const auto* layer : unsafe_state) {
    std::unique_ptr<LayerImpl> layer_impl(
        ReuseOrCreateLayerImpl(old_layers, layer, tree_impl));
    // TODO(crbug.com/40778609): remove diagnostic CHECK
    CHECK(layer_impl);

#if DCHECK_IS_ON()
    // Every layer should have valid property tree indices
    AssertValidPropertyTreeIndices(layer, unsafe_state.property_trees);
    // Every layer_impl should either have valid property tree indices already
    // or the corresponding layer should push them onto layer_impl.
    DCHECK(LayerHasValidPropertyTreeIndices(layer_impl.get()) ||
           commit_state.layers_that_should_push_properties.contains(layer));
#endif

    tree_impl->AddLayer(std::move(layer_impl));
  }
  tree_impl->OnCanDrawStateChangedForTree();
}

void PushLayerList(OwnedLayerImplMap* old_layers,
                   LayerTreeImpl* host,
                   LayerTreeImpl* tree_impl,
                   const PropertyTrees& property_trees) {
  DCHECK(tree_impl->LayerListIsEmpty());
  for (const auto* layer : *host) {
    std::unique_ptr<LayerImpl> layer_impl(
        ReuseOrCreateLayerImpl(old_layers, layer, tree_impl));
    // TODO(crbug.com/40778609): remove diagnostic CHECK
    CHECK(layer_impl);

#if DCHECK_IS_ON()
    // Every layer should have valid property tree indices
    AssertValidPropertyTreeIndices(layer, property_trees);
    // Every layer_impl should either have valid property tree indices already
    // or the corresponding layer should push them onto layer_impl.
    DCHECK(LayerHasValidPropertyTreeIndices(layer_impl.get()) ||
           LayerWillPushProperties(host, layer));
#endif

    tree_impl->AddLayer(std::move(layer_impl));
  }
  tree_impl->OnCanDrawStateChangedForTree();
}

void SynchronizeTreesInternal(const CommitState& commit_state,
                              const ThreadUnsafeCommitState& unsafe_state,
                              LayerTreeImpl* tree_impl) {
  DCHECK(tree_impl);

  TRACE_EVENT0("cc", "TreeSynchronizer::SynchronizeTrees");
  OwnedLayerImplList old_layers = tree_impl->DetachLayers();

  OwnedLayerImplMap old_layer_map;
  for (auto& it : old_layers) {
    DCHECK(it);
    old_layer_map[it->id()] = std::move(it);
  }

  PushLayerList(&old_layer_map, commit_state, unsafe_state, tree_impl);
}

void SynchronizeTreesInternal(LayerTreeImpl* source_tree,
                              LayerTreeImpl* tree_impl,
                              const PropertyTrees& property_trees) {
  DCHECK(tree_impl);

  TRACE_EVENT0("cc", "TreeSynchronizer::SynchronizeTrees");
  OwnedLayerImplList old_layers = tree_impl->DetachLayers();

  OwnedLayerImplMap old_layer_map;
  for (auto& it : old_layers) {
    DCHECK(it);
    old_layer_map[it->id()] = std::move(it);
  }

  PushLayerList(&old_layer_map, source_tree, tree_impl, property_trees);
}

}  // namespace

void TreeSynchronizer::SynchronizeTrees(
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state,
    LayerTreeImpl* tree_impl) {
  if (!unsafe_state.root_layer) {
    tree_impl->DetachLayers();
  } else {
    SynchronizeTreesInternal(commit_state, unsafe_state, tree_impl);
  }
}

void TreeSynchronizer::SynchronizeTrees(LayerTreeImpl* pending_tree,
                                        LayerTreeImpl* active_tree) {
  if (pending_tree->LayerListIsEmpty()) {
    active_tree->DetachLayers();
  } else {
    SynchronizeTreesInternal(pending_tree, active_tree,
                             *pending_tree->property_trees());
  }
}

template <typename Iterator>
static void PushLayerPropertiesInternal(Iterator source_layers_begin,
                                        Iterator source_layers_end,
                                        LayerTreeImpl* target_impl_tree) {
  for (Iterator it = source_layers_begin; it != source_layers_end; ++it) {
    auto& source_layer = *it;
    LayerImpl* target_layer = target_impl_tree->LayerById(source_layer->id());
    DCHECK(target_layer);
    source_layer->PushPropertiesTo(target_layer);
  }
}

void TreeSynchronizer::PushLayerProperties(LayerTreeImpl* pending_tree,
                                           LayerTreeImpl* active_tree) {
  const auto& layers = pending_tree->LayersThatShouldPushProperties();
  const auto& picture_layers = pending_tree->picture_layers();
  const size_t push_count =
      layers.size() + (pending_tree->always_push_properties_on_picture_layers()
                           ? picture_layers.size()
                           : 0);
  TRACE_EVENT1("cc", "TreeSynchronizer::PushLayerPropertiesTo.Impl",
               "layer_count", push_count);
  PushLayerPropertiesInternal(layers.begin(), layers.end(), active_tree);
  if (pending_tree->always_push_properties_on_picture_layers()) {
    // TODO(crbug.com/40335690): Stop always pushing PictureLayerImpl
    // properties.
    PushLayerPropertiesInternal(picture_layers.begin(), picture_layers.end(),
                                active_tree);
  }
  pending_tree->ClearLayersThatShouldPushProperties();
}

void TreeSynchronizer::PushLayerProperties(
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state,
    LayerTreeImpl* impl_tree) {
  TRACE_EVENT1("cc", "TreeSynchronizer::PushLayerPropertiesTo.Main",
               "layer_count",
               commit_state.layers_that_should_push_properties.size());
  auto source_layers_begin =
      commit_state.layers_that_should_push_properties.begin();
  auto source_layers_end =
      commit_state.layers_that_should_push_properties.end();
  for (auto it = source_layers_begin; it != source_layers_end; ++it) {
    auto* source_layer = *it;
    LayerImpl* target_layer = impl_tree->LayerById(source_layer->id());
    DCHECK(target_layer);
    source_layer->PushPropertiesTo(target_layer, commit_state, unsafe_state);
  }
}

}  // namespace cc
