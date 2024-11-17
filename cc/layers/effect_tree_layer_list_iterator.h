// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_
#define CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "cc/cc_export.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"

namespace cc {

class LayerImpl;
class LayerTreeImpl;

// This iterates over layers and render surfaces in front-to-back order (that
// is, in reverse-draw-order). Only layers that draw content to some render
// surface are visited. A render surface is visited immediately after all
// layers and surfaces that contribute content to that surface are visited.
// Surfaces are first visited in state kTargetSurface. Immediately after that,
// every surface other than the root surface is visited in state
// kContributingSurface, as it contributes to the next target surface.
//
// The iterator takes on the following states:
// 1. kLayer: The iterator is visiting layer |current_layer()| that contributes
//    to surface |target_render_surface()|.
// 2. kTargetSurface: The iterator is visiting render surface
//    |target_render_surface()|.
// 3. kContributingSurface: The iterator is visiting render surface
//    |current_render_surface()| that contributes to surface
//    |target_render_surface()|.
// 4. kEnd: All layers and render surfaces have already been visited.
class CC_EXPORT EffectTreeLayerListIterator {
 public:
  enum class State { kLayer, kTargetSurface, kContributingSurface, kEnd };

  explicit EffectTreeLayerListIterator(LayerTreeImpl* layer_tree_impl);
  EffectTreeLayerListIterator(const EffectTreeLayerListIterator& iterator);
  ~EffectTreeLayerListIterator();

  void operator++();

  State state() { return state_; }

  LayerImpl* current_layer() const {
    DCHECK(state_ == State::kLayer);
    return *layer_list_iterator_;
  }

  RenderSurfaceImpl* current_render_surface() const {
    DCHECK(state_ == State::kContributingSurface);
    return effect_tree_->GetRenderSurface(current_effect_tree_index_);
  }

  RenderSurfaceImpl* target_render_surface() const {
    switch (state_) {
      case State::kLayer:
      case State::kTargetSurface:
        return effect_tree_->GetRenderSurface(current_effect_tree_index_);
      case State::kContributingSurface: {
        int target_node_id =
            effect_tree_->Node(current_effect_tree_index_)->target_id;
        return effect_tree_->GetRenderSurface(target_node_id);
      }
      case State::kEnd:
        NOTREACHED();
    }
    NOTREACHED();
  }

  struct Position {
    State state = State::kEnd;
    // RAW_PTR_EXCLUSION: Renderer performance: visible in sampling profiler
    // stacks.
    RAW_PTR_EXCLUSION LayerImpl* current_layer = nullptr;
    RAW_PTR_EXCLUSION RenderSurfaceImpl* current_render_surface = nullptr;
    RAW_PTR_EXCLUSION RenderSurfaceImpl* target_render_surface = nullptr;
  };

  operator const Position() const {
    Position position;
    if (state_ == State::kEnd) {
      return position;
    }

    position.state = state_;
    position.target_render_surface = target_render_surface();
    if (state_ == State::kLayer) {
      position.current_layer = current_layer();
    } else if (state_ == State::kContributingSurface) {
      position.current_render_surface = current_render_surface();
    }

    return position;
  }

 private:
  State state_;

  // When in state kLayer, this is the layer that's currently being visited.
  // Otherwise, this is the layer that will be visited the next time we're in
  // state kLayer.
  LayerTreeImpl::const_reverse_iterator layer_list_iterator_;

  // When in state kLayer, this is the render target effect tree index for the
  // currently visited layer. Otherwise, this is the the effect tree index of
  // the currently visited render surface.
  int current_effect_tree_index_;

  // Render target effect tree index for the layer currently visited by
  // layer_list_iterator_.
  int next_effect_tree_index_;

  // The index in the effect tree of the lowest common ancestor
  // current_effect_tree_index_ and next_effect_tree_index_, that has a
  // render surface.
  int lowest_common_effect_tree_ancestor_index_;

  // RAW_PTR_EXCLUSION: Renderer performance: visible in sampling profiler
  // stacks.
  RAW_PTR_EXCLUSION LayerTreeImpl* layer_tree_impl_;
  RAW_PTR_EXCLUSION EffectTree* effect_tree_;
};

}  // namespace cc

#endif  // CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_
