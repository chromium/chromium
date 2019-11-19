// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_TILE_MANAGER_CLIENT_H_
#define CC_TEST_FAKE_TILE_MANAGER_CLIENT_H_

#include <vector>

#include "cc/tiles/tile_manager.h"

namespace cc {

class FakeTileManagerClient : public TileManagerClient {
 public:
  FakeTileManagerClient();
  ~FakeTileManagerClient() override;

  // TileManagerClient implementation.
  void NotifyReadyToActivate() override {}
  void NotifyReadyToDraw() override {}
  void NotifyAllTileTasksCompleted() override {}
  void NotifyTileStateChanged(const Tile* tile) override {}
  std::unique_ptr<RasterTilePriorityQueue> BuildRasterQueue(
      TreePriority tree_priority,
      RasterTilePriorityQueue::Type type) override;
  std::unique_ptr<EvictionTilePriorityQueue> BuildEvictionQueue(
      TreePriority tree_priority) override;
  void SetIsLikelyToRequireADraw(bool is_likely_to_require_a_draw) override {}
  const gfx::ColorSpace& GetRasterColorSpace() const override;
  void RequestImplSideInvalidationForCheckerImagedTiles() override {}
  size_t GetFrameIndexForImage(const PaintImage& paint_image,
                               WhichTree tree) const override;
  int GetMSAASampleCountForRaster(
      const scoped_refptr<DisplayItemList>& display_list) override;

 private:
  gfx::ColorSpace color_space_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_TILE_MANAGER_CLIENT_H_
