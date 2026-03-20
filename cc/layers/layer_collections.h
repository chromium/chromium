// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_COLLECTIONS_H_
#define CC_LAYERS_LAYER_COLLECTIONS_H_

#include <algorithm>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "cc/cc_export.h"

namespace cc {
class Layer;
class LayerImpl;
class RenderSurfaceImpl;

using LayerList = std::vector<scoped_refptr<Layer>>;
// RAW_PTR_EXCLUSION: Renderer performance: visible in sampling profiler stacks.
using LayerImplList = RAW_PTR_EXCLUSION std::vector<LayerImpl*>;
using RenderSurfaceList = RAW_PTR_EXCLUSION std::vector<RenderSurfaceImpl*>;

// OwnedLayerImplList handles ownership and all bookkeeping for a set of
// LayerImpls.  It allows fast random access and lookup by layer->id(), as well
// as tracking of layers that need to push properties upon activation.
// Iteration order is vector order.  The only permitted mutations are appending
// and clearing.
class CC_EXPORT OwnedLayerImplList {
 public:
  typedef std::vector<std::unique_ptr<LayerImpl>> VectorType;
  typedef base::flat_map<int, VectorType::iterator::difference_type> MapType;
  typedef std::unordered_set<int> SetType;

  using size_type = VectorType::size_type;
  using difference_type = VectorType::difference_type;
  using reference = VectorType::reference;
  using const_reference = VectorType::const_reference;
  using pointer = VectorType::pointer;
  using const_pointer = VectorType::const_pointer;
  using iterator = VectorType::iterator;
  using const_iterator = VectorType::const_iterator;
  using reverse_iterator = VectorType::reverse_iterator;
  using const_reverse_iterator = VectorType::const_reverse_iterator;

  // Move is OK, copy is not.
  OwnedLayerImplList();
  OwnedLayerImplList(OwnedLayerImplList&& other);
  OwnedLayerImplList(const OwnedLayerImplList&) = delete;
  OwnedLayerImplList& operator=(OwnedLayerImplList&&);
  const OwnedLayerImplList& operator=(const OwnedLayerImplList&) = delete;
  ~OwnedLayerImplList();

  bool empty() const { return layers_.empty(); }
  size_type size() const { return layers_.size(); }
  void reserve(size_type count);

  // Modification
  void clear();
  void push_back(std::unique_ptr<LayerImpl>&& value);

  // Vector-style iteration
  iterator begin() { return layers_.begin(); }
  const_iterator begin() const { return layers_.begin(); }
  const_iterator cbegin() const { return layers_.cbegin(); }
  iterator end() { return layers_.end(); }
  const_iterator end() const { return layers_.end(); }
  const_iterator cend() const { return layers_.cend(); }
  reverse_iterator rbegin() { return layers_.rbegin(); }
  const_reverse_iterator rbegin() const { return layers_.rbegin(); }
  const_reverse_iterator crbegin() const { return layers_.crbegin(); }
  reverse_iterator rend() { return layers_.rend(); }
  const_reverse_iterator rend() const { return layers_.rend(); }
  const_reverse_iterator crend() const { return layers_.crend(); }

  // Vector-style random access
  reference at(size_type pos) { return layers_.at(pos); }
  const_reference at(size_type pos) const { return layers_.at(pos); }
  reference operator[](size_type pos) { return at(pos); }
  const_reference operator[](size_type pos) const { return at(pos); }
  reference front() { return layers_.front(); }
  const_reference front() const { return layers_.front(); }
  reference back() { return layers_.back(); }
  const_reference back() const { return layers_.back(); }

  // Map-style lookup
  iterator find(int id);
  const_iterator find(int id) const;
  bool contains(int id) const;

  // Property invalidation
  void SetShouldPushProperties(int id) {
    layers_that_should_push_properties_.insert(id);
  }
  void ClearLayersShouldPushProperties() {
    layers_that_should_push_properties_.clear();
  }

  // Tracking of layers that need to push properties.
  class DirtyLayerIterator {
   public:
    using difference_type = SetType::iterator::difference_type;
    using value_type = LayerImpl*;

    DirtyLayerIterator() = default;
    DirtyLayerIterator(const OwnedLayerImplList& layer_list,
                       const SetType::iterator& cur)
        : layer_list_(&layer_list), cur_(cur) {}
    DirtyLayerIterator(const DirtyLayerIterator&) = default;
    DirtyLayerIterator(DirtyLayerIterator&&) = default;
    DirtyLayerIterator& operator=(const DirtyLayerIterator&) = default;
    DirtyLayerIterator& operator=(DirtyLayerIterator&&) = default;
    LayerImpl* operator*() const {
      if (!cur_layer_) {
        cur_layer_ = layer_list_->find(*cur_)->get();
      }
      return cur_layer_;
    }
    LayerImpl* operator->() const { return this->operator*(); }
    DirtyLayerIterator& operator++() {
      ++cur_;
      cur_layer_ = nullptr;
      return *this;
    }
    DirtyLayerIterator operator++(int) {
      DirtyLayerIterator result(*this);
      ++*this;
      return result;
    }
    bool operator==(const DirtyLayerIterator& other) const {
      return layer_list_ == other.layer_list_ && cur_ == other.cur_;
    }

   private:
    raw_ptr<const OwnedLayerImplList> layer_list_{nullptr};
    SetType::const_iterator cur_;
    mutable raw_ptr<LayerImpl> cur_layer_{nullptr};
  };

  class DirtyLayerRange {
   public:
    explicit DirtyLayerRange(const OwnedLayerImplList& layer_list)
        : layer_list_(layer_list) {}
    DirtyLayerRange(const DirtyLayerRange&) = default;
    DirtyLayerRange(DirtyLayerRange&&) = default;
    DirtyLayerRange& operator=(const DirtyLayerRange&) = default;
    DirtyLayerRange& operator=(DirtyLayerRange&&) = default;
    DirtyLayerIterator begin() const {
      return {*layer_list_,
              layer_list_->layers_that_should_push_properties_.begin()};
    }
    DirtyLayerIterator end() const {
      return {*layer_list_,
              layer_list_->layers_that_should_push_properties_.end()};
    }
    size_type size() const {
      return layer_list_->layers_that_should_push_properties_.size();
    }

   private:
    raw_ref<const OwnedLayerImplList> layer_list_;
  };
  DirtyLayerRange LayersThatShouldPushProperties() const {
    return DirtyLayerRange(*this);
  }

 private:
  VectorType layers_;
  SetType layers_that_should_push_properties_;
  mutable MapType layer_map_;
  mutable bool layer_map_needs_rebuild_ = false;

  void RebuildLayerMap() const;
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_COLLECTIONS_H_
