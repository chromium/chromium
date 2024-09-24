// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILING_SET_RASTER_QUEUE_ALL_H_
#define CC_TILES_TILING_SET_RASTER_QUEUE_ALL_H_

#include <stddef.h>

#include <memory>
#include <optional>

#include "base/notreached.h"
#include "cc/cc_export.h"
#include "cc/tiles/prioritized_tile.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_priority.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace cc {

class PictureLayerTilingSet;

// This queue returns all tiles required to be rasterized from HIGH_RESOLUTION
// and LOW_RESOLUTION tilings.
class CC_EXPORT TilingSetRasterQueueAll {
 public:
  static std::unique_ptr<TilingSetRasterQueueAll> Create(
      PictureLayerTilingSet* tiling_set,
      bool prioritize_low_res,
      bool is_drawing_layer);

  TilingSetRasterQueueAll(const TilingSetRasterQueueAll&) = delete;
  ~TilingSetRasterQueueAll();

  TilingSetRasterQueueAll& operator=(const TilingSetRasterQueueAll&) = delete;

  const PrioritizedTile& Top() const;
  void Pop();
  bool IsEmpty() const;
  bool is_drawing_layer() const { return is_drawing_layer_; }

 private:
  // Helper base class for individual region iterators.
  class OnePriorityRectIterator {
   public:
    OnePriorityRectIterator();
    OnePriorityRectIterator(
        PictureLayerTiling* tiling,
        TilingData* tiling_data,
        PictureLayerTiling::PriorityRectType priority_rect_type);

    bool done() const { return !current_tile_.tile(); }
    const PrioritizedTile& operator*() const { return current_tile_; }

   protected:
    ~OnePriorityRectIterator() = default;

    template <typename TilingIteratorType>
    void AdvanceToNextTile(TilingIteratorType* iterator);
    template <typename TilingIteratorType>
    bool GetFirstTileAndCheckIfValid(TilingIteratorType* iterator);

    enum IsTileValidResult {
      kTileNotValid,
      kTileNeedsRaster,
      kTileNeedsCheckerImageReraster
    };
    IsTileValidResult IsTileValid(const Tile* tile) const;

    PrioritizedTile current_tile_;

    // `tiling_` and `tiling_data_` are not a raw_ptr<...> for performance
    // reasons (based on analysis of sampling profiler data and
    // tab_search:top100:2020).
    RAW_PTR_EXCLUSION PictureLayerTiling* tiling_;
    RAW_PTR_EXCLUSION TilingData* tiling_data_;

    PictureLayerTiling::PriorityRectType priority_rect_type_;
    gfx::Rect pending_visible_rect_;
  };

  // Iterates over visible rect only, left to right top to bottom order.
  class VisibleTilingIterator : public OnePriorityRectIterator {
   public:
    VisibleTilingIterator() = default;
    VisibleTilingIterator(PictureLayerTiling* tiling, TilingData* tiling_data);

    VisibleTilingIterator& operator++();

   private:
    TilingData::Iterator iterator_;
  };

  class PendingVisibleTilingIterator : public OnePriorityRectIterator {
   public:
    PendingVisibleTilingIterator() = default;
    PendingVisibleTilingIterator(PictureLayerTiling* tiling,
                                 TilingData* tiling_data);

    PendingVisibleTilingIterator& operator++();

   private:
    TilingData::DifferenceIterator iterator_;
  };

  // Iterates over skewport only, spiral around the visible rect.
  class SkewportTilingIterator : public OnePriorityRectIterator {
   public:
    SkewportTilingIterator() = default;
    SkewportTilingIterator(PictureLayerTiling* tiling, TilingData* tiling_data);

    SkewportTilingIterator& operator++();

   private:
    TilingData::SpiralDifferenceIterator iterator_;
  };

  // Iterates over soon border only, spiral around the visible rect.
  class SoonBorderTilingIterator : public OnePriorityRectIterator {
   public:
    SoonBorderTilingIterator() = default;
    SoonBorderTilingIterator(PictureLayerTiling* tiling,
                             TilingData* tiling_data);

    SoonBorderTilingIterator& operator++();

   private:
    TilingData::SpiralDifferenceIterator iterator_;
  };

  // Iterates over eventually rect only, spiral around the soon rect.
  class EventuallyTilingIterator : public OnePriorityRectIterator {
   public:
    EventuallyTilingIterator() = default;
    EventuallyTilingIterator(PictureLayerTiling* tiling,
                             TilingData* tiling_data);

    EventuallyTilingIterator& operator++();

   private:
    TilingData::SpiralDifferenceIterator iterator_;
  };

  // Iterates over all of the above phases in the following order: visible,
  // skewport, soon border, eventually.
  class TilingIterator {
   public:
    TilingIterator();
    TilingIterator(PictureLayerTiling* tiling, TilingData* tiling_data);
    ~TilingIterator();

    bool done() const { return !current_tile_.tile(); }
    const PrioritizedTile& operator*() const { return current_tile_; }
    TilePriority::PriorityBin type() const {
      switch (phase_) {
        case Phase::VISIBLE_RECT:
          return TilePriority::NOW;
        case Phase::PENDING_VISIBLE_RECT:
        case Phase::SKEWPORT_RECT:
        case Phase::SOON_BORDER_RECT:
          return TilePriority::SOON;
        case Phase::EVENTUALLY_RECT:
          return TilePriority::EVENTUALLY;
      }
      NOTREACHED();
    }

    TilingIterator& operator++();

   private:
    using Phase = PictureLayerTiling::PriorityRectType;

    void AdvancePhase();

    // `tiling_` and `tiling_data_` are not a raw_ptr<...> for performance
    // reasons (based on analysis of sampling profiler data and
    // tab_search:top100:2020).
    // These fields are not raw_ptr<> for performance based on sampling profiler
    // data and tab_search:top100:2020 profiler data.
    RAW_PTR_EXCLUSION PictureLayerTiling* tiling_;
    RAW_PTR_EXCLUSION TilingData* tiling_data_;

    Phase phase_;

    PrioritizedTile current_tile_;
    VisibleTilingIterator visible_iterator_;
    PendingVisibleTilingIterator pending_visible_iterator_;
    SkewportTilingIterator skewport_iterator_;
    SoonBorderTilingIterator soon_border_iterator_;
    EventuallyTilingIterator eventually_iterator_;
  };

  enum IteratorType {
    LOW_RES,
    HIGH_RES,
    ACTIVE_NON_IDEAL_PENDING_HIGH_RES,
    NUM_ITERATORS
  };

  TilingSetRasterQueueAll(
      PictureLayerTiling* high_res_tiling,
      PictureLayerTiling* low_res_tiling,
      PictureLayerTiling* active_non_ideal_pending_high_res_tiling,
      bool is_drawing_layer);

  void MakeTilingIterator(IteratorType type, PictureLayerTiling* tiling);
  void AdvanceToNextStage();

  struct IterationStage {
    IterationStage(IteratorType type, TilePriority::PriorityBin bin);
    IteratorType iterator_type;
    TilePriority::PriorityBin tile_type;
  };

  size_t current_stage_;

  // The max number of stages is 6: 1 low res, 3 high res, and 2 active non
  // ideal pending high res.
  absl::InlinedVector<IterationStage, 6> stages_;
  std::optional<TilingIterator> iterators_[NUM_ITERATORS];
  bool is_drawing_layer_ = false;
};

}  // namespace cc

#endif  // CC_TILES_TILING_SET_RASTER_QUEUE_ALL_H_
