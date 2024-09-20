// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/picture_layer_tiling_set.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
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
    bool can_use_lcd_text = pending_twin_tiling->can_use_lcd_text();
    PictureLayerTiling* this_tiling =
        FindTilingWithScaleKey(pending_twin_tiling->contents_scale_key());
    if (this_tiling && (this_tiling->raster_transform() != raster_transform ||
                        this_tiling->can_use_lcd_text() != can_use_lcd_text)) {
      Remove(this_tiling);
      this_tiling = nullptr;
    }
    if (!this_tiling) {
      std::unique_ptr<PictureLayerTiling> new_tiling(
          new PictureLayerTiling(tree_, raster_transform, raster_source_,
                                 client_, kMaxSoonBorderDistanceInScreenPixels,
                                 max_preraster_distance_, can_use_lcd_text));
      tilings_.push_back(std::move(new_tiling));
      this_tiling = tilings_.back().get();
      tiling_sort_required = true;
      state_since_last_tile_priority_update_.added_tilings = true;
    }
    this_tiling->TakeTilesAndPropertiesFrom(pending_twin_tiling.get(),
                                            layer_invalidation);
    all_tiles_done_ &= this_tiling->all_tiles_done();
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

    all_tiles_done_ &= tiling->all_tiles_done();
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
  all_tiles_done_ = true;
  for (const auto& tiling : tilings_) {
    DCHECK(tree_ != PENDING_TREE || !tiling->has_tiles());
    // Force |UpdateTilePriorities| on commit for cases when tiling needs update
    state_since_last_tile_priority_update_.tiling_needs_update |=
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

    all_tiles_done_ &= tiling->all_tiles_done();
  }
  VerifyTilings(nullptr /* pending_twin_set */);
}

void PictureLayerTilingSet::Invalidate(const Region& layer_invalidation) {
  all_tiles_done_ = true;
  for (const auto& tiling : tilings_) {
    tiling->Invalidate(layer_invalidation);
    tiling->CreateMissingTilesInLiveTilesRect();
    all_tiles_done_ &= tiling->all_tiles_done();
  }
  state_since_last_tile_priority_update_.invalidated = true;
}

