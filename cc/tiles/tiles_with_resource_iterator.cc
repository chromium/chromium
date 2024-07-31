// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tiles_with_resource_iterator.h"

#include "base/memory/raw_ptr.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/tiles/picture_layer_tiling_set.h"

namespace cc {

TilesWithResourceIterator::TilesWithResourceIterator(
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>*
        picture_layers,
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>*
        secondary_picture_layers)
    : picture_layers_(picture_layers),
      secondary_picture_layers_(secondary_picture_layers),
      active_layers_(picture_layers) {
  FindNextInPictureLayers();
}

TilesWithResourceIterator::~TilesWithResourceIterator() = default;

bool TilesWithResourceIterator::AtEnd() const {
  return !tile_iterator_.has_value();
}

Tile* TilesWithResourceIterator::GetCurrent() {
  return AtEnd() ? nullptr : tile_iterator_->GetCurrent();
}

bool TilesWithResourceIterator::IsCurrentTileOccluded() {
  Tile* tile = GetCurrent();
  return tile && tile->tiling()->IsTileOccluded(tile);
}

void TilesWithResourceIterator::Next() {
  if (AtEnd())
    return;
  DCHECK(tile_iterator_);
  tile_iterator_->Next();
  if (FindNextInTileIterator())
    return;
  ++current_picture_layer_tiling_index_;
  if (FindNextInPictureLayerTilingSet())
    return;
  ++current_picture_layer_index_;
  if (FindNextInPictureLayers())
    return;
  // At the end.
  DCHECK(AtEnd());
}

bool TilesWithResourceIterator::FindNextInPictureLayers() {
  if (FindNextInActiveLayers())
    return true;
  DCHECK(AtEnd());
  if (is_active_layers_secondary_layers_)
    return false;
  // Finished iterating through primary picture layers. Start iterating
  // through secondary layers.
  is_active_layers_secondary_layers_ = true;
  active_layers_ = secondary_picture_layers_;
  if (!active_layers_)
    return false;
  current_picture_layer_index_ = 0;
  return FindNextInActiveLayers();
}

bool TilesWithResourceIterator::FindNextInActiveLayers() {
  for (; current_picture_layer_index_ < active_layers_->size();
       ++current_picture_layer_index_) {
    current_picture_layer_tiling_index_ = 0u;
    if (FindNextInPictureLayerTilingSet())
      return true;
  }
  // No more tiles to look at. Reset `tile_iterator_` so that AtEnd() returns
  // true.
  tile_iterator_.reset();
  return false;
}

bool TilesWithResourceIterator::FindNextInPictureLayerTilingSet() {
  PictureLayerTilingSet* tiling_set = CurrentPictureLayerTilingSet();
  for (; current_picture_layer_tiling_index_ < tiling_set->num_tilings();
       ++current_picture_layer_tiling_index_) {
    tile_iterator_.emplace(CurrentPictureLayerTiling());
    if (FindNextInTileIterator())
      return true;
    ++current_picture_layer_tiling_index_;
  }
  return false;
}

bool TilesWithResourceIterator::FindNextInTileIterator() {
  for (; !tile_iterator_->AtEnd(); tile_iterator_->Next()) {
    Tile* tile = tile_iterator_->GetCurrent();
    if (visited_.insert(tile).second && tile->draw_info().has_resource())
      return true;
  }
  return false;
}

PictureLayerTilingSet*
TilesWithResourceIterator::CurrentPictureLayerTilingSet() {
  return (*active_layers_)[current_picture_layer_index_]
      ->picture_layer_tiling_set();
}

PictureLayerTiling* TilesWithResourceIterator::CurrentPictureLayerTiling() {
  return CurrentPictureLayerTilingSet()->tiling_at(
      current_picture_layer_tiling_index_);
}

}  // namespace cc
