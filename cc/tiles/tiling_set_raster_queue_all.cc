// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/tiles/tiling_set_raster_queue_all.h"

#include <stddef.h>

#include <utility>

#include "base/notreached.h"
#include "cc/base/features.h"
#include "cc/tiles/picture_layer_tiling_set.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_priority.h"

namespace cc {

TilingSetRasterQueueAll::IterationStage::IterationStage(
    IteratorType type,
    TilePriority::PriorityBin bin)
    : iterator_type(type), tile_type(bin) {
}

// static
std::unique_ptr<TilingSetRasterQueueAll> TilingSetRasterQueueAll::Create(
    PictureLayerTilingSet* tiling_set,
    bool prioritize_low_res,
    bool is_drawing_layer) {
  DCHECK(tiling_set);

  // Early out if the tiling set has no tiles needing raster.
  if (features::IsCCSlimmingEnabled()) {
    if (tiling_set->all_tiles_done()) {
      return nullptr;
    }
  } else {
    if (!tiling_set->num_tilings()) {
      return base::WrapUnique(new TilingSetRasterQueueAll(
          nullptr, nullptr, nullptr, is_drawing_layer));
    }
  }

  const PictureLayerTilingClient* client = tiling_set->client();
  WhichTree tree = tiling_set->tree();
  // Find high and low res tilings and initialize the iterators.
  PictureLayerTiling* high_res_tiling = nullptr;
  PictureLayerTiling* low_res_tiling = nullptr;
  // This variable would point to a tiling that has a NON_IDEAL_RESOLUTION or
  // LOW_RESOLUTION on the active tree, but HIGH_RESOLUTION on the pending tree.
  // These tilings are the only non-high res tilings that could have required
  // for activation tiles, so they need to be considered for rasterization.
  PictureLayerTiling* active_non_ideal_pending_high_res_tiling = nullptr;
  for (size_t i = 0; i < tiling_set->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(i);
    if (tiling->resolution() == HIGH_RESOLUTION)
      high_res_tiling = tiling;
    if (prioritize_low_res && tiling->resolution() == LOW_RESOLUTION)
      low_res_tiling = tiling;
    if (tree == ACTIVE_TREE && tiling->resolution() != HIGH_RESOLUTION) {
      const PictureLayerTiling* twin =
          client->GetPendingOrActiveTwinTiling(tiling);
      if (twin && twin->resolution() == HIGH_RESOLUTION)
        active_non_ideal_pending_high_res_tiling = tiling;
    }
  }

  bool use_low_res_tiling = low_res_tiling && low_res_tiling->has_tiles() &&
                            !low_res_tiling->all_tiles_done();
  bool use_high_res_tiling = high_res_tiling && high_res_tiling->has_tiles() &&
                             !high_res_tiling->all_tiles_done();
  bool use_active_non_ideal_pending_high_res_tiling =
      active_non_ideal_pending_high_res_tiling &&
      active_non_ideal_pending_high_res_tiling->has_tiles() &&
      !active_non_ideal_pending_high_res_tiling->all_tiles_done();

  if (!use_low_res_tiling && !use_high_res_tiling &&
      !use_active_non_ideal_pending_high_res_tiling &&
      features::IsCCSlimmingEnabled()) {
    return nullptr;
  }

  return base::WrapUnique(new TilingSetRasterQueueAll(
      use_high_res_tiling ? high_res_tiling : nullptr,
      use_low_res_tiling ? low_res_tiling : nullptr,
      use_active_non_ideal_pending_high_res_tiling
          ? active_non_ideal_pending_high_res_tiling
          : nullptr,
      is_drawing_layer));
}

TilingSetRasterQueueAll::TilingSetRasterQueueAll(
    PictureLayerTiling* high_res_tiling,
    PictureLayerTiling* low_res_tiling,
    PictureLayerTiling* active_non_ideal_pending_high_res_tiling,
    bool is_drawing_layer)
    : current_stage_(0), is_drawing_layer_(is_drawing_layer) {
  if (!high_res_tiling && !low_res_tiling &&
      !active_non_ideal_pending_high_res_tiling) {
    DCHECK(!features::IsCCSlimmingEnabled());
    return;
  }

  // Make the tiling iterators.
  if (low_res_tiling) {
    MakeTilingIterator(LOW_RES, low_res_tiling);
  } else if (!features::IsCCSlimmingEnabled()) {
    iterators_[LOW_RES].emplace();
  }
  if (high_res_tiling) {
    MakeTilingIterator(HIGH_RES, high_res_tiling);
  } else if (!features::IsCCSlimmingEnabled()) {
    iterators_[HIGH_RES].emplace();
  }
  if (active_non_ideal_pending_high_res_tiling) {
    MakeTilingIterator(ACTIVE_NON_IDEAL_PENDING_HIGH_RES,
                       active_non_ideal_pending_high_res_tiling);
  } else if (!features::IsCCSlimmingEnabled()) {
    iterators_[ACTIVE_NON_IDEAL_PENDING_HIGH_RES].emplace();
  }

  // Set up the stages.
  if (low_res_tiling) {
    stages_.push_back(IterationStage(LOW_RES, TilePriority::NOW));
  }

  if (high_res_tiling) {
    stages_.push_back(IterationStage(HIGH_RES, TilePriority::NOW));
  }

  if (active_non_ideal_pending_high_res_tiling) {
    stages_.push_back(
        IterationStage(ACTIVE_NON_IDEAL_PENDING_HIGH_RES, TilePriority::NOW));
    stages_.push_back(
        IterationStage(ACTIVE_NON_IDEAL_PENDING_HIGH_RES, TilePriority::SOON));
  }

  if (high_res_tiling) {
    stages_.push_back(IterationStage(HIGH_RES, TilePriority::SOON));
    stages_.push_back(IterationStage(HIGH_RES, TilePriority::EVENTUALLY));
  }

  DCHECK(!stages_.empty());

  IteratorType index = stages_[current_stage_].iterator_type;
  TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;
  if (!iterators_[index] || iterators_[index]->done() ||
      iterators_[index]->type() != tile_type) {
    AdvanceToNextStage();
  }
}

TilingSetRasterQueueAll::~TilingSetRasterQueueAll() = default;

void TilingSetRasterQueueAll::MakeTilingIterator(IteratorType type,
                                                 PictureLayerTiling* tiling) {
  iterators_[type].emplace(tiling, &tiling->tiling_data_);
  if (iterators_[type]->done()) {
    tiling->set_all_tiles_done(true);
    // If we've marked the tiling as done, make sure we're actually done.
    tiling->VerifyNoTileNeedsRaster();
  }
}

bool TilingSetRasterQueueAll::IsEmpty() const {
  return current_stage_ >= stages_.size();
}

void TilingSetRasterQueueAll::Pop() {
  IteratorType index = stages_[current_stage_].iterator_type;
  TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;

  // First advance the iterator.
  DCHECK(iterators_[index]);
  DCHECK(!iterators_[index]->done());
  DCHECK(iterators_[index]->type() == tile_type);
  ++(*iterators_[index]);

  if (!iterators_[index] || iterators_[index]->done() ||
      iterators_[index]->type() != tile_type) {
    AdvanceToNextStage();
  }
}

const PrioritizedTile& TilingSetRasterQueueAll::Top() const {
  DCHECK(!IsEmpty());

  IteratorType index = stages_[current_stage_].iterator_type;
  DCHECK(iterators_[index]);
  DCHECK(!iterators_[index]->done());
  DCHECK(iterators_[index]->type() == stages_[current_stage_].tile_type);

  return **iterators_[index];
}

void TilingSetRasterQueueAll::AdvanceToNextStage() {
  DCHECK_LT(current_stage_, stages_.size());
  ++current_stage_;
  while (current_stage_ < stages_.size()) {
    IteratorType index = stages_[current_stage_].iterator_type;
    TilePriority::PriorityBin tile_type = stages_[current_stage_].tile_type;

    if (iterators_[index] && !iterators_[index]->done() &&
        iterators_[index]->type() == tile_type) {
      break;
    }
    ++current_stage_;
  }
}

// OnePriorityRectIterator
TilingSetRasterQueueAll::OnePriorityRectIterator::OnePriorityRectIterator()
    : tiling_(nullptr), tiling_data_(nullptr) {
}

TilingSetRasterQueueAll::OnePriorityRectIterator::OnePriorityRectIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data,
    PictureLayerTiling::PriorityRectType priority_rect_type)
    : tiling_(tiling),
      tiling_data_(tiling_data),
      priority_rect_type_(priority_rect_type),
      pending_visible_rect_(tiling->pending_visible_rect()) {
}

