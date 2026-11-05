// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_collections.h"

#include <utility>

#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer_impl.h"

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
  layer_maps_need_rebuild_ = false;
  picture_layer_map_.clear();
  picture_layers_with_animated_images_.clear();
  picture_layers_with_worklets_.clear();
  num_picture_layers_ = 0u;
}

void OwnedLayerImplList::push_back(std::unique_ptr<LayerImpl>&& value) {
  LayerImpl* layer = value.get();
  int id = layer->id();
  layers_.push_back(std::move(value));
  layer_maps_need_rebuild_ = true;
  if (layer->GetChangeFlag(LayerImpl::kChangedAllProperties)) {
    layers_that_should_push_properties_.insert(id);
  }
  if (layer->GetLayerType() == mojom::LayerType::kPicture) {
    num_picture_layers_++;
    if (static_cast<PictureLayerImpl*>(layer)
            ->GetPaintWorkletRecordMap()
            .size()) {
      picture_layers_with_worklets_.insert(id);
    }
    if (static_cast<PictureLayerImpl*>(layer)->HasAnimatedImages()) {
      picture_layers_with_animated_images_.insert(id);
    }
  }
}

void OwnedLayerImplList::SetShouldPushProperties(LayerImpl* layer) {
  layers_that_should_push_properties_.insert(layer->id());
}

void OwnedLayerImplList::ClearLayersShouldPushProperties() {
  layers_that_should_push_properties_.clear();
}

OwnedLayerImplList::Range<LayerImpl, OwnedLayerImplList::SetType>
OwnedLayerImplList::LayersThatShouldPushProperties() const {
  return {*this, layers_that_should_push_properties_};
}

OwnedLayerImplList::Range<PictureLayerImpl, OwnedLayerImplList::MapType>
OwnedLayerImplList::PictureLayers() const {
  if (layer_maps_need_rebuild_) {
    RebuildLayerMaps();
  }
  return {*this, picture_layer_map_};
}

void OwnedLayerImplList::SetPictureLayerWithAnimatedImages(
    PictureLayerImpl* layer) {
  DCHECK(contains(layer->id()));
  picture_layers_with_animated_images_.insert(layer->id());
}

void OwnedLayerImplList::RemovePictureLayerWithAnimatedImages(
    PictureLayerImpl* layer) {
  picture_layers_with_animated_images_.erase(layer->id());
}

OwnedLayerImplList::Range<PictureLayerImpl, OwnedLayerImplList::SetType>
OwnedLayerImplList::PictureLayersWithAnimatedImages() const {
  return {*this, picture_layers_with_animated_images_};
}

void OwnedLayerImplList::SetPictureLayerWithWorklet(PictureLayerImpl* layer) {
  DCHECK(contains(layer->id()));
  picture_layers_with_worklets_.insert(layer->id());
}

void OwnedLayerImplList::RemovePictureLayerWithWorklet(
    PictureLayerImpl* layer) {
  picture_layers_with_worklets_.erase(layer->id());
}

OwnedLayerImplList::Range<PictureLayerImpl, OwnedLayerImplList::SetType>
OwnedLayerImplList::PictureLayersWithWorklets() const {
  return {*this, picture_layers_with_worklets_};
}

OwnedLayerImplList::const_iterator OwnedLayerImplList::find(int id) const {
  if (layer_maps_need_rebuild_) {
    RebuildLayerMaps();
  }
  const auto iter = layer_map_.find(id);
  return iter == layer_map_.end() ? end() : begin() + iter->second;
}

bool OwnedLayerImplList::contains(int id) const {
  if (layer_maps_need_rebuild_) {
    RebuildLayerMaps();
  }
  return find(id) != end();
}

void OwnedLayerImplList::RebuildLayerMaps() const {
  std::vector<std::pair<int, difference_type>> layer_indices_vector;
  layer_indices_vector.reserve(size());
  std::vector<std::pair<int, difference_type>> picture_layer_indices_vector;
  picture_layer_indices_vector.reserve(num_picture_layers_);
  for (difference_type i = 0; i < static_cast<difference_type>(size()); ++i) {
    LayerImpl* layer = layers_[i].get();
    int layer_id = layer->id();
    layer_indices_vector.emplace_back(layer_id, i);
    if (layer->GetLayerType() == mojom::LayerType::kPicture) {
      picture_layer_indices_vector.emplace_back(layer_id, i);
    }
  }
  // Layer ids should be unique here, so doing a std::sort and
  // base::sorted_unique initializer is the most efficient, avoiding
  // a std::stable_sort inside base::flat_map.
  std::sort(layer_indices_vector.begin(), layer_indices_vector.end());
  layer_map_ = MapType(base::sorted_unique, std::move(layer_indices_vector));
  std::sort(picture_layer_indices_vector.begin(),
            picture_layer_indices_vector.end());
  picture_layer_map_ =
      MapType(base::sorted_unique, std::move(picture_layer_indices_vector));
  layer_maps_need_rebuild_ = false;
}

std::unique_ptr<LayerImpl>
OwnedLayerImplList::ReleaseLayerForTesting(  // IN-TEST
    int layer_id) {
  return ReleaseLayer(layer_id);
}

std::unique_ptr<LayerImpl> OwnedLayerImplList::ReleaseLayer(int layer_id) {
  if (layer_maps_need_rebuild_) {
    RebuildLayerMaps();
  }
  auto iter = layer_map_.find(layer_id);
  if (iter == layer_map_.end()) {
    return nullptr;
  }
  CHECK_GE(iter->second, 0);
  return std::move(layers_[iter->second]);
}

template <>
CC_EXPORT LayerImpl*
OwnedLayerImplList::Iterator<LayerImpl,
                             OwnedLayerImplList::SetType>::operator*() const {
  if (!cur_layer_) {
    cur_layer_ = layer_list_->find(*cur_)->get();
  }
  return cur_layer_;
}

template <>
CC_EXPORT PictureLayerImpl*
OwnedLayerImplList::Iterator<PictureLayerImpl,
                             OwnedLayerImplList::SetType>::operator*() const {
  if (!cur_layer_) {
    cur_layer_ =
        static_cast<PictureLayerImpl*>(layer_list_->find(*cur_)->get());
  }
  return cur_layer_;
}

template <>
CC_EXPORT LayerImpl*
OwnedLayerImplList::Iterator<LayerImpl,
                             OwnedLayerImplList::MapType>::operator*() const {
  if (!cur_layer_) {
    cur_layer_ = layer_list_->at(cur_->second).get();
  }
  return cur_layer_;
}

template <>
CC_EXPORT PictureLayerImpl*
OwnedLayerImplList::Iterator<PictureLayerImpl,
                             OwnedLayerImplList::MapType>::operator*() const {
  if (!cur_layer_) {
    cur_layer_ =
        static_cast<PictureLayerImpl*>(layer_list_->at(cur_->second).get());
  }
  return cur_layer_;
}

}  // namespace cc
