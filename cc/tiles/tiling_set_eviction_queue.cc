// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>

#include "base/notreached.h"
#include "cc/tiles/tiling_set_eviction_queue.h"

namespace cc {

TilingSetEvictionQueue::TilingSetEvictionQueue(
    PictureLayerTilingSet* tiling_set,
    bool is_drawing_layer)
    : tree_(tiling_set->tree()),
      phase_(EVENTUALLY_RECT),
      is_drawing_layer_(is_drawing_layer) {
  // Early out if the layer has no tilings.
  if (!tiling_set->num_tilings())
    return;
  GenerateTilingOrder(tiling_set);
  eventually_iterator_ = EventuallyTilingIterator(&tilings_, tree_);
  if (eventually_iterator_.done()) {
    AdvancePhase();
    return;
  }
  current_tile_ = *eventually_iterator_;
}

TilingSetEvictionQueue::~TilingSetEvictionQueue() = default;

void TilingSetEvictionQueue::GenerateTilingOrder(
    PictureLayerTilingSet* tiling_set) {
  tilings_.reserve(tiling_set->num_tilings());
  // Generate all of the tilings in the order described in the header comment
  // for this class.
  PictureLayerTilingSet::TilingRange range =
      tiling_set->GetTilingRange(PictureLayerTilingSet::HIGHER_THAN_HIGH_RES);
  for (size_t index = range.start; index < range.end; ++index) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(index);
    if (tiling->has_tiles())
      tilings_.push_back(tiling);
  }

  range = tiling_set->GetTilingRange(PictureLayerTilingSet::LOWER_THAN_LOW_RES);
  for (size_t i = range.start; i < range.end; ++i) {
    size_t index = range.start + (range.end - 1 - i);
    PictureLayerTiling* tiling = tiling_set->tiling_at(index);
    if (tiling->has_tiles())
      tilings_.push_back(tiling);
  }

  range = tiling_set->GetTilingRange(
      PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES);
  for (size_t i = range.start; i < range.end; ++i) {
    size_t index = range.start + (range.end - 1 - i);
    PictureLayerTiling* tiling = tiling_set->tiling_at(index);
    if (tiling->has_tiles())
      tilings_.push_back(tiling);
  }

  range = tiling_set->GetTilingRange(PictureLayerTilingSet::LOW_RES);
  for (size_t index = range.start; index < range.end; ++index) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(index);
    if (tiling->has_tiles())
      tilings_.push_back(tiling);
  }

  range = tiling_set->GetTilingRange(PictureLayerTilingSet::HIGH_RES);
  for (size_t index = range.start; index < range.end; ++index) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(index);
    if (tiling->has_tiles())
      tilings_.push_back(tiling);
  }
  DCHECK_GE(tiling_set->num_tilings(), tilings_.size());
}

void TilingSetEvictionQueue::AdvancePhase() {
  current_tile_ = PrioritizedTile();
  while (!current_tile_.tile() &&
         phase_ != VISIBLE_RECT_REQUIRED_FOR_ACTIVATION_UNOCCLUDED) {
    phase_ = static_cast<Phase>(phase_ + 1);
    switch (phase_) {
      case EVENTUALLY_RECT:
        NOTREACHED();
      case SOON_BORDER_RECT:
        soon_iterator_ = SoonBorderTilingIterator(&tilings_, tree_);
        if (!soon_iterator_.done())
          current_tile_ = *soon_iterator_;
        break;
      case SKEWPORT_RECT:
        skewport_iterator_ = SkewportTilingIterator(&tilings_, tree_);
        if (!skewport_iterator_.done())
          current_tile_ = *skewport_iterator_;
        break;
      case PENDING_VISIBLE_RECT:
        pending_visible_iterator_ = PendingVisibleTilingIterator(
            &tilings_, tree_, false /* return required for activation tiles */);
        if (!pending_visible_iterator_.done())
          current_tile_ = *pending_visible_iterator_;
        break;
      case PENDING_VISIBLE_RECT_REQUIRED_FOR_ACTIVATION:
        pending_visible_iterator_ = PendingVisibleTilingIterator(
            &tilings_, tree_, true /* return required for activation tiles */);
        if (!pending_visible_iterator_.done())
          current_tile_ = *pending_visible_iterator_;
        break;
      case VISIBLE_RECT_OCCLUDED:
        visible_iterator_ = VisibleTilingIterator(
            &tilings_, tree_, true /* return occluded tiles */,
            false /* return required for activation tiles */);
        if (!visible_iterator_.done())
          current_tile_ = *visible_iterator_;
        break;
      case VISIBLE_RECT_UNOCCLUDED:
        visible_iterator_ = VisibleTilingIterator(
            &tilings_, tree_, false /* return occluded tiles */,
            false /* return required for activation tiles */);
        if (!visible_iterator_.done())
          current_tile_ = *visible_iterator_;
        break;
      case VISIBLE_RECT_REQUIRED_FOR_ACTIVATION_OCCLUDED:
        visible_iterator_ = VisibleTilingIterator(
            &tilings_, tree_, true /* return occluded tiles */,
            true /* return required for activation tiles */);
        if (!visible_iterator_.done())
          current_tile_ = *visible_iterator_;
        break;
      case VISIBLE_RECT_REQUIRED_FOR_ACTIVATION_UNOCCLUDED:
        visible_iterator_ = VisibleTilingIterator(
            &tilings_, tree_, false /* return occluded tiles */,
            true /* return required for activation tiles */);
        if (!visible_iterator_.done())
          current_tile_ = *visible_iterator_;
        break;
    }
  }
}