template <typename TilingIteratorType>
void TilingSetRasterQueueAll::OnePriorityRectIterator::AdvanceToNextTile(
    TilingIteratorType* iterator) {
  for (;;) {
    ++(*iterator);
    if (!(*iterator)) {
      current_tile_ = PrioritizedTile();
      break;
    }
    Tile* tile = tiling_->TileAt(iterator->index_x(), iterator->index_y());
    auto result = IsTileValid(tile);
    if (result == kTileNotValid)
      continue;

    bool is_tile_occluded =
        result != kTileNeedsRaster && tiling_->IsTileOccluded(tile);
    current_tile_ = tiling_->MakePrioritizedTile(tile, priority_rect_type_,
                                                 is_tile_occluded);
    break;
  }
}

template <typename TilingIteratorType>
bool TilingSetRasterQueueAll::OnePriorityRectIterator::
    GetFirstTileAndCheckIfValid(TilingIteratorType* iterator) {
  Tile* tile = tiling_->TileAt(iterator->index_x(), iterator->index_y());
  auto result = IsTileValid(tile);
  if (result == kTileNotValid) {
    current_tile_ = PrioritizedTile();
    return false;
  }
  // Note that if tile needs raster then by definition it is not occluded.
  bool is_tile_occluded =
      result != kTileNeedsRaster && tiling_->IsTileOccluded(tile);
  current_tile_ =
      tiling_->MakePrioritizedTile(tile, priority_rect_type_, is_tile_occluded);
  return true;
}

