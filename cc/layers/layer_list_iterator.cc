// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_list_iterator.h"

#include "cc/layers/layer.h"

namespace cc {

LayerListIterator::LayerListIterator(Layer* root_layer)
    : current_layer_(root_layer) {
  DCHECK(!root_layer || !root_layer->parent());
  list_indices_.push_back(0);
}

LayerListIterator::LayerListIterator(const LayerListIterator& other) = default;

LayerListIterator& LayerListIterator::operator++() {
  // case 0: done
  if (!current_layer_)
    return *this;

  // case 1: descend.
  if (!current_layer_->children().empty()) {
    current_layer_ = current_layer_->children()[0].get();
    list_indices_.push_back(0);
    return *this;
  }

  for (Layer* parent = current_layer_->mutable_parent(); parent;
       parent = parent->mutable_parent()) {
    // We now try and advance in some list of siblings.
    // case 2: Advance to a sibling.
    if (list_indices_.back() + 1 < parent->children().size()) {
      ++list_indices_.back();
      current_layer_ = parent->children()[list_indices_.back()].get();
      return *this;
    }

    // We need to ascend. We will pop an index off the stack.
    list_indices_.pop_back();
  }

  current_layer_ = nullptr;
  return *this;
}

LayerListIterator::~LayerListIterator() = default;

LayerListConstIterator::LayerListConstIterator(const Layer* root_layer)
    : current_layer_(root_layer) {
  DCHECK(!root_layer || !root_layer->parent());
  list_indices_.push_back(0);
}

LayerListConstIterator::LayerListConstIterator(
    const LayerListConstIterator& other) = default;

LayerListConstIterator& LayerListConstIterator::operator++() {
  // case 0: done
  if (!current_layer_)
    return *this;

  // case 1: descend.
  if (!current_layer_->children().empty()) {
    current_layer_ = current_layer_->children()[0].get();
    list_indices_.push_back(0);
    return *this;
  }

  for (const Layer* parent = current_layer_->parent(); parent;
       parent = parent->parent()) {
    // We now try and advance in some list of siblings.
    // case 2: Advance to a sibling.
    if (list_indices_.back() + 1 < parent->children().size()) {
      ++list_indices_.back();
      current_layer_ = parent->children()[list_indices_.back()].get();
      return *this;
    }

    // We need to ascend. We will pop an index off the stack.
    list_indices_.pop_back();
  }

  current_layer_ = nullptr;
  return *this;
}

LayerListConstIterator::~LayerListConstIterator() = default;

LayerListReverseIterator::LayerListReverseIterator(Layer* root_layer)
    : current_layer_(root_layer) {
  DCHECK(!root_layer || !root_layer->parent());
  list_indices_.push_back(0);
  DescendToRightmostInSubtree();
}

LayerListReverseIterator::LayerListReverseIterator(
    const LayerListReverseIterator& other) = default;

// We will only support prefix increment.
LayerListReverseIterator& LayerListReverseIterator::operator++() {
  // case 0: done
  if (!current_layer_)
    return *this;

  // case 1: we're the leftmost sibling.
  if (!list_indices_.back()) {
    list_indices_.pop_back();
    current_layer_ = current_layer_->mutable_parent();
    return *this;
  }

  // case 2: we're not the leftmost sibling. In this case, we want to move one
  // sibling over, and then descend to the rightmost descendant in that subtree.
  CHECK(current_layer_->parent());
  --list_indices_.back();
  this->current_layer_ =
      current_layer_->mutable_parent()->children()[list_indices_.back()].get();
  DescendToRightmostInSubtree();
  return *this;
}

void LayerListReverseIterator::DescendToRightmostInSubtree() {
  if (!current_layer_)
    return;

  if (current_layer_->children().empty())
    return;

  size_t last_index = current_layer_->children().size() - 1;
  this->current_layer_ = current_layer_->children()[last_index].get();
  list_indices_.push_back(last_index);
  DescendToRightmostInSubtree();
}

LayerListReverseIterator::~LayerListReverseIterator() = default;

LayerListReverseConstIterator::LayerListReverseConstIterator(
    const LayerListReverseConstIterator& other) = default;

LayerListReverseConstIterator::LayerListReverseConstIterator(
    const Layer* root_layer)
    : current_layer_(root_layer) {
  DCHECK(!root_layer || !root_layer->parent());
  list_indices_.push_back(0);
  DescendToRightmostInSubtree();
}

LayerListReverseConstIterator& LayerListReverseConstIterator::operator++() {
  // case 0: done
  if (!current_layer_)
    return *this;

  // case 1: we're the leftmost sibling.
  if (!list_indices_.back()) {
    list_indices_.pop_back();
    current_layer_ = current_layer_->parent();
    return *this;
  }

  // case 2: we're not the leftmost sibling. In this case, we want to move one
  // sibling over, and then descend to the rightmost descendant in that subtree.
  CHECK(current_layer_->parent());
  --list_indices_.back();
  this->current_layer_ =
      current_layer_->parent()->children()[list_indices_.back()].get();
  DescendToRightmostInSubtree();
  return *this;
}

void LayerListReverseConstIterator::DescendToRightmostInSubtree() {
  if (!current_layer_)
    return;

  if (current_layer_->children().empty())
    return;

  size_t last_index = current_layer_->children().size() - 1;
  this->current_layer_ = current_layer_->children()[last_index].get();
  list_indices_.push_back(last_index);
  DescendToRightmostInSubtree();
}

LayerListReverseConstIterator::~LayerListReverseConstIterator() = default;

}  // namespace cc