bool TilingSetEvictionQueue::IsEmpty() const {
  return !current_tile_.tile();
}

const PrioritizedTile& TilingSetEvictionQueue::Top() const {
  DCHECK(!IsEmpty());
  return current_tile_;
}

void TilingSetEvictionQueue::Pop() {
  DCHECK(!IsEmpty());
  current_tile_ = PrioritizedTile();
  switch (phase_) {
    case EVENTUALLY_RECT:
      ++eventually_iterator_;
      if (!eventually_iterator_.done())
        current_tile_ = *eventually_iterator_;
      break;
    case SOON_BORDER_RECT:
      ++soon_iterator_;
      if (!soon_iterator_.done())
        current_tile_ = *soon_iterator_;
      break;
    case SKEWPORT_RECT:
      ++skewport_iterator_;
      if (!skewport_iterator_.done())
        current_tile_ = *skewport_iterator_;
      break;
    case PENDING_VISIBLE_RECT:
    case PENDING_VISIBLE_RECT_REQUIRED_FOR_ACTIVATION:
      ++pending_visible_iterator_;
      if (!pending_visible_iterator_.done())
        current_tile_ = *pending_visible_iterator_;
      break;
    case VISIBLE_RECT_OCCLUDED:
    case VISIBLE_RECT_UNOCCLUDED:
    case VISIBLE_RECT_REQUIRED_FOR_ACTIVATION_OCCLUDED:
    case VISIBLE_RECT_REQUIRED_FOR_ACTIVATION_UNOCCLUDED:
      ++visible_iterator_;
      if (!visible_iterator_.done())
        current_tile_ = *visible_iterator_;
      break;
  }
  if (!current_tile_.tile())
    AdvancePhase();
}

// EvictionRectIterator
TilingSetEvictionQueue::EvictionRectIterator::EvictionRectIterator()
    : tilings_(nullptr), tree_(ACTIVE_TREE), tiling_index_(0) {
}

TilingSetEvictionQueue::EvictionRectIterator::EvictionRectIterator(
    std::vector<PictureLayerTiling*>* tilings,
    WhichTree tree,
    PictureLayerTiling::PriorityRectType priority_rect_type)
    : tilings_(tilings),
      tree_(tree),
      priority_rect_type_(priority_rect_type),
      tiling_index_(0) {
}

template <typename TilingIteratorType>
bool TilingSetEvictionQueue::EvictionRectIterator::AdvanceToNextTile(
    TilingIteratorType* iterator) {
  bool found_tile = false;
  while (!found_tile) {
    ++(*iterator);
    if (!(*iterator)) {
      prioritized_tile_ = PrioritizedTile();
      break;
    }
    found_tile = GetFirstTileAndCheckIfValid(iterator);
  }
  return found_tile;
}

template <typename TilingIteratorType>
bool TilingSetEvictionQueue::EvictionRectIterator::GetFirstTileAndCheckIfValid(
    TilingIteratorType* iterator) {
  PictureLayerTiling* tiling = (*tilings_)[tiling_index_];
  Tile* tile = tiling->TileAt(iterator->index_x(), iterator->index_y());
  prioritized_tile_ = PrioritizedTile();
  // If there's nothing to evict, return false.
  if (!tile || !tile->draw_info().has_resource())
    return false;
  // After the pending visible rect has been processed, we must return false
  // for pending visible rect tiles as tiling iterators do not ignore those
  // tiles.
  if (priority_rect_type_ > PictureLayerTiling::PENDING_VISIBLE_RECT) {
    gfx::Rect tile_rect = tiling->tiling_data()->TileBounds(
        tile->tiling_i_index(), tile->tiling_j_index());
    if (tiling->pending_visible_rect().Intersects(tile_rect))
      return false;
  }
  prioritized_tile_ = tiling->MakePrioritizedTile(tile, priority_rect_type_,
                                                  tiling->IsTileOccluded(tile));
  // In other cases, the tile we got is a viable candidate, return true.
  return true;
}

