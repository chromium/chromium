// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILING_SET_COVERAGE_ITERATOR_H_
#define CC_TILES_TILING_SET_COVERAGE_ITERATOR_H_

#include <algorithm>
#include <concepts>
#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "cc/base/region.h"
#include "cc/tiles/tile_index.h"
#include "cc/tiles/tiling_coverage_iterator.h"
#include "cc/tiles/tiling_internal.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

namespace internal {

// Tilings also need to provide a Cover() method which exposes an appropriate
// coverage iterator in order to be used with TilingSetCoverageIterator.
template <typename T>
concept TilingWithCover =
    requires(const T t, const gfx::Rect& rect, float scale) {
      { t.Cover(rect, scale) } -> std::derived_from<TilingCoverageIterator<T>>;
    };

}  // namespace internal

// TilingSetCoverageIterator iterates over the best, minimal set of drawable
// tiles to cover a given output rectangle.
template <typename T>
  requires internal::TilingWithCover<T>
class TilingSetCoverageIterator {
 public:
  using Container = std::vector<std::unique_ptr<T>>;
  using Tile = typename T::Tile;

  // Constructs an iterator to emit tiles filling `coverage_rect`, which is an
  // output rectangle that has been pre-scaled by `coverage_scale`. The iterator
  // will prefer to use tiles from tilings at `ideal_contents_scale`, falling
  // back onto larger and then smaller raster scales to fill in gaps as needed
  // where more ideal tiles aren't ready to draw.
  //
  // All tiles are drawn from `tilings`, which must contain one or more
  // objects of some type T which conforms to TilingWithCover as defined above.
  // `tilings` must be sorted in descending order of raster scale key.
  TilingSetCoverageIterator(const Container& tilings,
                            const gfx::Rect& coverage_rect,
                            float coverage_scale,
                            float ideal_contents_scale)
      : tilings_(tilings),
        coverage_scale_(coverage_scale),
        ideal_tiling_(FindIdealTiling(tilings_, ideal_contents_scale)),
        missing_region_(coverage_rect) {
    AdvanceUntilTileIsRelevant();
  }

  ~TilingSetCoverageIterator() = default;

  // Returns true if and only if this iterator has been initialized and any
  // portion of the coverage rect remains uncovered by the union of all
  // visited geometry rects so far. If true, at least CurrentTiling() and
  // geometry_rect() are safe to call.
  bool IsValid() const {
    return (current_tiling_ && *current_tiling_ != tilings_.end()) ||
           region_iter_ != current_region_.end();
  }
  explicit operator bool() const { return IsValid(); }

  // The tiling for the current iterator position. If this returns null but
  // IsValid() is true, then there are no more applicable tilings (and
  // therefore no more tiles to draw); but geometry_rect() is still meaningful
  // and should be checkerboarded.
  T* CurrentTiling() const {
    if (!current_tiling_ || current_tiling_ == tilings_.end()) {
      return nullptr;
    }
    return current_tiling_.value()->get();
  }

  Tile* operator*() const {
    return tiling_iter_.IsValid() ? *tiling_iter_ : nullptr;
  }
  Tile* operator->() const { return **this; }

  // The current output rectangle in pre-scaled coverage space. This is always
  // meaningful as long as IsValid() is true, even if there is no
  // CurrentTiling() or tile to cover the rect. Geometry rects across all
  // iterations are mutually non-overlapping and their total union comprises the
  // full coverage rect over which this iterator was constructed.
  gfx::Rect geometry_rect() const {
    if (tiling_iter_.IsValid()) {
      return tiling_iter_.geometry_rect();
    }

    if (region_iter_ != current_region_.end()) {
      return *region_iter_;
    }

    return gfx::Rect();
  }

  gfx::RectF texture_rect() const {
    if (tiling_iter_.IsValid()) {
      return tiling_iter_.texture_rect();
    }
    return gfx::RectF();
  }

  TileResolution resolution() const {
    const T* tiling = CurrentTiling();
    DCHECK(tiling);
    return tiling->resolution();
  }

  TilingSetCoverageIterator& operator++() {
    DCHECK(IsValid());
    AdvanceUntilTileIsRelevant();
    return *this;
  }

 private:
  using TilingIterator = typename Container::const_iterator;
  static TilingIterator FindIdealTiling(const Container& tilings,
                                        float ideal_contents_scale) {
    if (tilings.empty()) {
      return tilings.end();
    }

    // Determine the smallest-scale tiling with a scale higher than the ideal,
    // or the first tiling if all scales are less than the ideal.
    for (auto iter = tilings.begin(); iter != tilings.end(); ++iter) {
      if ((*iter)->contents_scale_key() < ideal_contents_scale) {
        return iter == tilings.begin() ? iter : iter - 1;
      }
    }

    // If all scale factors are at least as large as the ideal, use the
    // smallest (last) one.
    return tilings.end() - 1;
  }

  void AdvanceTiling() {
    // Order of tilings visited upon successive calls to this method is:
    //   1. Ideal tiling index
    //   2. Tiling indices < Ideal in decreasing order (higher res than ideal)
    //   3. Tiling indices > Ideal in increasing order (lower res than ideal)
    DCHECK(current_tiling_ != tilings_.end());
    if (!current_tiling_) {
      current_tiling_ = ideal_tiling_;
    } else if (*current_tiling_ > ideal_tiling_) {
      ++*current_tiling_;
    } else if (*current_tiling_ > tilings_.begin()) {
      --*current_tiling_;
    } else {
      current_tiling_ = ideal_tiling_;
      ++*current_tiling_;
    }
  }

  void AdvanceUntilTileIsRelevant() {
    if (!IsValid() && current_tiling_) {
      return;
    }

    if (tiling_iter_.IsValid()) {
      ++tiling_iter_;
    }

    while (true) {
      for (; tiling_iter_.IsValid(); ++tiling_iter_) {
        Tile* const tile = (**current_tiling_)->TileAt(tiling_iter_.index());
        if (tile && tile->IsReadyToDraw()) {
          return;
        }
        // For any tile which is not yet ready to draw, accumulate its
        // coverage back into the uncovered region so that subsequent tilings
        // may attempt to cover it.
        missing_region_.Union(tiling_iter_.geometry_rect());
      }

      // If the set of current rects for this tiling is done, or if this is
      // the first call at construction time, update the current tiling.
      if (region_iter_ == current_region_.end()) {
        AdvanceTiling();
        current_region_.Swap(&missing_region_);
        missing_region_.Clear();
        region_iter_ = current_region_.begin();
        if (region_iter_ == current_region_.end()) {
          // Region is fully covered.
          current_tiling_ = tilings_.end();
          return;
        }

        if (current_tiling_ == tilings_.end()) {
          // No more tilings. This and subsequent iterations will return null
          // tilings until we've iterated over the remaining geometry rects.
          return;
        }
      }

      gfx::Rect last_rect = *region_iter_;
      ++region_iter_;
      if (current_tiling_ == tilings_.end()) {
        return;
      }

      tiling_iter_ = (**current_tiling_)->Cover(last_rect, coverage_scale_);
    }
  }

  // RAW_PTR_EXCLUSION: Renderer performance: visible in sampling profiler
  // stacks.
  RAW_PTR_EXCLUSION const Container& tilings_;
  const float coverage_scale_;
  const TilingIterator ideal_tiling_;

  std::optional<TilingIterator> current_tiling_;
  Region missing_region_;
  Region current_region_;
  Region::Iterator region_iter_;

  TilingCoverageIterator<T> tiling_iter_;
};

}  // namespace cc

#endif  // CC_TILES_TILING_SET_COVERAGE_ITERATOR_H_
