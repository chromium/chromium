// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/raster_tile_priority_queue_required.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "cc/base/features.h"
#include "cc/tiles/tiling_set_raster_queue_required.h"

namespace cc {

namespace {

void AppendTilingSetRequiredQueues(
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>& layers,
    std::vector<std::unique_ptr<TilingSetRasterQueueRequired>>* queues) {
  const bool cc_slimming_enabled = features::IsCCSlimmingEnabled();
  for (PictureLayerImpl* layer : layers) {
    if (!layer->HasValidTilePriorities())
      continue;

    PictureLayerTilingSet* tiling_set = layer->picture_layer_tiling_set();
    if (cc_slimming_enabled && tiling_set->all_tiles_done()) {
      continue;
    }
    std::unique_ptr<TilingSetRasterQueueRequired> tiling_set_queue =
        TilingSetRasterQueueRequired::Create(
            tiling_set,
            RasterTilePriorityQueueRequired::Type::REQUIRED_FOR_ACTIVATION);
    if (tiling_set_queue && !tiling_set_queue->IsEmpty()) {
      queues->push_back(std::move(tiling_set_queue));
    }
  }
}

}  // namespace

RasterTilePriorityQueueRequired::RasterTilePriorityQueueRequired() = default;

RasterTilePriorityQueueRequired::~RasterTilePriorityQueueRequired() = default;

void RasterTilePriorityQueueRequired::Build(
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
        active_layers,
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
        pending_layers,
    Type type) {
  DCHECK_NE(static_cast<int>(type), static_cast<int>(Type::ALL));
  if (type == Type::REQUIRED_FOR_DRAW)
    BuildRequiredForDraw(active_layers);
  else
    BuildRequiredForActivation(active_layers, pending_layers);
}

void RasterTilePriorityQueueRequired::BuildRequiredForDraw(
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
        active_layers) {
  const bool cc_slimming_enabled = features::IsCCSlimmingEnabled();
  for (PictureLayerImpl* layer : active_layers) {
    if (!layer->HasValidTilePriorities())
      continue;

    PictureLayerTilingSet* tiling_set = layer->picture_layer_tiling_set();
    if (cc_slimming_enabled && tiling_set->all_tiles_done()) {
      continue;
    }
    std::unique_ptr<TilingSetRasterQueueRequired> tiling_set_queue =
        TilingSetRasterQueueRequired::Create(tiling_set,
                                             Type::REQUIRED_FOR_DRAW);
    if (tiling_set_queue && !tiling_set_queue->IsEmpty()) {
      tiling_set_queues_.push_back(std::move(tiling_set_queue));
    }
  }
}

void RasterTilePriorityQueueRequired::BuildRequiredForActivation(
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
        active_layers,
    const std::vector<raw_ptr<PictureLayerImpl, VectorExperimental>>&
        pending_layers) {
  AppendTilingSetRequiredQueues(active_layers, &tiling_set_queues_);
  AppendTilingSetRequiredQueues(pending_layers, &tiling_set_queues_);
}

bool RasterTilePriorityQueueRequired::IsEmpty() const {
  return tiling_set_queues_.empty();
}

const PrioritizedTile& RasterTilePriorityQueueRequired::Top() const {
  DCHECK(!IsEmpty());
  return tiling_set_queues_.back()->Top();
}

void RasterTilePriorityQueueRequired::Pop() {
  DCHECK(!IsEmpty());
  tiling_set_queues_.back()->Pop();
  if (tiling_set_queues_.back()->IsEmpty())
    tiling_set_queues_.pop_back();
}

}  // namespace cc
