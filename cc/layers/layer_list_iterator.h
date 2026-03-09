// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_LIST_ITERATOR_H_
#define CC_LAYERS_LAYER_LIST_ITERATOR_H_

#include <stdlib.h>

#include <cstddef>
#include <vector>

#include "base/memory/stack_allocated.h"
#include "cc/cc_export.h"

namespace cc {

class Layer;

// This visits a tree of layers in drawing order.
class CC_EXPORT LayerListIterator {
  STACK_ALLOCATED();

 public:
  using difference_type = std::ptrdiff_t;

  LayerListIterator();
  explicit LayerListIterator(Layer* root_layer);
  LayerListIterator(const LayerListIterator& other);
  ~LayerListIterator();

  bool operator==(const LayerListIterator& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListIterator& other) const {
    return !(*this == other);
  }

  LayerListIterator& operator++();
  LayerListIterator operator++(int) {
    LayerListIterator that(*this);
    ++*this;
    return that;
  }
  Layer* operator->() const { return current_layer_; }
  Layer* operator*() const { return current_layer_; }

 private:
  // The implementation of this iterator is currently tied tightly to the layer
  // tree, but it should be straightforward to reimplement in terms of a list
  // when it's ready.
  Layer* current_layer_ = nullptr;

  std::vector<size_t> list_indices_;
};

class CC_EXPORT LayerListConstIterator {
  STACK_ALLOCATED();

 public:
  using difference_type = std::ptrdiff_t;

  LayerListConstIterator();
  explicit LayerListConstIterator(const Layer* root_layer);
  LayerListConstIterator(const LayerListConstIterator& other);
  ~LayerListConstIterator();

  bool operator==(const LayerListConstIterator& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListConstIterator& other) const {
    return !(*this == other);
  }

  LayerListConstIterator& operator++();
  LayerListConstIterator operator++(int) {
    LayerListConstIterator that(*this);
    ++*this;
    return that;
  }
  const Layer* operator->() const { return current_layer_; }
  const Layer* operator*() const { return current_layer_; }

 private:
  const Layer* current_layer_ = nullptr;
  std::vector<size_t> list_indices_;
};

class CC_EXPORT LayerListReverseIterator {
  STACK_ALLOCATED();

 public:
  using difference_type = std::ptrdiff_t;

  LayerListReverseIterator();
  explicit LayerListReverseIterator(Layer* root_layer);
  LayerListReverseIterator(const LayerListReverseIterator& other);
  ~LayerListReverseIterator();

  bool operator==(const LayerListReverseIterator& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListReverseIterator& other) const {
    return !(*this == other);
  }

  LayerListReverseIterator& operator++();
  LayerListReverseIterator operator++(int) {
    LayerListReverseIterator that(*this);
    ++*this;
    return that;
  }
  Layer* operator->() const { return current_layer_; }
  Layer* operator*() const { return current_layer_; }

 private:
  void DescendToRightmostInSubtree();

  Layer* current_layer_ = nullptr;
  std::vector<size_t> list_indices_;
};

class CC_EXPORT LayerListReverseConstIterator {
  STACK_ALLOCATED();

 public:
  using difference_type = std::ptrdiff_t;

  LayerListReverseConstIterator();
  explicit LayerListReverseConstIterator(const Layer* root_layer);
  LayerListReverseConstIterator(const LayerListReverseConstIterator& other);
  ~LayerListReverseConstIterator();

  bool operator==(const LayerListReverseConstIterator& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListReverseConstIterator& other) const {
    return !(*this == other);
  }

  LayerListReverseConstIterator& operator++();
  LayerListReverseConstIterator operator++(int) {
    LayerListReverseConstIterator that(*this);
    ++*this;
    return that;
  }
  const Layer* operator->() const { return current_layer_; }
  const Layer* operator*() const { return current_layer_; }

 private:
  void DescendToRightmostInSubtree();

  const Layer* current_layer_ = nullptr;
  std::vector<size_t> list_indices_;
};
}  // namespace cc

#endif  // CC_LAYERS_LAYER_LIST_ITERATOR_H_