void PictureLayerTilingSet::VerifyTilings(
    const PictureLayerTilingSet* pending_twin_set) const {
#if DCHECK_IS_ON()
  for (const auto& tiling : tilings_) {
    DCHECK(tiling->tile_size() ==
           client_->CalculateTileSize(tiling->tiling_rect().size()))
        << "tile_size: " << tiling->tile_size().ToString()
        << " tiling_size: " << tiling->tiling_rect().ToString()
        << " CalculateTileSize: "
        << client_->CalculateTileSize(tiling->tiling_rect().size()).ToString();
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
    const std::vector<raw_ptr<PictureLayerTiling, VectorExperimental>>&
        needed_tilings,
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
  std::erase_if(tilings_, [](const std::unique_ptr<PictureLayerTiling>& t) {
    return t->resolution() == NON_IDEAL_RESOLUTION;
  });
}

void PictureLayerTilingSet::MarkAllTilingsNonIdeal() {
  for (const auto& tiling : tilings_)
    tiling->set_resolution(NON_IDEAL_RESOLUTION);
}

PictureLayerTiling* PictureLayerTilingSet::AddTiling(
    const gfx::AxisTransform2d& raster_transform,
    scoped_refptr<RasterSource> raster_source,
    bool can_use_lcd_text) {
  if (!raster_source_)
    raster_source_ = raster_source;

#if DCHECK_IS_ON()
  for (const auto& tiling : tilings_) {
    const gfx::Vector2dF& scale = raster_transform.scale();
    DCHECK_NE(tiling->contents_scale_key(), std::max(scale.x(), scale.y()));
    DCHECK_EQ(tiling->raster_source(), raster_source.get());
  }
#endif  // DCHECK_IS_ON()

  tilings_.push_back(std::make_unique<PictureLayerTiling>(
      tree_, raster_transform, raster_source, client_,
      kMaxSoonBorderDistanceInScreenPixels, max_preraster_distance_,
      can_use_lcd_text));
  PictureLayerTiling* appended = tilings_.back().get();
  state_since_last_tile_priority_update_.added_tilings = true;

  std::sort(tilings_.begin(), tilings_.end(), LargestToSmallestScaleFunctor());
  return appended;
}

int PictureLayerTilingSet::NumHighResTilings() const {
  return base::ranges::count(tilings_, HIGH_RESOLUTION,
                             &PictureLayerTiling::resolution);
}

PictureLayerTiling* PictureLayerTilingSet::FindTilingWithScaleKey(
    float scale_key) const {
  for (const auto& tiling : tilings_) {
    if (tiling->contents_scale_key() == scale_key)
      return tiling.get();
  }
  return nullptr;
}

PictureLayerTiling* PictureLayerTilingSet::FindTilingWithResolution(
    TileResolution resolution) const {
  auto iter =
      base::ranges::find(tilings_, resolution, &PictureLayerTiling::resolution);
  if (iter == tilings_.end())
    return nullptr;
  return iter->get();
}

PictureLayerTiling* PictureLayerTilingSet::FindTilingWithNearestScaleKey(
    float start_scale,
    float snap_to_existing_tiling_ratio) const {
  PictureLayerTiling* nearest_tiling = nullptr;
  float nearest_ratio = snap_to_existing_tiling_ratio;
  for (const auto& tiling : tilings_) {
    float tiling_contents_scale = tiling->contents_scale_key();
    float ratio = LargerRatio(tiling_contents_scale, start_scale);
    if (ratio <= nearest_ratio) {
      nearest_tiling = tiling.get();
      nearest_ratio = ratio;
    }
  }
  return nearest_tiling;
}

void PictureLayerTilingSet::RemoveTilingsBelowScaleKey(
    float minimum_scale_key) {
  std::erase_if(
      tilings_,
      [minimum_scale_key](const std::unique_ptr<PictureLayerTiling>& tiling) {
        return tiling->contents_scale_key() < minimum_scale_key;
      });
}

void PictureLayerTilingSet::RemoveTilingsAboveScaleKey(
    float maximum_scale_key) {
  std::erase_if(
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
  all_tiles_done_ = true;
}

void PictureLayerTilingSet::Remove(PictureLayerTiling* tiling) {
  auto iter = base::ranges::find(tilings_, tiling,
                                 &std::unique_ptr<PictureLayerTiling>::get);
  if (iter == tilings_.end())
    return;
  tilings_.erase(iter);
}

void PictureLayerTilingSet::RemoveAllTiles() {
  for (const auto& tiling : tilings_)
    tiling->Reset();
  all_tiles_done_ = true;
}

float PictureLayerTilingSet::GetMaximumContentsScale() const {
  if (tilings_.empty())
    return 0.f;
  // The first tiling has the largest contents scale.
  return tilings_[0]->contents_scale_key();
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

  if (state_since_last_tile_priority_update_.tiling_needs_update) {
    return true;
  }

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
  max_skewport.Inset(-skewport_extrapolation_limit_in_layer_pixels);

  skewport.Inset(
      gfx::Insets::TLBR(inset_y, inset_x, inset_bottom, inset_right));
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
  soon_border_rect.Inset(-distance);
  soon_border_rect.Intersect(eventually_rect_in_layer_space_);
  return soon_border_rect;
}

void PictureLayerTilingSet::UpdatePriorityRects(
    const gfx::Rect& visible_rect_in_layer_space,
    double current_frame_time_in_seconds,
    float ideal_contents_scale) {
  bool has_visible_rects = false;
  if (!visible_rect_in_layer_space.IsEmpty()) {
    gfx::RectF eventually_rectf(visible_rect_in_layer_space);
    eventually_rectf.Inset(-tiling_interest_area_padding_ /
                           ideal_contents_scale);
    if (eventually_rectf.Intersects(
            gfx::RectF(raster_source_->recorded_bounds()))) {
      visible_rect_in_layer_space_ = visible_rect_in_layer_space;
      eventually_rect_in_layer_space_ = gfx::ToEnclosingRect(eventually_rectf);
      has_visible_rects = true;
    }
  }

  if (!has_visible_rects) {
    visible_rect_in_layer_space_ = gfx::Rect();
    eventually_rect_in_layer_space_ = gfx::Rect();
    skewport_rect_in_layer_space_ = gfx::Rect();
    soon_border_rect_in_layer_space_ = gfx::Rect();
    // If we have no visible rect, clear all interest rects.
    visible_rect_history_.clear();
    return;
  }

  skewport_rect_in_layer_space_ =
      ComputeSkewport(visible_rect_in_layer_space_,
                      current_frame_time_in_seconds, ideal_contents_scale);
  DCHECK(skewport_rect_in_layer_space_.Contains(visible_rect_in_layer_space_));
  DCHECK(
      eventually_rect_in_layer_space_.Contains(skewport_rect_in_layer_space_));

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

  all_tiles_done_ = true;
  for (const auto& tiling : tilings_) {
    tiling->set_can_require_tiles_for_activation(
        can_require_tiles_for_activation);
    tiling->ComputeTilePriorityRects(
        visible_rect_in_layer_space_, skewport_rect_in_layer_space_,
        soon_border_rect_in_layer_space_, eventually_rect_in_layer_space_,
        ideal_contents_scale, occlusion_in_layer_space);
    all_tiles_done_ &= tiling->all_tiles_done();
  }
  return true;
}

void PictureLayerTilingSet::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
  for (const auto& tiling : tilings_)
    tiling->GetAllPrioritizedTilesForTracing(prioritized_tiles);
}

PictureLayerTilingSet::CoverageIterator PictureLayerTilingSet::Cover(
    const gfx::Rect& coverage_rect,
    float coverage_scale,
    float ideal_contents_scale) {
  return CoverageIterator(tilings_, coverage_rect, coverage_scale,
                          ideal_contents_scale);
}

void PictureLayerTilingSet::AsValueInto(
    base::trace_event::TracedValue* state) const {
  for (const auto& tiling : tilings_) {
    state->BeginDictionary();
    tiling->AsValueInto(state);
    state->EndDictionary();
  }
}

size_t PictureLayerTilingSet::GPUMemoryUsageInBytes() const {
  size_t amount = 0;
  for (const auto& tiling : tilings_)
    amount += tiling->GPUMemoryUsageInBytes();
  return amount;
}

PictureLayerTilingSet::TilingRange PictureLayerTilingSet::GetTilingRange(
    TilingRangeType type) const {
  // Doesn't seem to be the case right now but if it ever becomes a performance
  // problem to compute these ranges each time this function is called, we can
  // compute them only when the tiling set has changed instead.
  size_t tilings_size = tilings_.size();
  TilingRange high_res_range(0, 0);
  TilingRange low_res_range(tilings_size, tilings_size);
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
