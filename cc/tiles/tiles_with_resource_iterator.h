// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILES_WITH_RESOURCE_ITERATOR_H_
#define CC_TILES_TILES_WITH_RESOURCE_ITERATOR_H_

#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/prioritized_tile.h"

namespace cc {

class PictureLayerImpl;
class PictureLayerTilingSet;

// Iterates over all tiles that have a resource. The order of iteration is not
// defined.
class CC_EXPORT TilesWithResourceIterator {
 public:
  TilesWithResourceIterator(
      const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>*
          picture_layers,
      const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>*
          secondary_picture_layers);
  TilesWithResourceIterator(const TilesWithResourceIterator&) = delete;
  TilesWithResourceIterator& operator=(const TilesWithResourceIterator&) =
      delete;
  ~TilesWithResourceIterator();

  bool AtEnd() const;
  void Next();
  Tile* GetCurrent();

  // Returns true if the current tile is occluded, false if at the end.
  bool IsCurrentTileOccluded();

 private:
  // The following functions start iterating at the *current* location.
  // Each function returns true if a match is found, false indicates there
  // are no more items to iterate through.
  bool FindNextInPictureLayers();
  bool FindNextInActiveLayers();
  bool FindNextInPictureLayerTilingSet();
  bool FindNextInTileIterator();

  PictureLayerTilingSet* CurrentPictureLayerTilingSet();
  PictureLayerTiling* CurrentPictureLayerTiling();

  // Iteration occurs over this vector first.
  const raw_ptr<
      const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>>
      picture_layers_;

  // The secondary set of layers to iterate through, may be null.
  const raw_ptr<
      const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>>
      secondary_picture_layers_;

  // Indicates whether `active_layers_` is referencing `picture_layers_` or
  // `secondary_picture_layers_`.
  bool is_active_layers_secondary_layers_ = false;

  raw_ptr<const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>>
      active_layers_;

  // Index into `active_layers_` the current tile comes from.
  size_t current_picture_layer_index_ = 0;

  // Index into the current PictureLayerTilingSet the current tile comes from.
  size_t current_picture_layer_tiling_index_ = 0;

  // Iterates over the tiles from the current PictureLayerTiling. If this is
  // not set, the end has been reached.
  std::optional<PictureLayerTiling::TileIterator> tile_iterator_;

  // Set of tiles that have been visited. Used to ensure the same tile isn't
  // visited more than once.
  std::set<raw_ptr<Tile, SetExperimental>> visited_;
};

}  // namespace cc

#endif  // CC_TILES_TILES_WITH_RESOURCE_ITERATOR_H_
