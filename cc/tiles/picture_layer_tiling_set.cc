// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/picture_layer_tiling_set.h"

#include <stddef.h>

#include <limits>
#include <set>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/raster/raster_source.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

namespace {

class LargestToSmallestScaleFunctor {
 public:
  bool operator()(const std::unique_ptr<PictureLayerTiling>& left,
                  const std::unique_ptr<PictureLayerTiling>& right) {
    return left->contents_scale_key() > right->contents_scale_key();
  }
};

inline float LargerRatio(float float1, float float2) {
  DCHECK_GT(float1, 0.f);
  DCHECK_GT(float2, 0.f);
  return float1 > float2 ? float1 / float2 : float2 / float1;
}

const float kSoonBorderDistanceViewportPercentage = 0.15f;
const float kMaxSoonBorderDistanceInScreenPixels = 312.f;

}  // namespace

// static
std::unique_ptr<PictureLayerTilingSet> PictureLayerTilingSet::Create(
    WhichTree tree,
    PictureLayerTilingClient* client,
    int tiling_interest_area_padding,
    float skewport_target_time_in_seconds,
    int skewport_extrapolation_limit_in_screen_pixels,
    float max_preraster_distance) {
  return base::WrapUnique(new PictureLayerTilingSet(
      tree, client, tiling_interest_area_padding,
      skewport_target_time_in_seconds,
      skewport_extrapolation_limit_in_screen_pixels, max_preraster_distance));
}

PictureLayerTilingSet::PictureLayerTilingSet(
    WhichTree tree,
    PictureLayerTilingClient* client,
    int tiling_interest_area_padding,
    float skewport_target_time_in_seconds,
    int skewport_extrapolation_limit_in_screen_pixels,
    float max_preraster_distance)
    : tiling_interest_area_padding_(tiling_interest_area_padding),
      skewport_target_time_in_seconds_(skewport_target_time_in_seconds),
      skewport_extrapolation_limit_in_screen_pixels_(
          skewport_extrapolation_limit_in_screen_pixels),
      tree_(tree),
      client_(client),
      max_preraster_distance_(max_preraster_distance) {}

PictureLayerTilingSet::~PictureLayerTilingSet() = default;

void PictureLayerTilingSet::CopyTilingsAndPropertiesFromPendingTwin(
    const PictureLayerTilingSet* pending_twin_set,
    scoped_refptr<RasterSource> raster_source,
    const Region& layer_invalidation) {
  if (pending_twin_set->tilings_.empty()) {
    // If the twin (pending) tiling set is empty, it was not updated for the
    // current frame. So we drop tilings from our set as well, instead of
    // leaving behind unshared tilings that are all non-ideal.
    RemoveAllTilings();
    return;
  }

  bool tiling_sort_required = false;
  for (const auto& pending_twin_tiling : pending_twin_set->tilings_) {
    gfx::AxisTransform2d raster_transform =
        pending_twin_tiling->raster_transform();
    PictureLayerTiling* this_tiling =
        FindTilingWithScaleKey(pending_twin_tiling->contents_scale_key());
    if (this_tiling && this_tiling->raster_transform() != raster_transform) {
      Remove(this_tiling);
      this_tiling = nullptr;
    }
    if (!this_tiling) {
      std::unique_ptr<PictureLayerTiling> new_tiling(new PictureLayerTiling(
          tree_, raster_transform, raster_source_, client_,
          kMaxSoonBorderDistanceInScreenPixels, max_preraster_distance_));
      tilings_.push_back(std::move(new_tiling));
      this_tiling = tilings_.back().get();
      tiling_sort_required = true;
      state_since_last_tile_priority_update_.added_tilings = true;
    }
    this_tiling->TakeTilesAndPropertiesFrom(pending_twin_tiling.get(),
                                            layer_invalidation);
  }

  if (tiling_sort_required) {
    std::sort(tilings_.begin(), tilings_.end(),
              LargestToSmallestScaleFunctor());
  }
}

