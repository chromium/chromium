// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_LAYER_TILING_CLIENT_H_
#define CC_TEST_FAKE_PICTURE_LAYER_TILING_CLIENT_H_

#include "cc/raster/raster_source.h"
#include "cc/test/fake_tile_manager_client.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_manager.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
class ClientResourceProvider;
class ContextProvider;
}

namespace cc {

class FakePictureLayerTilingClient : public PictureLayerTilingClient {
 public:
  FakePictureLayerTilingClient();
  explicit FakePictureLayerTilingClient(
      viz::ClientResourceProvider* resource_provider,
      viz::ContextProvider* context_provider);
  ~FakePictureLayerTilingClient() override;

  // PictureLayerTilingClient implementation.
  std::unique_ptr<Tile> CreateTile(const Tile::CreateInfo& info) override;
  gfx::Size CalculateTileSize(const gfx::Size& content_bounds) override;
  bool HasValidTilePriorities() const override;

  void SetTileSize(const gfx::Size& tile_size);
  gfx::Size TileSize() const { return tile_size_; }

  const Region* GetPendingInvalidation() override;
  const PictureLayerTiling* GetPendingOrActiveTwinTiling(
      const PictureLayerTiling* tiling) const override;
  bool RequiresHighResToDraw() const override;
  const PaintWorkletRecordMap& GetPaintWorkletRecords() const override;

  void set_twin_tiling_set(PictureLayerTilingSet* set) {
    twin_set_ = set;
    twin_tiling_ = nullptr;
  }
  void set_twin_tiling(PictureLayerTiling* tiling) {
    twin_tiling_ = tiling;
    twin_set_ = nullptr;
  }
  void set_text_rect(const gfx::Rect& rect) { text_rect_ = rect; }
  void set_invalidation(const Region& region) { invalidation_ = region; }
  void set_has_valid_tile_priorities(bool has_valid_tile_priorities) {
    has_valid_tile_priorities_ = has_valid_tile_priorities;
  }
  RasterSource* raster_source() { return raster_source_.get(); }

  TileManager* tile_manager() const {
    return tile_manager_.get();
  }

 protected:
  FakeTileManagerClient tile_manager_client_;
  std::unique_ptr<ResourcePool> resource_pool_;
  std::unique_ptr<TileManager> tile_manager_;
  scoped_refptr<RasterSource> raster_source_;
  gfx::Size tile_size_;
  PictureLayerTilingSet* twin_set_;
  PictureLayerTiling* twin_tiling_;
  gfx::Rect text_rect_;
  Region invalidation_;
  bool has_valid_tile_priorities_;
  PaintWorkletRecordMap paint_worklet_records_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_LAYER_TILING_CLIENT_H_
