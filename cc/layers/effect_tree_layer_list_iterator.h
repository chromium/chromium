// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_
#define CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_

#include "base/check.h"
#include "base/check_op.h"
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
// 1. kLayer: The iterator is visiting layer |CurrentLayer()| that contributes
//    to surface |TargetRenderSurface()|.
// 2. kTargetSurface: The iterator is visiting render surface
//    |TargetRenderSurface()|.
// 3. kContributingSurface: The iterator is visiting render surface
//    |CurrentRenderSurface()| that contributes to surface
//    |TargetRenderSurface()|.
// 4. kEnd: All layers and render surfaces have already been visited.
//
// The iterator holds no pointers. It only stores indices into the layer list
// and the effect tree. Every method that needs the tree takes it as an
// argument, and all of those calls must pass the same LayerTreeImpl that was
// given to the constructor. Each index is bounds-checked against the tree when
// it is used, so the iterator can't read outside of the layer list or the
// effect tree.
//
// Typical usage:
//   for (EffectTreeLayerListIterator it(tree);
//        it.state() != EffectTreeLayerListIterator::State::kEnd;
//        it.Advance(tree)) {
//     ...
//   }
class CC_EXPORT EffectTreeLayerListIterator {
 public:
  enum class State { kLayer, kTargetSurface, kContributingSurface, kEnd };

  explicit EffectTreeLayerListIterator(const LayerTreeImpl& layer_tree_impl);
  EffectTreeLayerListIterator(const EffectTreeLayerListIterator& iterator);
  EffectTreeLayerListIterator& operator=(
      const EffectTreeLayerListIterator& iterator);
  ~EffectTreeLayerListIterator();

  // Moves to the next step of the traversal.
  void Advance(const LayerTreeImpl& layer_tree_impl);

  State state() const { return state_; }

  LayerImpl* CurrentLayer(const LayerTreeImpl& layer_tree_impl) const {
    DCHECK(state_ == State::kLayer);
    return LayerAtCursor(layer_tree_impl);
  }

  int current_effect_tree_index() const {
    DCHECK(state_ == State::kContributingSurface);
    return current_effect_tree_index_;
  }

  RenderSurfaceImpl* CurrentRenderSurface(
      LayerTreeImpl& layer_tree_impl) const {
    DCHECK(state_ == State::kContributingSurface);
    return GetEffectTree(layer_tree_impl)
        .GetRenderSurface(current_effect_tree_index_);
  }

  int TargetEffectTreeIndex(const LayerTreeImpl& layer_tree_impl) const {
    switch (state_) {
      case State::kLayer:
      case State::kTargetSurface:
        return current_effect_tree_index_;
      case State::kContributingSurface:
        return GetEffectTree(layer_tree_impl)
            .Node(current_effect_tree_index_)
            .target_id;
      case State::kEnd:
        NOTREACHED();
    }
    NOTREACHED();
  }

  RenderSurfaceImpl* TargetRenderSurface(LayerTreeImpl& layer_tree_impl) const {
    return GetEffectTree(layer_tree_impl)
        .GetRenderSurface(TargetEffectTreeIndex(layer_tree_impl));
  }
  const RenderSurfaceImpl* TargetRenderSurface(
      const LayerTreeImpl& layer_tree_impl) const {
    return GetEffectTree(layer_tree_impl)
        .GetRenderSurface(TargetEffectTreeIndex(layer_tree_impl));
  }

 private:
  static const EffectTree& GetEffectTree(const LayerTreeImpl& layer_tree_impl) {
    return layer_tree_impl.property_trees()->effect_tree();
  }
  static EffectTree& GetEffectTree(LayerTreeImpl& layer_tree_impl) {
    return layer_tree_impl.property_trees()->effect_tree_mutable();
  }

  // Returns the layer at the cursor position (see |layers_remaining_|). The
  // index is bounds-checked against the layer list on every access, so a
  // stale cursor can't be used to read outside of the layer list.
  LayerImpl* LayerAtCursor(const LayerTreeImpl& layer_tree_impl) const {
    CHECK_GT(layers_remaining_, 0u);
    return layer_tree_impl.LayerAtIndex(layers_remaining_ - 1);
  }

  // Moves the cursor backwards (towards the front of the draw order) until it
  // points at a layer that contributes to a drawn render surface, or until
  // there are no more layers.
  void SkipLayersNotContributingToDrawnSurface(
      const LayerTreeImpl& layer_tree_impl);

  State state_;

  // Reverse cursor into the layer list: the number of layers that have not
  // been passed over yet. The layer at the cursor is at index
  // |layers_remaining_ - 1|, and a value of 0 means that all layers have been
  // visited.
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
};

}  // namespace cc

#endif  // CC_LAYERS_EFFECT_TREE_LAYER_LIST_ITERATOR_H_
