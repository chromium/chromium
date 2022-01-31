// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/occluded_tile_iterator.h"

#include "cc/layers/picture_layer_impl.h"
#include "cc/tiles/picture_layer_tiling_set.h"

namespace cc {

OccludedTileIterator::OccludedTileIterator(
    const std::vector<PictureLayerImpl*>& picture_layers)
    : picture_layers_(picture_layers) {
  FindNextInPictureLayers();
}

OccludedTileIterator::~OccludedTileIterator() = default;

bool OccludedTileIterator::AtEnd() const {
  return !tile_iterator_.has_value();
}

Tile* OccludedTileIterator::GetCurrent() {
  return AtEnd() ? nullptr : tile_iterator_->GetCurrent();
}

void OccludedTileIterator::Next() {
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

bool OccludedTileIterator::FindNextInPictureLayers() {
  for (; current_picture_layer_index_ < picture_layers_.size();
       ++current_picture_layer_index_) {
    current_picture_layer_tiling_index_ = 0u;
    if (FindNextInPictureLayerTilingSet())
      return true;
  }
  // No more tiles to look at. Reset `tile_iterator_` so that AtEnd() returns
  // true.
  tile_iterator_.reset();
  DCHECK(AtEnd());
  return false;
}

bool OccludedTileIterator::FindNextInPictureLayerTilingSet() {
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

bool OccludedTileIterator::FindNextInTileIterator() {
  PictureLayerTiling* tiling = CurrentPictureLayerTiling();
  for (; !tile_iterator_->AtEnd(); tile_iterator_->Next()) {
    if (tiling->IsTileOccluded(tile_iterator_->GetCurrent()))
      return true;
  }
  return false;
}

PictureLayerTilingSet* OccludedTileIterator::CurrentPictureLayerTilingSet() {
  return picture_layers_[current_picture_layer_index_]
      ->picture_layer_tiling_set();
}

PictureLayerTiling* OccludedTileIterator::CurrentPictureLayerTiling() {
  return CurrentPictureLayerTilingSet()->tiling_at(
      current_picture_layer_tiling_index_);
}

}  // namespace cc