void PictureLayerTilingSet::UpdateTilingsToCurrentRasterSourceForActivation(
    scoped_refptr<RasterSource> raster_source,
    const PictureLayerTilingSet* pending_twin_set,
    const Region& layer_invalidation,
    float minimum_contents_scale,
    float maximum_contents_scale) {
  RemoveTilingsBelowScaleKey(minimum_contents_scale);
  RemoveTilingsAboveScaleKey(maximum_contents_scale);

  raster_source_ = raster_source;

  // Copy over tilings that are shared with the |pending_twin_set| tiling set.
  // Also, copy all of the properties from twin tilings.
  CopyTilingsAndPropertiesFromPendingTwin(pending_twin_set, raster_source,
                                          layer_invalidation);

  // If the tiling is not shared (FindTilingWithScale returns nullptr), then
  // invalidate tiles and update them to the new raster source.
  for (const auto& tiling : tilings_) {
    if (pending_twin_set->FindTilingWithScaleKey(tiling->contents_scale_key()))
      continue;

    tiling->SetRasterSourceAndResize(raster_source);
    tiling->Invalidate(layer_invalidation);
    state_since_last_tile_priority_update_.invalidated = true;
    // This is needed for cases where the live tiles rect didn't change but
    // recordings exist in the raster source that did not exist on the last
    // raster source.
    tiling->CreateMissingTilesInLiveTilesRect();

    // |this| is active set and |tiling| is not in the pending set, which means
    // it is now NON_IDEAL_RESOLUTION. The exception is for LOW_RESOLUTION
    // tilings, which are computed and created entirely on the active tree.
    // Since the pending tree does not have them, we should just leave them as
    // low resolution to not lose them.
    if (tiling->resolution() != LOW_RESOLUTION)
      tiling->set_resolution(NON_IDEAL_RESOLUTION);
  }

  VerifyTilings(pending_twin_set);
}

void PictureLayerTilingSet::UpdateTilingsToCurrentRasterSourceForCommit(
    scoped_refptr<RasterSource> raster_source,
    const Region& layer_invalidation,
    float minimum_contents_scale,
    float maximum_contents_scale) {
  RemoveTilingsBelowScaleKey(minimum_contents_scale);
  RemoveTilingsAboveScaleKey(maximum_contents_scale);

  raster_source_ = raster_source;

  // Invalidate tiles and update them to the new raster source.
  for (const std::unique_ptr<PictureLayerTiling>& tiling : tilings_) {
    DCHECK(tree_ != PENDING_TREE || !tiling->has_tiles());
    tiling->SetRasterSourceAndResize(raster_source);

    // Force |UpdateTilePriorities| on commit for cases where the compositor is
    // heavily pipelined resulting in back to back draw and commit. This
    // prevents the early out from |UpdateTilePriorities| because frame time
    // didn't change. That in turn causes an early out from PrepareTiles which
    // can cause checkerboarding.
    state_since_last_tile_priority_update_.invalidated = true;

    // We can commit on either active or pending trees, but only active one can
    // have tiles at this point.
    if (tree_ == ACTIVE_TREE)
      tiling->Invalidate(layer_invalidation);

    // This is needed for cases where the live tiles rect didn't change but
    // recordings exist in the raster source that did not exist on the last
    // raster source.
    tiling->CreateMissingTilesInLiveTilesRect();
  }
  VerifyTilings(nullptr /* pending_twin_set */);
}

void PictureLayerTilingSet::Invalidate(const Region& layer_invalidation) {
  for (const auto& tiling : tilings_) {
    tiling->Invalidate(layer_invalidation);
    tiling->CreateMissingTilesInLiveTilesRect();
  }
  state_since_last_tile_priority_update_.invalidated = true;
}

void PictureLayerTilingSet::VerifyTilings(
    const PictureLayerTilingSet* pending_twin_set) const {
#if DCHECK_IS_ON()
  for (const auto& tiling : tilings_) {
    DCHECK(tiling->tile_size() ==
           client_->CalculateTileSize(tiling->tiling_size()))
        << "tile_size: " << tiling->tile_size().ToString()
        << " tiling_size: " << tiling->tiling_size().ToString()
        << " CalculateTileSize: "
        << client_->CalculateTileSize(tiling->tiling_size()).ToString();
  }

  if (!tilings_.empty()) {
    DCHECK_LE(NumHighResTilings(), 1);
    // When commiting from the main thread the high res tiling may get dropped,
    // but when cloning to the active tree, there should always be one.
    if (pending_twin_set) {
      DCHECK_EQ(1, NumHighResTilings())
          << " num tilings on active: " << tilings_.size()
          << " num tilings on pending: " << pending_twin_set->tilings_.size()
          << " num high res on pending: "
          << pending_twin_set->NumHighResTilings()
          << " are on active tree: " << (tree_ == ACTIVE_TREE);
    }
  }
#endif
}

