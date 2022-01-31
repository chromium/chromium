// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_OCCLUDED_TILE_ITERATOR_H_
#define CC_TILES_OCCLUDED_TILE_ITERATOR_H_

#include <vector>

#include "cc/cc_export.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cc {

class PictureLayerImpl;
class PictureLayerTilingSet;

// Used to iterate over the occluded tiles in a vector of PictureLayerImpls.
// The order of iteration is not defined.
class CC_EXPORT OccludedTileIterator {
 public:
  explicit OccludedTileIterator(
      const std::vector<PictureLayerImpl*>& picture_layers);
  OccludedTileIterator(const OccludedTileIterator&) = delete;
  OccludedTileIterator& operator=(const OccludedTileIterator&) = delete;
  ~OccludedTileIterator();

  bool AtEnd() const;
  void Next();
  Tile* GetCurrent();

 private:
  // The following functions start iterating at the *current* location.
  // Each function returns true if a match is found, false indicates there
  // are no more items to iterate through.
  bool FindNextInPictureLayers();
  bool FindNextInPictureLayerTilingSet();
  bool FindNextInTileIterator();

  PictureLayerTilingSet* CurrentPictureLayerTilingSet();
  PictureLayerTiling* CurrentPictureLayerTiling();

  const std::vector<PictureLayerImpl*>& picture_layers_;
  // Index into `picture_layers_` the current tile comes from.
  size_t current_picture_layer_index_ = 0;
  // Index into the current PictureLayerTilingSet the current tile comes from.
  size_t current_picture_layer_tiling_index_ = 0;
  // Iterates over the tiles from the current PictureLayerTiling. If this is
  // not set, the end has been reached.
  absl::optional<PictureLayerTiling::TileIterator> tile_iterator_;
};

}  // namespace cc

#endif  // CC_TILES_OCCLUDED_TILE_ITERATOR_H_
