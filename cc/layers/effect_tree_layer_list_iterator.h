// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_
#define CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_

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
// Surfaces are first visited in state TARGET_SURFACE. Immediately after that,
// every surface other than the root surface is visited in state
// CONTRIBUTING_SURFACE, as it contributes to the next target surface.
//
// The iterator takes on the following states:
// 1. LAYER: The iterator is visiting layer |current_layer()| that contributes
//    to surface |target_render_surface()|.
// 2. TARGET_SURFACE: The iterator is visiting render surface
//    |target_render_surface()|.
// 3. CONTRIBUTING_SURFACE: The iterator is visiting render surface
//    |current_render_surface()| that contributes to surface
//    |target_render_surface()|.
// 4. END: All layers and render surfaces have already been visited.
class CC_EXPORT EffectTreeLayerListIterator {
 public:
  enum class State { LAYER, TARGET_SURFACE, CONTRIBUTING_SURFACE, END };

  explicit EffectTreeLayerListIterator(LayerTreeImpl* layer_tree_impl);
  EffectTreeLayerListIterator(const EffectTreeLayerListIterator& iterator);
  ~EffectTreeLayerListIterator();

  void operator++();

  State state() { return state_; }

  LayerImpl* current_layer() const {
    DCHECK(state_ == State::LAYER);
    return *layer_list_iterator_;
  }

  RenderSurfaceImpl* current_render_surface() const {
    DCHECK(state_ == State::CONTRIBUTING_SURFACE);
    return effect_tree_->GetRenderSurface(current_effect_tree_index_);
  }

  RenderSurfaceImpl* target_render_surface() const {
    switch (state_) {
      case State::LAYER:
      case State::TARGET_SURFACE:
        return effect_tree_->GetRenderSurface(current_effect_tree_index_);
      case State::CONTRIBUTING_SURFACE: {
        int target_node_id =
            effect_tree_->Node(current_effect_tree_index_)->target_id;
        return effect_tree_->GetRenderSurface(target_node_id);
      }
      case State::END:
        NOTREACHED();
    }
    NOTREACHED();
    return nullptr;
  }

  struct Position {
    State state = State::END;
    LayerImpl* current_layer = nullptr;
    RenderSurfaceImpl* current_render_surface = nullptr;
    RenderSurfaceImpl* target_render_surface = nullptr;
  };

  operator const Position() const {
    Position position;
    if (state_ == State::END)
      return position;

    position.state = state_;
    position.target_render_surface = target_render_surface();
    if (state_ == State::LAYER)
      position.current_layer = current_layer();
    else if (state_ == State::CONTRIBUTING_SURFACE)
      position.current_render_surface = current_render_surface();

    return position;
  }

 private:
  State state_;

  // When in state LAYER, this is the layer that's currently being visited.
  // Otherwise, this is the layer that will be visited the next time we're in
  // state LAYER.
  LayerTreeImpl::const_reverse_iterator layer_list_iterator_;

  // When in state LAYER, this is the render target effect tree index for the
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

  LayerTreeImpl* layer_tree_impl_;
  EffectTree* effect_tree_;
};

}  // namespace cc

#endif  // CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_