TilingSetRasterQueueAll::OnePriorityRectIterator::IsTileValidResult
TilingSetRasterQueueAll::OnePriorityRectIterator::IsTileValid(
    const Tile* tile) const {
  if (!tile)
    return kTileNotValid;

  // A tile is valid for raster if it needs raster and is unoccluded.
  bool tile_is_valid_for_raster =
      tile->draw_info().NeedsRaster() && !tiling_->IsTileOccluded(tile);

  // A tile is not valid for the raster queue if it is not valid for raster or
  // processing for checker-images.
  IsTileValidResult result = kTileNeedsRaster;
  if (!tile_is_valid_for_raster) {
    // Note that we might need to re-raster the tile if it was checker imaged.
    bool tile_is_valid_for_checker_images =
        tile->draw_info().is_checker_imaged() &&
        tiling_->ShouldDecodeCheckeredImagesForTile(tile);
    if (!tile_is_valid_for_checker_images)
      return kTileNotValid;
    result = kTileNeedsCheckerImageReraster;
  }

  // After the pending visible rect has been processed, we must return false
  // for pending visible rect tiles as tiling iterators do not ignore those
  // tiles.
  if (priority_rect_type_ > PictureLayerTiling::PENDING_VISIBLE_RECT) {
    gfx::Rect tile_bounds = tiling_data_->TileBounds(tile->tiling_i_index(),
                                                     tile->tiling_j_index());
    if (pending_visible_rect_.Intersects(tile_bounds))
      return kTileNotValid;
  }
  return result;
}

// VisibleTilingIterator.
TilingSetRasterQueueAll::VisibleTilingIterator::VisibleTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling,
                              tiling_data,
                              PictureLayerTiling::VISIBLE_RECT) {
  if (!tiling_->has_visible_rect_tiles())
    return;
  iterator_ =
      TilingData::Iterator(tiling_data_, tiling_->current_visible_rect(),
                           false /* include_borders */);
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetRasterQueueAll::VisibleTilingIterator&
    TilingSetRasterQueueAll::VisibleTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  return *this;
}

// PendingVisibleTilingIterator.
TilingSetRasterQueueAll::PendingVisibleTilingIterator::
    PendingVisibleTilingIterator(PictureLayerTiling* tiling,
                                 TilingData* tiling_data)
    : OnePriorityRectIterator(tiling,
                              tiling_data,
                              PictureLayerTiling::PENDING_VISIBLE_RECT) {
  iterator_ = TilingData::DifferenceIterator(
      tiling_data_, pending_visible_rect_, tiling_->current_visible_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetRasterQueueAll::PendingVisibleTilingIterator&
    TilingSetRasterQueueAll::PendingVisibleTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  return *this;
}

// SkewportTilingIterator.
TilingSetRasterQueueAll::SkewportTilingIterator::SkewportTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling,
                              tiling_data,
                              PictureLayerTiling::SKEWPORT_RECT) {
  if (!tiling_->has_skewport_rect_tiles())
    return;
  iterator_ = TilingData::SpiralDifferenceIterator(
      tiling_data_, tiling_->current_skewport_rect(),
      tiling_->current_visible_rect(), tiling_->current_visible_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_)) {
    ++(*this);
    return;
  }
}

TilingSetRasterQueueAll::SkewportTilingIterator&
    TilingSetRasterQueueAll::SkewportTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  return *this;
}

// SoonBorderTilingIterator.
TilingSetRasterQueueAll::SoonBorderTilingIterator::SoonBorderTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling,
                              tiling_data,
                              PictureLayerTiling::SOON_BORDER_RECT) {
  if (!tiling_->has_soon_border_rect_tiles())
    return;
  iterator_ = TilingData::SpiralDifferenceIterator(
      tiling_data_, tiling_->current_soon_border_rect(),
      tiling_->current_skewport_rect(), tiling_->current_visible_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_)) {
    ++(*this);
    return;
  }
}