void PictureLayerTilingSet::CleanUpTilings(
    float min_acceptable_high_res_scale_key,
    float max_acceptable_high_res_scale_key,
    const std::vector<PictureLayerTiling*>& needed_tilings,
    PictureLayerTilingSet* twin_set) {
  std::vector<PictureLayerTiling*> to_remove;
  for (const auto& tiling : tilings_) {
    // Keep all tilings within the min/max scales.
    if (tiling->contents_scale_key() >= min_acceptable_high_res_scale_key &&
        tiling->contents_scale_key() <= max_acceptable_high_res_scale_key) {
      continue;
    }

    // Keep low resolution tilings.
    if (tiling->resolution() == LOW_RESOLUTION)
      continue;

    // Don't remove tilings that are required.
    if (base::Contains(needed_tilings, tiling.get())) {
      continue;
    }

    to_remove.push_back(tiling.get());
  }

  for (auto* tiling : to_remove) {
    DCHECK_NE(HIGH_RESOLUTION, tiling->resolution());
    Remove(tiling);
  }
}

void PictureLayerTilingSet::RemoveNonIdealTilings() {
  base::EraseIf(tilings_, [](const std::unique_ptr<PictureLayerTiling>& t) {
    return t->resolution() == NON_IDEAL_RESOLUTION;
  });
}

void PictureLayerTilingSet::MarkAllTilingsNonIdeal() {
  for (const auto& tiling : tilings_)
    tiling->set_resolution(NON_IDEAL_RESOLUTION);
}

PictureLayerTiling* PictureLayerTilingSet::AddTiling(
    const gfx::AxisTransform2d& raster_transform,
    scoped_refptr<RasterSource> raster_source) {
  if (!raster_source_)
    raster_source_ = raster_source;

#if DCHECK_IS_ON()
  for (size_t i = 0; i < tilings_.size(); ++i) {
    DCHECK_NE(tilings_[i]->contents_scale_key(), raster_transform.scale());
    DCHECK_EQ(tilings_[i]->raster_source(), raster_source.get());
  }
#endif  // DCHECK_IS_ON()

  tilings_.push_back(std::make_unique<PictureLayerTiling>(
      tree_, raster_transform, raster_source, client_,
      kMaxSoonBorderDistanceInScreenPixels, max_preraster_distance_));
  PictureLayerTiling* appended = tilings_.back().get();
  state_since_last_tile_priority_update_.added_tilings = true;

  std::sort(tilings_.begin(), tilings_.end(), LargestToSmallestScaleFunctor());
  return appended;
}

int PictureLayerTilingSet::NumHighResTilings() const {
  return std::count_if(tilings_.begin(), tilings_.end(),
                       [](const std::unique_ptr<PictureLayerTiling>& tiling) {
                         return tiling->resolution() == HIGH_RESOLUTION;
                       });
}

PictureLayerTiling* PictureLayerTilingSet::FindTilingWithScaleKey(
    float scale_key) const {
  for (size_t i = 0; i < tilings_.size(); ++i) {
    if (tilings_[i]->contents_scale_key() == scale_key)
      return tilings_[i].get();
  }
  return nullptr;
}

PictureLayerTiling* PictureLayerTilingSet::FindTilingWithResolution(
    TileResolution resolution) const {
  auto iter = std::find_if(
      tilings_.begin(), tilings_.end(),
      [resolution](const std::unique_ptr<PictureLayerTiling>& tiling) {
        return tiling->resolution() == resolution;
      });
  if (iter == tilings_.end())
    return nullptr;
  return iter->get();
}

void PictureLayerTilingSet::RemoveTilingsBelowScaleKey(
    float minimum_scale_key) {
  base::EraseIf(
      tilings_,
      [minimum_scale_key](const std::unique_ptr<PictureLayerTiling>& tiling) {
        return tiling->contents_scale_key() < minimum_scale_key;
      });
}

void PictureLayerTilingSet::RemoveTilingsAboveScaleKey(
    float maximum_scale_key) {
  base::EraseIf(
      tilings_,
      [maximum_scale_key](const std::unique_ptr<PictureLayerTiling>& tiling) {
        return tiling->contents_scale_key() > maximum_scale_key;
      });
}

