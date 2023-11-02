// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_LIST_ITERATOR_H_
#define CC_LAYERS_LAYER_LIST_ITERATOR_H_

#include <stdlib.h>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "cc/cc_export.h"

namespace cc {

class Layer;

// This visits a tree of layers in drawing order.
class CC_EXPORT LayerListIterator {
 public:
  explicit LayerListIterator(Layer* root_layer);
  LayerListIterator(const LayerListIterator& other);
  ~LayerListIterator();

  bool operator==(const LayerListIterator& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListIterator& other) const {
    return !(*this == other);
  }

  // We will only support prefix increment.
  LayerListIterator& operator++();
  Layer* operator->() const { return current_layer_; }
  Layer* operator*() const { return current_layer_; }

 private:
  // The implementation of this iterator is currently tied tightly to the layer
  // tree, but it should be straightforward to reimplement in terms of a list
  // when it's ready.

  // `current_layer` is not a raw_ptr<...> for performance reasons (based on
  // analysis of sampling profiler data and tab_search:top100:2020).
  RAW_PTR_EXCLUSION Layer* current_layer_;

  std::vector<size_t> list_indices_;
};

class CC_EXPORT LayerListConstIterator {
 public:
  explicit LayerListConstIterator(const Layer* root_layer);
  LayerListConstIterator(const LayerListConstIterator& other);
  ~LayerListConstIterator();

  bool operator==(const LayerListConstIterator& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListConstIterator& other) const {
    return !(*this == other);
  }

  // We will only support prefix increment.
  LayerListConstIterator& operator++();
  const Layer* operator->() const { return current_layer_; }
  const Layer* operator*() const { return current_layer_; }

 private:
  raw_ptr<const Layer> current_layer_;
  std::vector<size_t> list_indices_;
};

class CC_EXPORT LayerListReverseIterator {
 public:
  explicit LayerListReverseIterator(Layer* root_layer);
  LayerListReverseIterator(const LayerListReverseIterator& other);
  ~LayerListReverseIterator();

  bool operator==(const LayerListReverseIterator& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListReverseIterator& other) const {
    return !(*this == other);
  }

  // We will only support prefix increment.
  LayerListReverseIterator& operator++();
  Layer* operator->() const { return current_layer_; }
  Layer* operator*() const { return current_layer_; }

 private:
  void DescendToRightmostInSubtree();

  raw_ptr<Layer> current_layer_;
  std::vector<size_t> list_indices_;
};

class CC_EXPORT LayerListReverseConstIterator {
 public:
  explicit LayerListReverseConstIterator(const Layer* root_layer);
  LayerListReverseConstIterator(const LayerListReverseConstIterator& other);
  ~LayerListReverseConstIterator();

  bool operator==(const LayerListReverseConstIterator& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListReverseConstIterator& other) const {
    return !(*this == other);
  }

  // We will only support prefix increment.
  LayerListReverseConstIterator& operator++();
  const Layer* operator->() const { return current_layer_; }
  const Layer* operator*() const { return current_layer_; }

 private:
  void DescendToRightmostInSubtree();

  raw_ptr<const Layer> current_layer_;
  std::vector<size_t> list_indices_;
};
}  // namespace cc

#endif  // CC_LAYERS_LAYER_LIST_ITERATOR_H_
