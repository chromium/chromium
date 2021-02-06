// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_LIST_ITERATOR_H_
#define CC_LAYERS_LAYER_LIST_ITERATOR_H_

#include <stdlib.h>
#include <vector>

#include "cc/cc_export.h"

namespace cc {

class Layer;

// This visits a tree of layers in drawing order.
class CC_EXPORT LayerListIterator {
 public:
  explicit LayerListIterator(Layer* root_layer);
  LayerListIterator(const LayerListIterator& other);
  virtual ~LayerListIterator();

  bool operator==(const LayerListIterator& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListIterator& other) const {
    return !(*this == other);
  }

  // We will only support prefix increment.
  virtual LayerListIterator& operator++();
  Layer* operator->() const { return current_layer_; }
  Layer* operator*() const { return current_layer_; }

 protected:
  // The implementation of this iterator is currently tied tightly to the layer
  // tree, but it should be straightforward to reimplement in terms of a list
  // when it's ready.
  Layer* current_layer_;
  std::vector<size_t> list_indices_;
};

class CC_EXPORT LayerListReverseIterator : public LayerListIterator {
 public:
  explicit LayerListReverseIterator(Layer* root_layer);
  ~LayerListReverseIterator() override;

  // We will only support prefix increment.
  LayerListIterator& operator++() override;

 private:
  void DescendToRightmostInSubtree();
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_LIST_ITERATOR_H_