void PictureLayerTilingSet::ReleaseAllResources() {
  RemoveAllTilings();
  raster_source_ = nullptr;
}

void PictureLayerTilingSet::RemoveAllTilings() {
  tilings_.clear();
}

void PictureLayerTilingSet::Remove(PictureLayerTiling* tiling) {
  auto iter = std::find_if(
      tilings_.begin(), tilings_.end(),
      [tiling](const std::unique_ptr<PictureLayerTiling>& candidate) {
        return candidate.get() == tiling;
      });
  if (iter == tilings_.end())
    return;
  tilings_.erase(iter);
}

void PictureLayerTilingSet::RemoveAllTiles() {
  for (size_t i = 0; i < tilings_.size(); ++i)
    tilings_[i]->Reset();
}

float PictureLayerTilingSet::GetSnappedContentsScaleKey(
    float start_scale,
    float snap_to_existing_tiling_ratio) const {
  // If a tiling exists within the max snapping ratio, snap to its scale.
  float snapped_contents_scale = start_scale;
  float snapped_ratio = snap_to_existing_tiling_ratio;
  for (const auto& tiling : tilings_) {
    float tiling_contents_scale = tiling->contents_scale_key();
    float ratio = LargerRatio(tiling_contents_scale, start_scale);
    if (ratio < snapped_ratio) {
      snapped_contents_scale = tiling_contents_scale;
      snapped_ratio = ratio;
    }
  }
  return snapped_contents_scale;
}

float PictureLayerTilingSet::GetMaximumContentsScale() const {
  if (tilings_.empty())
    return 0.f;
  // The first tiling has the largest contents scale.
  return tilings_[0]->raster_transform().scale();
}

bool PictureLayerTilingSet::TilingsNeedUpdate(
    const gfx::Rect& visible_rect_in_layer_space,
    double current_frame_time_in_seconds) {
  // If we don't have any tilings, we don't need an update.
  if (num_tilings() == 0)
    return false;

  // If we never updated the tiling set, then our history is empty. We should
  // update tilings.
  if (visible_rect_history_.empty())
    return true;

  // If we've added new tilings since the last update, then we have to update at
  // least that one tiling.
  if (state_since_last_tile_priority_update_.added_tilings)
    return true;

  // Finally, if some state changed (either frame time or visible rect), then we
  // need to inform the tilings of the change.
  const auto& last_frame = visible_rect_history_.front();
  if (current_frame_time_in_seconds != last_frame.frame_time_in_seconds)
    return true;

  if (visible_rect_in_layer_space != last_frame.visible_rect_in_layer_space)
    return true;
  return false;
}

gfx::Rect PictureLayerTilingSet::ComputeSkewport(
    const gfx::Rect& visible_rect_in_layer_space,
    double current_frame_time_in_seconds,
    float ideal_contents_scale) {
  gfx::Rect skewport = visible_rect_in_layer_space;
  if (skewport.IsEmpty() || visible_rect_history_.empty())
    return skewport;

  // Use the oldest recorded history to get a stable skewport.
  const auto& historical_frame = visible_rect_history_.back();
  double time_delta =
      current_frame_time_in_seconds - historical_frame.frame_time_in_seconds;
  if (time_delta == 0.)
    return skewport;

  double extrapolation_multiplier =
      skewport_target_time_in_seconds_ / time_delta;
  int old_x = historical_frame.visible_rect_in_layer_space.x();
  int old_y = historical_frame.visible_rect_in_layer_space.y();
  int old_right = historical_frame.visible_rect_in_layer_space.right();
  int old_bottom = historical_frame.visible_rect_in_layer_space.bottom();

  int new_x = visible_rect_in_layer_space.x();
  int new_y = visible_rect_in_layer_space.y();
  int new_right = visible_rect_in_layer_space.right();
  int new_bottom = visible_rect_in_layer_space.bottom();

  int inset_x = (new_x - old_x) * extrapolation_multiplier;
  int inset_y = (new_y - old_y) * extrapolation_multiplier;
  int inset_right = (old_right - new_right) * extrapolation_multiplier;
  int inset_bottom = (old_bottom - new_bottom) * extrapolation_multiplier;

  int skewport_extrapolation_limit_in_layer_pixels =
      skewport_extrapolation_limit_in_screen_pixels_ / ideal_contents_scale;
  gfx::Rect max_skewport = skewport;
  max_skewport.Inset(-skewport_extrapolation_limit_in_layer_pixels,
                     -skewport_extrapolation_limit_in_layer_pixels);

  skewport.Inset(inset_x, inset_y, inset_right, inset_bottom);
  skewport.Union(visible_rect_in_layer_space);
  skewport.Intersect(max_skewport);

  // Due to limits in int's representation, it is possible that the two
  // operations above (union and intersect) result in an empty skewport. To
  // avoid any unpleasant situations like that, union the visible rect again to
  // ensure that skewport.Contains(visible_rect_in_layer_space) is always
  // true.
  skewport.Union(visible_rect_in_layer_space);
  skewport.Intersect(eventually_rect_in_layer_space_);
  return skewport;
}