TilingSetRasterQueueAll::SoonBorderTilingIterator&
    TilingSetRasterQueueAll::SoonBorderTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  return *this;
}

// EventuallyTilingIterator.
TilingSetRasterQueueAll::EventuallyTilingIterator::EventuallyTilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : OnePriorityRectIterator(tiling,
                              tiling_data,
                              PictureLayerTiling::EVENTUALLY_RECT) {
  if (!tiling_->has_eventually_rect_tiles())
    return;
  iterator_ = TilingData::SpiralDifferenceIterator(
      tiling_data_, tiling_->current_eventually_rect(),
      tiling_->current_skewport_rect(), tiling_->current_soon_border_rect());
  if (!iterator_)
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_)) {
    ++(*this);
    return;
  }
}

TilingSetRasterQueueAll::EventuallyTilingIterator&
    TilingSetRasterQueueAll::EventuallyTilingIterator::
    operator++() {
  AdvanceToNextTile(&iterator_);
  return *this;
}

// TilingIterator
TilingSetRasterQueueAll::TilingIterator::TilingIterator() : tiling_(nullptr) {
}

TilingSetRasterQueueAll::TilingIterator::TilingIterator(
    PictureLayerTiling* tiling,
    TilingData* tiling_data)
    : tiling_(tiling), tiling_data_(tiling_data), phase_(Phase::VISIBLE_RECT) {
  visible_iterator_ = VisibleTilingIterator(tiling_, tiling_data_);
  if (visible_iterator_.done()) {
    AdvancePhase();
    return;
  }
  current_tile_ = *visible_iterator_;
}

TilingSetRasterQueueAll::TilingIterator::~TilingIterator() = default;

void TilingSetRasterQueueAll::TilingIterator::AdvancePhase() {
  DCHECK_LT(phase_, Phase::EVENTUALLY_RECT);

  current_tile_ = PrioritizedTile();
  while (!current_tile_.tile() && phase_ < Phase::EVENTUALLY_RECT) {
    phase_ = static_cast<Phase>(phase_ + 1);
    switch (phase_) {
      case Phase::VISIBLE_RECT:
        NOTREACHED();
      case Phase::PENDING_VISIBLE_RECT:
        pending_visible_iterator_ =
            PendingVisibleTilingIterator(tiling_, tiling_data_);
        if (!pending_visible_iterator_.done())
          current_tile_ = *pending_visible_iterator_;
        break;
      case Phase::SKEWPORT_RECT:
        skewport_iterator_ = SkewportTilingIterator(tiling_, tiling_data_);
        if (!skewport_iterator_.done())
          current_tile_ = *skewport_iterator_;
        break;
      case Phase::SOON_BORDER_RECT:
        soon_border_iterator_ = SoonBorderTilingIterator(tiling_, tiling_data_);
        if (!soon_border_iterator_.done())
          current_tile_ = *soon_border_iterator_;
        break;
      case Phase::EVENTUALLY_RECT:
        eventually_iterator_ = EventuallyTilingIterator(tiling_, tiling_data_);
        if (!eventually_iterator_.done())
          current_tile_ = *eventually_iterator_;
        break;
    }
  }
}

TilingSetRasterQueueAll::TilingIterator&
    TilingSetRasterQueueAll::TilingIterator::
    operator++() {
  switch (phase_) {
    case Phase::VISIBLE_RECT:
      ++visible_iterator_;
      if (visible_iterator_.done()) {
        AdvancePhase();
        return *this;
      }
      current_tile_ = *visible_iterator_;
      break;
    case Phase::PENDING_VISIBLE_RECT:
      ++pending_visible_iterator_;
      if (pending_visible_iterator_.done()) {
        AdvancePhase();
        return *this;
      }
      current_tile_ = *pending_visible_iterator_;
      break;
    case Phase::SKEWPORT_RECT:
      ++skewport_iterator_;
      if (skewport_iterator_.done()) {
        AdvancePhase();
        return *this;
      }
      current_tile_ = *skewport_iterator_;
      break;
    case Phase::SOON_BORDER_RECT:
      ++soon_border_iterator_;
      if (soon_border_iterator_.done()) {
        AdvancePhase();
        return *this;
      }
      current_tile_ = *soon_border_iterator_;
      break;
    case Phase::EVENTUALLY_RECT:
      ++eventually_iterator_;
      if (eventually_iterator_.done()) {
        current_tile_ = PrioritizedTile();
        return *this;
      }
      current_tile_ = *eventually_iterator_;
      break;
  }
  return *this;
}

}  // namespace cc
