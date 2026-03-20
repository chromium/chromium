// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_collections.h"

#include <utility>

#include "cc/layers/layer_impl.h"

namespace cc {

OwnedLayerImplList::OwnedLayerImplList() {}
OwnedLayerImplList::OwnedLayerImplList(OwnedLayerImplList&&) = default;
OwnedLayerImplList& OwnedLayerImplList::operator=(OwnedLayerImplList&&) =
    default;

// NOTE: There is some subtle logic to this destructor!  When layers_ is
// destructed, it will synchronously call LayerImpl destructors, some of which
// have complex logic that may call back into this OwnedLayerImplList (e.g. via
// LayerTreeImpl::LayerById). To prevent unsafe re-entry, we call clear() to
// ensure that none of the layers can be accessed during destruction.
OwnedLayerImplList::~OwnedLayerImplList() {
  clear();
}

void OwnedLayerImplList::reserve(size_type count) {
  layers_.reserve(count);
}

void OwnedLayerImplList::clear() {
  // Clear the id->layer map and make the layers unreachable via LayerById()
  // to prevent re-entry during layer destruction.
  VectorType detached_layers = std::move(layers_);
  layers_that_should_push_properties_.clear();
  layer_map_.clear();
  layer_map_needs_rebuild_ = false;
}

void OwnedLayerImplList::push_back(std::unique_ptr<LayerImpl>&& value) {
  layer_map_needs_rebuild_ = true;
  layers_.push_back(std::move(value));
  if (back()->GetChangeFlag(LayerImpl::kChangedAllProperties)) {
    SetShouldPushProperties(back()->id());
  }
}

OwnedLayerImplList::iterator OwnedLayerImplList::find(int id) {
  if (layer_map_needs_rebuild_) {
    RebuildLayerMap();
  }
  auto iter = layer_map_.find(id);
  return iter == layer_map_.end() ? end() : begin() + iter->second;
}

OwnedLayerImplList::const_iterator OwnedLayerImplList::find(int id) const {
  if (layer_map_needs_rebuild_) {
    RebuildLayerMap();
  }
  const auto iter = layer_map_.find(id);
  return iter == layer_map_.end() ? end() : begin() + iter->second;
}

bool OwnedLayerImplList::contains(int id) const {
  if (layer_map_needs_rebuild_) {
    RebuildLayerMap();
  }
  return find(id) != end();
}

void OwnedLayerImplList::RebuildLayerMap() const {
  std::vector<std::pair<int, difference_type>> layer_indices_vector;
  layer_indices_vector.reserve(size());
  for (difference_type i = 0; i < static_cast<difference_type>(size()); ++i) {
    layer_indices_vector.emplace_back(layers_[i]->id(), i);
  }
  // Layer ids should be unique here, so doing a std::sort and
  // base::sorted_unique initializer is the most efficient, avoiding
  // a std::stable_sort inside base::flat_map.
  std::sort(layer_indices_vector.begin(), layer_indices_vector.end());
  layer_map_ = MapType(base::sorted_unique, std::move(layer_indices_vector));
  layer_map_needs_rebuild_ = false;
}

}  // namespace cc