gfx::Rect PictureLayerTilingSet::ComputeSoonBorderRect(
    const gfx::Rect& visible_rect,
    float ideal_contents_scale) {
  int max_dimension = std::max(visible_rect.width(), visible_rect.height());
  int distance =
      std::min<int>(kMaxSoonBorderDistanceInScreenPixels * ideal_contents_scale,
                    max_dimension * kSoonBorderDistanceViewportPercentage);

  gfx::Rect soon_border_rect = visible_rect;
  soon_border_rect.Inset(-distance, -distance);
  soon_border_rect.Intersect(eventually_rect_in_layer_space_);
  return soon_border_rect;
}

void PictureLayerTilingSet::UpdatePriorityRects(
    const gfx::Rect& visible_rect_in_layer_space,
    double current_frame_time_in_seconds,
    float ideal_contents_scale) {
  visible_rect_in_layer_space_ = gfx::Rect();
  eventually_rect_in_layer_space_ = gfx::Rect();

  // We keep things as floats in here.
  if (!visible_rect_in_layer_space.IsEmpty()) {
    gfx::RectF eventually_rectf(visible_rect_in_layer_space);
    eventually_rectf.Inset(
        -tiling_interest_area_padding_ / ideal_contents_scale,
        -tiling_interest_area_padding_ / ideal_contents_scale);
    if (eventually_rectf.Intersects(
            gfx::RectF(gfx::SizeF(raster_source_->GetSize())))) {
      visible_rect_in_layer_space_ = visible_rect_in_layer_space;
      eventually_rect_in_layer_space_ = gfx::ToEnclosingRect(eventually_rectf);
    }
  }

  skewport_in_layer_space_ =
      ComputeSkewport(visible_rect_in_layer_space_,
                      current_frame_time_in_seconds, ideal_contents_scale);
  DCHECK(skewport_in_layer_space_.Contains(visible_rect_in_layer_space_));
  DCHECK(eventually_rect_in_layer_space_.Contains(skewport_in_layer_space_));

  soon_border_rect_in_layer_space_ =
      ComputeSoonBorderRect(visible_rect_in_layer_space_, ideal_contents_scale);
  DCHECK(
      soon_border_rect_in_layer_space_.Contains(visible_rect_in_layer_space_));
  DCHECK(eventually_rect_in_layer_space_.Contains(
      soon_border_rect_in_layer_space_));

  // Finally, update our visible rect history. Note that we use the original
  // visible rect here, since we want as accurate of a history as possible for
  // stable skewports.
  if (visible_rect_history_.size() == 2)
    visible_rect_history_.pop_back();
  visible_rect_history_.push_front(FrameVisibleRect(
      visible_rect_in_layer_space_, current_frame_time_in_seconds));
}

bool PictureLayerTilingSet::UpdateTilePriorities(
    const gfx::Rect& visible_rect_in_layer_space,
    float ideal_contents_scale,
    double current_frame_time_in_seconds,
    const Occlusion& occlusion_in_layer_space,
    bool can_require_tiles_for_activation) {
  StateSinceLastTilePriorityUpdate::AutoClear auto_clear_state(
      &state_since_last_tile_priority_update_);

  if (!TilingsNeedUpdate(visible_rect_in_layer_space,
                         current_frame_time_in_seconds)) {
    return state_since_last_tile_priority_update_.invalidated;
  }

  UpdatePriorityRects(visible_rect_in_layer_space,
                      current_frame_time_in_seconds, ideal_contents_scale);

  for (const auto& tiling : tilings_) {
    tiling->set_can_require_tiles_for_activation(
        can_require_tiles_for_activation);
    tiling->ComputeTilePriorityRects(
        visible_rect_in_layer_space_, skewport_in_layer_space_,
        soon_border_rect_in_layer_space_, eventually_rect_in_layer_space_,
        ideal_contents_scale, occlusion_in_layer_space);
  }
  return true;
}

