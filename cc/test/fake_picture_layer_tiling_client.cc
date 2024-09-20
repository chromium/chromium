// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer_tiling_client.h"

#include <stddef.h>

#include <limits>
#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_tile_manager.h"

namespace cc {

namespace {

void SetupGlobalTileState(GlobalStateThatImpactsTilePriority* state) {
  state->soft_memory_limit_in_bytes = 100 * 1000 * 1000;
  state->num_resources_limit = 10000;
  state->hard_memory_limit_in_bytes = state->soft_memory_limit_in_bytes * 2;
  state->memory_limit_policy = ALLOW_ANYTHING;
  state->tree_priority = SAME_PRIORITY_FOR_BOTH_TREES;
}

}  // namespace

FakePictureLayerTilingClient::FakePictureLayerTilingClient()
    : tile_manager_(new FakeTileManager(&tile_manager_client_)),
      raster_source_(FakeRasterSource::CreateInfiniteFilled()),
      twin_set_(nullptr),
      twin_tiling_(nullptr),
      has_valid_tile_priorities_(true) {
  SetupGlobalTileState(&global_tile_state_);
}

FakePictureLayerTilingClient::FakePictureLayerTilingClient(
    viz::ClientResourceProvider* resource_provider,
    viz::RasterContextProvider* context_provider)
    : resource_pool_(std::make_unique<ResourcePool>(
          resource_provider,
          context_provider,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          ResourcePool::kDefaultExpirationDelay,
          false)),
      tile_manager_(
          new FakeTileManager(&tile_manager_client_, resource_pool_.get())),
      raster_source_(FakeRasterSource::CreateInfiniteFilled()),
      twin_set_(nullptr),
      twin_tiling_(nullptr),
      has_valid_tile_priorities_(true) {
  SetupGlobalTileState(&global_tile_state_);
}

FakePictureLayerTilingClient::~FakePictureLayerTilingClient() = default;

std::unique_ptr<Tile> FakePictureLayerTilingClient::CreateTile(
    const Tile::CreateInfo& info) {
  return tile_manager_->CreateTile(info, 0, 0, 0);
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

std::vector<const DrawImage*>
FakePictureLayerTilingClient::GetDiscardableImagesInRect(
    const gfx::Rect& rect) const {
  return {};
}

ScrollOffsetMap FakePictureLayerTilingClient::GetRasterInducingScrollOffsets()
    const {
  return ScrollOffsetMap();
}

const GlobalStateThatImpactsTilePriority&
FakePictureLayerTilingClient::global_tile_state() const {
  return global_tile_state_;
}

}  // namespace cc
