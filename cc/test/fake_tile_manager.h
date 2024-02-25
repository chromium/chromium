// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_TILE_MANAGER_H_
#define CC_TEST_FAKE_TILE_MANAGER_H_

#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "cc/tiles/tile_manager.h"

namespace cc {

class FakeTileManager : public TileManager {
 public:
  FakeTileManager(TileManagerClient* client,
                  ResourcePool* resource_pool = nullptr);
  ~FakeTileManager() override;

  bool HasBeenAssignedMemory(Tile* tile);
  void AssignMemoryToTiles(
      const GlobalStateThatImpactsTilePriority& state);

  std::vector<raw_ptr<Tile, VectorExperimental>> tiles_for_raster;

 private:
  SoftwareImageDecodeCache image_decode_cache_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_TILE_MANAGER_H_
