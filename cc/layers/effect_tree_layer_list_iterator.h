// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_
#define CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_

#include "base/check.h"
#include "base/check_op.h"
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

  State state() const { return state_; }

  LayerImpl* current_layer() const {
    DCHECK(state_ == State::kLayer);
    return LayerAtCursor();
  }

  int current_effect_tree_index() const {
    DCHECK(state_ == State::kContributingSurface);
    return current_effect_tree_index_;
  }

  RenderSurfaceImpl* current_render_surface() const {
    DCHECK(state_ == State::kContributingSurface);
    return effect_tree().GetRenderSurface(current_effect_tree_index_);
  }

  int target_effect_tree_index() const {
    switch (state_) {
      case State::kLayer:
      case State::kTargetSurface:
        return current_effect_tree_index_;
      case State::kContributingSurface:
        return effect_tree().Node(current_effect_tree_index_).target_id;
      case State::kEnd:
        NOTREACHED();
    }
    NOTREACHED();
  }

  RenderSurfaceImpl* target_render_surface() const {
    return effect_tree().GetRenderSurface(target_effect_tree_index());
  }

 private:
  // The effect tree is looked up through the owning LayerTreeImpl rather than
  // cached as a separate pointer. This is only an address computation off of
  // |layer_tree_impl_|, so it doesn't introduce an extra memory load.
  EffectTree& effect_tree() const {
    return layer_tree_impl_->property_trees()->effect_tree_mutable();
  }

  // Returns the layer at the cursor position (see |layers_remaining_|). The
  // index is bounds-checked against the layer list on every access, so a
  // stale cursor can't be used to read outside of the layer list.
  LayerImpl* LayerAtCursor() const {
    CHECK_GT(layers_remaining_, 0u);
    return layer_tree_impl_->LayerAtIndex(layers_remaining_ - 1);
  }

  // Moves the cursor backwards (towards the front of the draw order) until it
  // points at a layer that contributes to a drawn render surface, or until
  // there are no more layers.
  void SkipLayersNotContributingToDrawnSurface();

  State state_;

  // Reverse cursor into the layer list of |layer_tree_impl_|: the number of
  // layers that have not been passed over yet. The layer at the cursor is at
  // index |layers_remaining_ - 1|, and a value of 0 means that all layers have
  // been visited.
  // When in state kLayer, the layer at the cursor is the layer that's currently
  // being visited. Otherwise, it's the layer that will be visited the next time
  // we're in state kLayer.
  size_t layers_remaining_;

  // When in state kLayer, this is the render target effect tree index for the
  // currently visited layer. Otherwise, this is the the effect tree index of
  // the currently visited render surface.
  int current_effect_tree_index_;

  // Render target effect tree index for the layer at the cursor.
  int next_effect_tree_index_;

  // The index in the effect tree of the lowest common ancestor
  // current_effect_tree_index_ and next_effect_tree_index_, that has a
  // render surface.
  int lowest_common_effect_tree_ancestor_index_;

  // The owning tree. This is the only pointer held by the iterator; all other
  // state (the layer cursor and effect tree node ids) is stored as indices
  // that are bounds-checked against |layer_tree_impl_| on access, and the
  // effect tree is derived from it rather than cached separately.
  // RAW_PTR_EXCLUSION: Renderer performance: visible in sampling profiler
  // stacks. The iterator is short-lived and stack allocated, and must not
  // outlive |layer_tree_impl_|.
  RAW_PTR_EXCLUSION LayerTreeImpl* layer_tree_impl_;
};

}  // namespace cc

#endif  // CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_
