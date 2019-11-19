// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer_tiling_client.h"

#include <stddef.h>

#include <limits>

#include "base/threading/thread_task_runner_handle.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_tile_manager.h"

namespace cc {

FakePictureLayerTilingClient::FakePictureLayerTilingClient()
    : tile_manager_(new FakeTileManager(&tile_manager_client_)),
      raster_source_(FakeRasterSource::CreateInfiniteFilled()),
      twin_set_(nullptr),
      twin_tiling_(nullptr),
      has_valid_tile_priorities_(true) {}

FakePictureLayerTilingClient::FakePictureLayerTilingClient(
    viz::ClientResourceProvider* resource_provider,
    viz::ContextProvider* context_provider)
    : resource_pool_(
          std::make_unique<ResourcePool>(resource_provider,
                                         context_provider,
                                         base::ThreadTaskRunnerHandle::Get(),
                                         ResourcePool::kDefaultExpirationDelay,
                                         false)),
      tile_manager_(
          new FakeTileManager(&tile_manager_client_, resource_pool_.get())),
      raster_source_(FakeRasterSource::CreateInfiniteFilled()),
      twin_set_(nullptr),
      twin_tiling_(nullptr),
      has_valid_tile_priorities_(true) {}

FakePictureLayerTilingClient::~FakePictureLayerTilingClient() = default;

std::unique_ptr<Tile> FakePictureLayerTilingClient::CreateTile(
    const Tile::CreateInfo& info) {
  return tile_manager_->CreateTile(info, 0, 0, 0, false);
}

void FakePictureLayerTilingClient::SetTileSize(const gfx::Size& tile_size) {
  tile_size_ = tile_size;
}

gfx::Size FakePictureLayerTilingClient::CalculateTileSize(
    const gfx::Size& /* content_bounds */) {
  return tile_size_;
}

bool FakePictureLayerTilingClient::HasValidTilePriorities() const {
  return has_valid_tile_priorities_;
}

const Region* FakePictureLayerTilingClient::GetPendingInvalidation() {
  return &invalidation_;
}

const PictureLayerTiling*
FakePictureLayerTilingClient::GetPendingOrActiveTwinTiling(
    const PictureLayerTiling* tiling) const {
  if (!twin_set_)
    return twin_tiling_;
  for (size_t i = 0; i < twin_set_->num_tilings(); ++i) {
    if (twin_set_->tiling_at(i)->raster_transform() ==
        tiling->raster_transform())
      return twin_set_->tiling_at(i);
  }
  return nullptr;
}

bool FakePictureLayerTilingClient::RequiresHighResToDraw() const {
  return false;
}

const PaintWorkletRecordMap&
FakePictureLayerTilingClient::GetPaintWorkletRecords() const {
  return paint_worklet_records_;
}

}  // namespace cc