// EventuallyTilingIterator
TilingSetEvictionQueue::EventuallyTilingIterator::EventuallyTilingIterator(
    std::vector<PictureLayerTiling*>* tilings,
    WhichTree tree)
    : EvictionRectIterator(tilings, tree, PictureLayerTiling::EVENTUALLY_RECT) {
  // Find the first tiling with a tile.
  while (tiling_index_ < tilings_->size()) {
    if (!(*tilings_)[tiling_index_]->has_eventually_rect_tiles()) {
      ++tiling_index_;
      continue;
    }
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_eventually_rect(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_soon_border_rect());
    if (!iterator_) {
      ++tiling_index_;
      continue;
    }
    break;
  }
  if (tiling_index_ >= tilings_->size())
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetEvictionQueue::EventuallyTilingIterator&
    TilingSetEvictionQueue::EventuallyTilingIterator::
    operator++() {
  bool found_tile = AdvanceToNextTile(&iterator_);
  while (!found_tile && (tiling_index_ + 1) < tilings_->size()) {
    ++tiling_index_;
    if (!(*tilings_)[tiling_index_]->has_eventually_rect_tiles())
      continue;
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_eventually_rect(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_soon_border_rect());
    if (!iterator_)
      continue;
    found_tile = GetFirstTileAndCheckIfValid(&iterator_);
    if (!found_tile)
      found_tile = AdvanceToNextTile(&iterator_);
  }
  return *this;
}

// SoonBorderTilingIterator
TilingSetEvictionQueue::SoonBorderTilingIterator::SoonBorderTilingIterator(
    std::vector<PictureLayerTiling*>* tilings,
    WhichTree tree)
    : EvictionRectIterator(tilings,
                           tree,
                           PictureLayerTiling::SOON_BORDER_RECT) {
  // Find the first tiling with a tile.
  while (tiling_index_ < tilings_->size()) {
    if (!(*tilings_)[tiling_index_]->has_soon_border_rect_tiles()) {
      ++tiling_index_;
      continue;
    }
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_soon_border_rect(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_) {
      ++tiling_index_;
      continue;
    }
    break;
  }
  if (tiling_index_ >= tilings_->size())
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetEvictionQueue::SoonBorderTilingIterator&
    TilingSetEvictionQueue::SoonBorderTilingIterator::
    operator++() {
  bool found_tile = AdvanceToNextTile(&iterator_);
  while (!found_tile && (tiling_index_ + 1) < tilings_->size()) {
    ++tiling_index_;
    if (!(*tilings_)[tiling_index_]->has_soon_border_rect_tiles())
      continue;
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_soon_border_rect(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_)
      continue;
    found_tile = GetFirstTileAndCheckIfValid(&iterator_);
    if (!found_tile)
      found_tile = AdvanceToNextTile(&iterator_);
  }
  return *this;
}

// SkewportTilingIterator
TilingSetEvictionQueue::SkewportTilingIterator::SkewportTilingIterator(
    std::vector<PictureLayerTiling*>* tilings,
    WhichTree tree)
    : EvictionRectIterator(tilings, tree, PictureLayerTiling::SKEWPORT_RECT) {
  // Find the first tiling with a tile.
  while (tiling_index_ < tilings_->size()) {
    if (!(*tilings_)[tiling_index_]->has_skewport_rect_tiles()) {
      ++tiling_index_;
      continue;
    }
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_) {
      ++tiling_index_;
      continue;
    }
    break;
  }
  if (tiling_index_ >= tilings_->size())
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_))
    ++(*this);
}

TilingSetEvictionQueue::SkewportTilingIterator&
    TilingSetEvictionQueue::SkewportTilingIterator::
    operator++() {
  bool found_tile = AdvanceToNextTile(&iterator_);
  while (!found_tile && (tiling_index_ + 1) < tilings_->size()) {
    ++tiling_index_;
    if (!(*tilings_)[tiling_index_]->has_skewport_rect_tiles())
      continue;
    iterator_ = TilingData::ReverseSpiralDifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_skewport_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_)
      continue;
    found_tile = GetFirstTileAndCheckIfValid(&iterator_);
    if (!found_tile)
      found_tile = AdvanceToNextTile(&iterator_);
  }
  return *this;
}