void PictureLayerTilingSet::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
  for (const auto& tiling : tilings_)
    tiling->GetAllPrioritizedTilesForTracing(prioritized_tiles);
}

PictureLayerTilingSet::CoverageIterator::CoverageIterator(
    const PictureLayerTilingSet* set,
    float coverage_scale,
    const gfx::Rect& coverage_rect,
    float ideal_contents_scale)
    : set_(set),
      coverage_scale_(coverage_scale),
      current_tiling_(std::numeric_limits<size_t>::max()) {
  missing_region_.Union(coverage_rect);

  // Determine the smallest content_scale tiling which a scale higher than the
  // ideal (or the first tiling if all tilings have a scale less than ideal).
  size_t tilings_size = set_->tilings_.size();
  for (ideal_tiling_ = 0; ideal_tiling_ < tilings_size; ++ideal_tiling_) {
    PictureLayerTiling* tiling = set_->tilings_[ideal_tiling_].get();
    if (tiling->contents_scale_key() < ideal_contents_scale) {
      if (ideal_tiling_ > 0)
        ideal_tiling_--;
      break;
    }
  }

  // If all tilings have a scale larger than the ideal, then use the smallest
  // scale (which is the last one).
  if (ideal_tiling_ == tilings_size && ideal_tiling_ > 0)
    ideal_tiling_--;

  ++(*this);
}

PictureLayerTilingSet::CoverageIterator::~CoverageIterator() = default;

gfx::Rect PictureLayerTilingSet::CoverageIterator::geometry_rect() const {
  // If we don't have any more tilings to process, then return the region
  // iterator rect that we need to fill, so that the caller can checkerboard it.
  if (!tiling_iter_) {
    if (region_iter_ == current_region_.end())
      return gfx::Rect();
    return *region_iter_;
  }
  return tiling_iter_.geometry_rect();
}

gfx::RectF PictureLayerTilingSet::CoverageIterator::texture_rect() const {
  // Texture rects are only valid if we have a tiling.
  if (!tiling_iter_)
    return gfx::RectF();
  return tiling_iter_.texture_rect();
}

Tile* PictureLayerTilingSet::CoverageIterator::operator->() const {
  if (!tiling_iter_)
    return nullptr;
  return *tiling_iter_;
}

Tile* PictureLayerTilingSet::CoverageIterator::operator*() const {
  if (!tiling_iter_)
    return nullptr;
  return *tiling_iter_;
}

TileResolution PictureLayerTilingSet::CoverageIterator::resolution() const {
  const PictureLayerTiling* tiling = CurrentTiling();
  DCHECK(tiling);
  return tiling->resolution();
}

PictureLayerTiling* PictureLayerTilingSet::CoverageIterator::CurrentTiling()
    const {
  if (current_tiling_ == std::numeric_limits<size_t>::max())
    return nullptr;
  if (current_tiling_ >= set_->tilings_.size())
    return nullptr;
  return set_->tilings_[current_tiling_].get();
}

size_t PictureLayerTilingSet::CoverageIterator::NextTiling() const {
  // Order returned by this method is:
  // 1. Ideal tiling index
  // 2. Tiling index < Ideal in decreasing order (higher res than ideal)
  // 3. Tiling index > Ideal in increasing order (lower res than ideal)
  // 4. Tiling index > tilings.size() (invalid index)
  if (current_tiling_ == std::numeric_limits<size_t>::max())
    return ideal_tiling_;
  else if (current_tiling_ > ideal_tiling_)
    return current_tiling_ + 1;
  else if (current_tiling_)
    return current_tiling_ - 1;
  else
    return ideal_tiling_ + 1;
}

