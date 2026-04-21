// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_COLLECTIONS_H_
#define CC_LAYERS_LAYER_COLLECTIONS_H_

#include <algorithm>
#include <memory>
#include <ranges>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "cc/cc_export.h"

namespace cc {
class Layer;
class LayerImpl;
class PictureLayerImpl;
class RenderSurfaceImpl;

// OwnedLayerImplList handles ownership and all bookkeeping for a set of
// LayerImpls.  It allows fast random access and lookup by layer->id(), as well
// as tracking of layers that need to push properties upon activation.
// Iteration order is vector order.  The only permitted mutations are appending
// and clearing.
class CC_EXPORT OwnedLayerImplList {
 public:
  typedef std::vector<std::unique_ptr<LayerImpl>> VectorType;
  typedef base::flat_map<int, VectorType::iterator::difference_type> MapType;
  typedef base::flat_set<int> SetType;

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

  // Used to iterate over the various tracked subsets of LayerImpls stored as
  // members of OwnedLayerImplList.
  template <typename LayerType, typename ContainerType>
  class Iterator {
   public:
    using difference_type = ContainerType::iterator::difference_type;
    using value_type = LayerType*;

    Iterator() = default;
    Iterator(const OwnedLayerImplList& layer_list,
             ContainerType::const_iterator cur,
             ContainerType::const_iterator end)
        : layer_list_(&layer_list), cur_(cur) {}
    Iterator(const Iterator<LayerType, ContainerType>&) = default;
    Iterator(Iterator<LayerType, ContainerType>&&) = default;
    Iterator& operator=(const Iterator<LayerType, ContainerType>&) = default;
    Iterator& operator=(Iterator<LayerType, ContainerType>&&) = default;
    LayerType* operator*() const;
    LayerType* operator->() const { return this->operator*(); }
    Iterator<LayerType, ContainerType>& operator++() {
      if (cur_ != end_) {
        ++cur_;
      }
      cur_layer_ = nullptr;
      return *this;
    }
    Iterator<LayerType, ContainerType> operator++(int) {
      Iterator<LayerType, ContainerType> result(*this);
      ++*this;
      return result;
    }
    bool operator==(const Iterator<LayerType, ContainerType>& other) const {
      return layer_list_ == other.layer_list_ && cur_ == other.cur_;
    }

   private:
    raw_ptr<const OwnedLayerImplList> layer_list_{nullptr};
    ContainerType::const_iterator cur_;
    ContainerType::const_iterator end_;
    mutable raw_ptr<LayerType> cur_layer_{nullptr};
  };

  // Functions as an iterable std::ranges::range over the various tracked
  // subsets of LayerImpls stored as members of OwnedLayerImplList.
  template <typename LayerType, typename ContainerType>
  class Range {
   public:
    using iterator = Iterator<LayerType, ContainerType>;

    Range() {}
    Range(const OwnedLayerImplList& layer_list, const ContainerType& container)
        : layer_list_(&layer_list), container_(&container) {}
    Range(const Range&) = default;
    Range(Range&&) = default;
    Range& operator=(const Range&) = default;
    Range& operator=(Range&&) = default;
    iterator begin() const {
      return layer_list_ && container_
                 ? iterator(*layer_list_, container_->begin(),
                            container_->end())
                 : iterator();
    }
    iterator end() const {
      return layer_list_ && container_
                 ? iterator(*layer_list_, container_->end(), container_->end())
                 : iterator();
    }
    size_type size() const { return container_ ? container_->size() : 0u; }
    bool empty() const { return size() == 0u; }

   private:
    raw_ptr<const OwnedLayerImplList> layer_list_{nullptr};
    raw_ptr<const ContainerType> container_{nullptr};
  };

  Range<LayerImpl, SetType> LayersThatShouldPushProperties() const;
  void SetShouldPushProperties(LayerImpl* layer);
  void ClearLayersShouldPushProperties();

  Range<PictureLayerImpl, MapType> PictureLayers() const;

  Range<PictureLayerImpl, SetType> PictureLayersWithAnimatedImages() const;
  void SetPictureLayerWithAnimatedImages(PictureLayerImpl* layer);
  void RemovePictureLayerWithAnimatedImages(PictureLayerImpl* layer);

  Range<PictureLayerImpl, SetType> PictureLayersWithWorklets() const;
  void SetPictureLayerWithWorklet(PictureLayerImpl* layer);
  void RemovePictureLayerWithWorklet(PictureLayerImpl* layer);

 private:
  VectorType layers_;
  SetType layers_that_should_push_properties_;
  SetType picture_layers_with_animated_images_;
  SetType picture_layers_with_worklets_;
  uint32_t num_picture_layers_ = 0u;
  mutable MapType layer_map_;
  mutable MapType picture_layer_map_;
  mutable bool layer_maps_need_rebuild_ = false;

  void RebuildLayerMaps() const;
};

using LayerList = std::vector<scoped_refptr<Layer>>;
// RAW_PTR_EXCLUSION: Renderer performance: visible in sampling profiler stacks.
using LayerImplList = RAW_PTR_EXCLUSION std::vector<LayerImpl*>;
using RenderSurfaceList = RAW_PTR_EXCLUSION std::vector<RenderSurfaceImpl*>;
using LayerImplRange =
    OwnedLayerImplList::Range<LayerImpl, OwnedLayerImplList::MapType>;
using PictureLayerImplRange =
    OwnedLayerImplList::Range<PictureLayerImpl, OwnedLayerImplList::MapType>;

}  // namespace cc

#endif  // CC_LAYERS_LAYER_COLLECTIONS_H_