// PendingVisibleIterator
TilingSetEvictionQueue::PendingVisibleTilingIterator::
    PendingVisibleTilingIterator(std::vector<PictureLayerTiling*>* tilings,
                                 WhichTree tree,
                                 bool return_required_for_activation_tiles)
    : EvictionRectIterator(tilings,
                           tree,
                           PictureLayerTiling::PENDING_VISIBLE_RECT),
      return_required_for_activation_tiles_(
          return_required_for_activation_tiles) {
  // Find the first tiling with a tile.
  while (tiling_index_ < tilings_->size()) {
    iterator_ = TilingData::DifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->pending_visible_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_) {
      ++tiling_index_;
      continue;
    }
    break;
  }
  if (tiling_index_ >= tilings_->size())
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_)) {
    ++(*this);
    return;
  }
  if (!TileMatchesRequiredFlags(prioritized_tile_)) {
    ++(*this);
    return;
  }
}

TilingSetEvictionQueue::PendingVisibleTilingIterator&
    TilingSetEvictionQueue::PendingVisibleTilingIterator::
    operator++() {
  bool found_tile = AdvanceToNextTile(&iterator_);
  while (found_tile && !TileMatchesRequiredFlags(prioritized_tile_))
    found_tile = AdvanceToNextTile(&iterator_);

  while (!found_tile && (tiling_index_ + 1) < tilings_->size()) {
    ++tiling_index_;
    iterator_ = TilingData::DifferenceIterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->pending_visible_rect(),
        (*tilings_)[tiling_index_]->current_visible_rect());
    if (!iterator_)
      continue;
    found_tile = GetFirstTileAndCheckIfValid(&iterator_);
    if (!found_tile)
      found_tile = AdvanceToNextTile(&iterator_);
    while (found_tile && !TileMatchesRequiredFlags(prioritized_tile_))
      found_tile = AdvanceToNextTile(&iterator_);
  }
  return *this;
}

bool TilingSetEvictionQueue::PendingVisibleTilingIterator::
    TileMatchesRequiredFlags(const PrioritizedTile& tile) const {
  bool activation_flag_matches = tile.tile()->required_for_activation() ==
                                 return_required_for_activation_tiles_;
  return activation_flag_matches;
}

// VisibleTilingIterator
TilingSetEvictionQueue::VisibleTilingIterator::VisibleTilingIterator(
    std::vector<PictureLayerTiling*>* tilings,
    WhichTree tree,
    bool return_occluded_tiles,
    bool return_required_for_activation_tiles)
    : EvictionRectIterator(tilings, tree, PictureLayerTiling::VISIBLE_RECT),
      return_occluded_tiles_(return_occluded_tiles),
      return_required_for_activation_tiles_(
          return_required_for_activation_tiles) {
  // Find the first tiling with a tile.
  while (tiling_index_ < tilings_->size()) {
    if (!(*tilings_)[tiling_index_]->has_visible_rect_tiles()) {
      ++tiling_index_;
      continue;
    }
    iterator_ = TilingData::Iterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_visible_rect(), false);
    if (!iterator_) {
      ++tiling_index_;
      continue;
    }
    break;
  }
  if (tiling_index_ >= tilings_->size())
    return;
  if (!GetFirstTileAndCheckIfValid(&iterator_)) {
    ++(*this);
    return;
  }
  if (!TileMatchesRequiredFlags(prioritized_tile_)) {
    ++(*this);
    return;
  }
}

TilingSetEvictionQueue::VisibleTilingIterator&
    TilingSetEvictionQueue::VisibleTilingIterator::
    operator++() {
  bool found_tile = AdvanceToNextTile(&iterator_);
  while (found_tile && !TileMatchesRequiredFlags(prioritized_tile_))
    found_tile = AdvanceToNextTile(&iterator_);

  while (!found_tile && (tiling_index_ + 1) < tilings_->size()) {
    ++tiling_index_;
    if (!(*tilings_)[tiling_index_]->has_visible_rect_tiles())
      continue;
    iterator_ = TilingData::Iterator(
        (*tilings_)[tiling_index_]->tiling_data(),
        (*tilings_)[tiling_index_]->current_visible_rect(), false);
    if (!iterator_)
      continue;
    found_tile = GetFirstTileAndCheckIfValid(&iterator_);
    if (!found_tile)
      found_tile = AdvanceToNextTile(&iterator_);
    while (found_tile && !TileMatchesRequiredFlags(prioritized_tile_))
      found_tile = AdvanceToNextTile(&iterator_);
  }
  return *this;
}

bool TilingSetEvictionQueue::VisibleTilingIterator::TileMatchesRequiredFlags(
    const PrioritizedTile& tile) const {
  bool activation_flag_matches = tile.tile()->required_for_activation() ==
                                 return_required_for_activation_tiles_;
  bool occluded_flag_matches = tile.is_occluded() == return_occluded_tiles_;
  return activation_flag_matches && occluded_flag_matches;
}

}  // namespace cc