PictureLayerTilingSet::CoverageIterator&
PictureLayerTilingSet::CoverageIterator::operator++() {
  bool first_time = current_tiling_ == std::numeric_limits<size_t>::max();

  if (!*this && !first_time)
    return *this;

  if (tiling_iter_)
    ++tiling_iter_;

  // Loop until we find a valid place to stop.
  while (true) {
    // While we don't have a ready to draw tile, accumulate the geometry rects
    // back into the missing region, which will be iterated after this tiling is
    // processed.
    while (tiling_iter_ &&
           (!*tiling_iter_ || !tiling_iter_->draw_info().IsReadyToDraw())) {
      missing_region_.Union(tiling_iter_.geometry_rect());
      ++tiling_iter_;
    }
    // We found a ready tile, yield it!
    if (tiling_iter_)
      return *this;

    // If the set of current rects for this tiling is done, go to the next
    // tiling and set up to iterate through all of the remaining holes.
    // This will also happen the first time through the loop.
    if (region_iter_ == current_region_.end()) {
      current_tiling_ = NextTiling();
      current_region_.Swap(&missing_region_);
      missing_region_.Clear();
      region_iter_ = current_region_.begin();

      // All done and all filled.
      if (region_iter_ == current_region_.end()) {
        current_tiling_ = set_->tilings_.size();
        return *this;
      }

      // No more valid tiles, return this checkerboard rect.
      if (current_tiling_ >= set_->tilings_.size())
        return *this;
    }

    // Pop a rect off.  If there are no more tilings, then these will be
    // treated as geometry with null tiles that the caller can checkerboard.
    gfx::Rect last_rect = *region_iter_;
    ++region_iter_;

    // Done, found next checkerboard rect to return.
    if (current_tiling_ >= set_->tilings_.size())
      return *this;

    // Construct a new iterator for the next tiling, but we need to loop
    // again until we get to a valid one.
    tiling_iter_ = PictureLayerTiling::CoverageIterator(
        set_->tilings_[current_tiling_].get(), coverage_scale_, last_rect);
  }

  return *this;
}

PictureLayerTilingSet::CoverageIterator::operator bool() const {
  return current_tiling_ < set_->tilings_.size() ||
         region_iter_ != current_region_.end();
}

void PictureLayerTilingSet::AsValueInto(
    base::trace_event::TracedValue* state) const {
  for (size_t i = 0; i < tilings_.size(); ++i) {
    state->BeginDictionary();
    tilings_[i]->AsValueInto(state);
    state->EndDictionary();
  }
}

size_t PictureLayerTilingSet::GPUMemoryUsageInBytes() const {
  size_t amount = 0;
  for (size_t i = 0; i < tilings_.size(); ++i)
    amount += tilings_[i]->GPUMemoryUsageInBytes();
  return amount;
}

PictureLayerTilingSet::TilingRange PictureLayerTilingSet::GetTilingRange(
    TilingRangeType type) const {
  // Doesn't seem to be the case right now but if it ever becomes a performance
  // problem to compute these ranges each time this function is called, we can
  // compute them only when the tiling set has changed instead.
  size_t tilings_size = tilings_.size();
  TilingRange high_res_range(0, 0);
  TilingRange low_res_range(tilings_.size(), tilings_.size());
  for (size_t i = 0; i < tilings_size; ++i) {
    const PictureLayerTiling* tiling = tilings_[i].get();
    if (tiling->resolution() == HIGH_RESOLUTION)
      high_res_range = TilingRange(i, i + 1);
    if (tiling->resolution() == LOW_RESOLUTION)
      low_res_range = TilingRange(i, i + 1);
  }

  TilingRange range(0, 0);
  switch (type) {
    case HIGHER_THAN_HIGH_RES:
      range = TilingRange(0, high_res_range.start);
      break;
    case HIGH_RES:
      range = high_res_range;
      break;
    case BETWEEN_HIGH_AND_LOW_RES:
      // TODO(vmpstr): This code assumes that high res tiling will come before
      // low res tiling, however there are cases where this assumption is
      // violated. As a result, it's better to be safe in these situations,
      // since otherwise we can end up accessing a tiling that doesn't exist.
      // See crbug.com/429397 for high res tiling appearing after low res
      // tiling discussion/fixes.
      if (high_res_range.start <= low_res_range.start)
        range = TilingRange(high_res_range.end, low_res_range.start);
      else
        range = TilingRange(low_res_range.end, high_res_range.start);
      break;
    case LOW_RES:
      range = low_res_range;
      break;
    case LOWER_THAN_LOW_RES:
      range = TilingRange(low_res_range.end, tilings_size);
      break;
  }

  DCHECK_LE(range.start, range.end);
  return range;
}

}  // namespace cc
