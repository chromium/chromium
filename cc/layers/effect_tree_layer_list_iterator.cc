// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/effect_tree_layer_list_iterator.h"

#include "base/notreached.h"

namespace cc {

EffectTreeLayerListIterator::EffectTreeLayerListIterator(
    LayerTreeImpl* layer_tree_impl)
    : state_(EffectTreeLayerListIterator::State::kEnd),
      layer_list_iterator_(layer_tree_impl->rbegin()),
      current_effect_tree_index_(kInvalidPropertyNodeId),
      next_effect_tree_index_(kInvalidPropertyNodeId),
      lowest_common_effect_tree_ancestor_index_(kInvalidPropertyNodeId),
      layer_tree_impl_(layer_tree_impl),
      effect_tree_(&layer_tree_impl->property_trees()->effect_tree_mutable()) {
  // Find the front-most drawn layer.
  while (layer_list_iterator_ != layer_tree_impl->rend() &&
         !(*layer_list_iterator_)->contributes_to_drawn_render_surface()) {
    ++layer_list_iterator_;
  }

  // If there are no drawn layers, start at the root render surface, if it
  // exists.
  if (layer_list_iterator_ == layer_tree_impl->rend()) {
    DCHECK(effect_tree_->size() > kContentsRootPropertyNodeId);
    state_ = State::kTargetSurface;
    current_effect_tree_index_ = kContentsRootPropertyNodeId;
  } else {
    state_ = State::kLayer;
    current_effect_tree_index_ =
        (*layer_list_iterator_)->render_target_effect_tree_index();
    next_effect_tree_index_ = current_effect_tree_index_;
    lowest_common_effect_tree_ancestor_index_ = current_effect_tree_index_;
  }
}

EffectTreeLayerListIterator::EffectTreeLayerListIterator(
    const EffectTreeLayerListIterator& iterator) = default;

EffectTreeLayerListIterator::~EffectTreeLayerListIterator() = default;

void EffectTreeLayerListIterator::operator++() {
  switch (state_) {
    case State::kLayer:
      // Find the next drawn layer.
      ++layer_list_iterator_;
      while (layer_list_iterator_ != layer_tree_impl_->rend() &&
             !(*layer_list_iterator_)->contributes_to_drawn_render_surface()) {
        ++layer_list_iterator_;
      }
      if (layer_list_iterator_ == layer_tree_impl_->rend()) {
        next_effect_tree_index_ = kInvalidPropertyNodeId;
        lowest_common_effect_tree_ancestor_index_ = kInvalidPropertyNodeId;
        state_ = State::kTargetSurface;
        break;
      }

      next_effect_tree_index_ =
          (*layer_list_iterator_)->render_target_effect_tree_index();

      // If the next drawn layer has a different target effect tree index, check
      // for surfaces whose contributors have all been visited.
      if (next_effect_tree_index_ != current_effect_tree_index_) {
        lowest_common_effect_tree_ancestor_index_ =
            effect_tree_->LowestCommonAncestorWithRenderSurface(
                current_effect_tree_index_, next_effect_tree_index_);
        // If the current layer's target effect node is an ancestor of the next
        // layer's target effect node, then the current effect node still has
        // more contributors that need to be visited. Otherwise, all
        // contributors have been visited, so we visit the node's surface next.
        if (current_effect_tree_index_ ==
            lowest_common_effect_tree_ancestor_index_) {
          current_effect_tree_index_ = next_effect_tree_index_;
          lowest_common_effect_tree_ancestor_index_ = next_effect_tree_index_;
        } else {
          state_ = State::kTargetSurface;
        }
      }
      break;
    case State::kTargetSurface:
      if (current_effect_tree_index_ == kContentsRootPropertyNodeId) {
        current_effect_tree_index_ = kInvalidPropertyNodeId;
        state_ = State::kEnd;
        DCHECK(next_effect_tree_index_ == kInvalidPropertyNodeId);
        DCHECK(layer_list_iterator_ == layer_tree_impl_->rend());
      } else {
        state_ = State::kContributingSurface;
      }
      break;
    case State::kContributingSurface:
      DCHECK(current_effect_tree_index_ !=
             lowest_common_effect_tree_ancestor_index_);
      // Step towards the lowest common ancestor.
      current_effect_tree_index_ =
          effect_tree_->Node(current_effect_tree_index_)->target_id;
      if (current_effect_tree_index_ == next_effect_tree_index_) {
        state_ = State::kLayer;
      } else if (current_effect_tree_index_ ==
                 lowest_common_effect_tree_ancestor_index_) {
        // In this case, we know that more content contributes to the current
        // effect node (since the next effect node is a descendant), so we're
        // not yet ready to visit it as a target surface. The same holds for all
        // effect nodes on the path from the current node to the next effect
        // tree node.
        state_ = State::kLayer;
        current_effect_tree_index_ = next_effect_tree_index_;
        lowest_common_effect_tree_ancestor_index_ = next_effect_tree_index_;
      } else {
        // In this case, the lowest common ancestor is a proper ancestor of the
        // current effect node. This means that all contributors to the current
        // effect node have been visited, so we're ready to visit it as a target
        // surface.
        state_ = State::kTargetSurface;
      }
      break;
    case State::kEnd:
      NOTREACHED();
  }
}

}  // namespace cc
