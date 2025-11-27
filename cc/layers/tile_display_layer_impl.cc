// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_display_layer_impl.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "cc/base/math_util.h"
#include "cc/layers/append_quads_data.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"

namespace cc {

namespace {

class TilingOrder {
 public:
  bool operator()(const std::unique_ptr<TileDisplayLayerTiling>& left,
                  const std::unique_ptr<TileDisplayLayerTiling>& right) {
    return left->contents_scale_key() > right->contents_scale_key();
  }
};

}  // namespace

TileDisplayLayerTile::TileDisplayLayerTile(
    TileDisplayLayerImpl& layer,
    const TileDisplayLayerTileContents& contents)
    : layer_(layer), contents_(contents) {}

TileDisplayLayerTile::TileDisplayLayerTile(TileDisplayLayerTile&&) = default;

TileDisplayLayerTile::~TileDisplayLayerTile() {
  if (auto* resource = std::get_if<TileDisplayLayerTileResource>(&contents_)) {
    layer_->DiscardResource(resource->resource_id);
  }
}

TileDisplayLayerTiling::TileDisplayLayerTiling(TileDisplayLayerImpl& layer,
                                               float scale_key)
    : layer_(layer), scale_key_(scale_key) {}

TileDisplayLayerTiling::~TileDisplayLayerTiling() = default;

TileDisplayLayerTile* TileDisplayLayerTiling::TileAt(
    const TileIndex& index) const {
  auto it = tiles_.find(index);
  if (it == tiles_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void TileDisplayLayerTiling::SetRasterTransform(
    const gfx::AxisTransform2d& transform) {
  DCHECK_EQ(std::max(transform.scale().x(), transform.scale().y()), scale_key_);
  raster_transform_ = transform;
}

void TileDisplayLayerTiling::SetTileSize(const gfx::Size& size) {
  if (size == tiling_data_.max_texture_size()) {
    return;
  }

  tiling_data_.SetMaxTextureSize(size);
}

void TileDisplayLayerTiling::SetTilingRect(const gfx::Rect& rect) {
  if (rect == tiling_data_.tiling_rect()) {
    return;
  }

  tiling_data_.SetTilingRect(rect);
}

void TileDisplayLayerTiling::SetTileContents(
    const TileIndex& key,
    const TileDisplayLayerTileContents& contents,
    bool update_damage) {
  if (update_damage) {
    // Full tree updates receive damage as part of the LayerImpl::update_rect.
    // For incremental tile updates on an Active tree, we need to record the
    // damage caused by each tile change.
    gfx::Rect tile_rect = tiling_data_.TileBoundsWithBorder(key.i, key.j);
    tile_rect.set_size(tiling_data_.max_texture_size());
    gfx::Rect enclosing_layer_rect = ToEnclosingRect(
        raster_transform_.InverseMapRect(gfx::RectF(tile_rect)));
    layer_->RecordDamage(enclosing_layer_rect);
  }

  std::unique_ptr<Tile> old_tile;
  if (std::holds_alternative<TileDisplayLayerNoContents>(contents)) {
    const auto& no_contents = std::get<TileDisplayLayerNoContents>(contents);
    if (no_contents.reason == mojom::MissingTileReason::kTileDeleted) {
      tiles_.erase(key);
    } else {
      old_tile =
          std::exchange(tiles_[key], std::make_unique<Tile>(*layer_, contents));
    }
  } else {
    old_tile =
        std::exchange(tiles_[key], std::make_unique<Tile>(*layer_, contents));
  }
}

DisplayTilingCoverageIterator TileDisplayLayerTiling::Cover(
    const gfx::Rect& coverage_rect,
    float coverage_scale) const {
  return DisplayTilingCoverageIterator(this, coverage_scale, coverage_rect);
}

gfx::Size TileDisplayLayerTiling::raster_size() const {
  return layer_->bounds();
}

TileDisplayLayerImpl::TileDisplayLayerImpl(LayerTreeImpl& tree, int id)
    : TileBasedLayerImpl(&tree, id) {}

TileDisplayLayerImpl::~TileDisplayLayerImpl() = default;

TileDisplayLayerTiling& TileDisplayLayerImpl::GetOrCreateTilingFromScaleKey(
    float scale_key) {
  auto it = std::find_if(tilings_.begin(), tilings_.end(),
                         [scale_key](const auto& tiling) {
                           return tiling->contents_scale_key() == scale_key;
                         });
  if (it != tilings_.end()) {
    return **it;
  }

  tilings_.push_back(
      std::make_unique<TileDisplayLayerTiling>(*this, scale_key));
  TileDisplayLayerTiling& tiling = *tilings_.back();
  std::sort(tilings_.begin(), tilings_.end(), TilingOrder());
  return tiling;
}

void TileDisplayLayerImpl::RemoveTiling(float scale_key) {
  auto it = std::find_if(tilings_.begin(), tilings_.end(),
                         [scale_key](const auto& tiling) {
                           return tiling->contents_scale_key() == scale_key;
                         });
  if (it != tilings_.end()) {
    tilings_.erase(it);
  }
}

const TileDisplayLayerTiling* TileDisplayLayerImpl::GetTilingForTesting(
    float scale_key) const {
  auto it = std::find_if(tilings_.begin(), tilings_.end(),
                         [scale_key](const auto& tiling) {
                           return tiling->contents_scale_key() == scale_key;
                         });
  return it != tilings_.end() ? it->get() : nullptr;
}

mojom::LayerType TileDisplayLayerImpl::GetLayerType() const {
  return mojom::LayerType::kTileDisplay;
}

std::unique_ptr<LayerImpl> TileDisplayLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  NOTREACHED();
}

void TileDisplayLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  NOTREACHED();
}

void TileDisplayLayerImpl::AppendQuadsSpecialization(
    const AppendQuadsContext& context,
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data,
    viz::SharedQuadState* shared_quad_state,
    const Occlusion& scaled_occlusion,
    const gfx::Vector2d& quad_offset) {
  const float max_contents_scale = GetMaximumContentsScaleForUseInAppendQuads();

  // Keep track of the tilings that were used so that tilings that are
  // unused can be considered for removal.
  last_append_quads_scales_.clear();

  // TODO(crbug.com/40902346): Use scaled_cull_rect to set
  // append_quads_data->checkerboarded_needs_record.
  std::optional<gfx::Rect> scaled_cull_rect;
  const ScrollTree& scroll_tree =
      layer_tree_impl()->property_trees()->scroll_tree();
  if (const ScrollNode* scroll_node = scroll_tree.Node(scroll_tree_index())) {
    if (transform_tree_index() == scroll_node->transform_id) {
      if (const gfx::Rect* cull_rect =
              scroll_tree.ScrollingContentsCullRect(scroll_node->element_id)) {
        scaled_cull_rect =
            gfx::ScaleToEnclosingRect(*cull_rect, max_contents_scale);
      }
    }
  }

  const float ideal_scale_key = GetIdealContentsScaleKey();
  const gfx::Rect scaled_recorded_bounds =
      gfx::ScaleToEnclosingRect(recorded_bounds_, max_contents_scale);

  // Append quads for the tiles in this layer.
  for (auto iter = Cover(shared_quad_state->visible_quad_layer_rect,
                         max_contents_scale, ideal_scale_key);
       iter; ++iter) {
    const gfx::Rect geometry_rect = iter.geometry_rect();
    if (!scaled_recorded_bounds.Intersects(geometry_rect)) {
      // This happens when the tiling rect is snapped to be bigger than the
      // recorded bounds, and CoverageIterator returns a "missing" tile
      // to cover some of the empty area. The tile should be ignored, otherwise
      // it would be mistakenly treated as checkerboarded and drawn with the
      // safe background color.
      // TODO(crbug.com/328677988): Ideally we should check intersection with
      // visible_geometry_rect and remove the visible_geometry_rect.IsEmpty()
      // condition below.
      continue;
    }
    const gfx::Rect visible_geometry_rect =
        scaled_occlusion.GetUnoccludedContentRect(geometry_rect);
    if (visible_geometry_rect.IsEmpty()) {
      continue;
    }

    const gfx::Rect offset_geometry_rect = geometry_rect + quad_offset;
    const gfx::Rect offset_visible_geometry_rect =
        visible_geometry_rect + quad_offset;
    const bool needs_blending = !contents_opaque();

    const uint64_t visible_geometry_area =
        visible_geometry_rect.size().Area64();
    append_quads_data->visible_layer_area += visible_geometry_area;
    bool has_draw_quad = false;
    if (*iter) {
      if (auto resource = iter->resource()) {
        const gfx::RectF texture_rect = iter.texture_rect();
        auto* quad = render_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
        quad->SetNew(shared_quad_state, offset_geometry_rect,
                     offset_visible_geometry_rect, needs_blending,
                     resource->resource_id, texture_rect, nearest_neighbor_,
                     !layer_tree_impl()->settings().enable_edge_anti_aliasing);
        has_draw_quad = true;
      } else if (auto color = iter->solid_color()) {
        has_draw_quad = true;
        const float alpha = color->fA * shared_quad_state->opacity;
        if (alpha >= std::numeric_limits<float>::epsilon()) {
          auto* quad =
              render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
          quad->SetNew(
              shared_quad_state, offset_geometry_rect,
              offset_visible_geometry_rect, *color,
              !layer_tree_impl()->settings().enable_edge_anti_aliasing);
        }
      } else if (iter->is_oom()) {
        // Keep `has_draw_quad` false to end up checkerboarding below.
      }
    }
    if (!has_draw_quad) {
      // Checkerboard due to missing raster.
      SkColor4f color = safe_opaque_background_color();
      auto* quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      quad->SetNew(shared_quad_state, offset_geometry_rect,
                   offset_visible_geometry_rect, color, false);
      continue;
    }

    if (last_append_quads_scales_.empty() ||
        last_append_quads_scales_.back() !=
            iter.CurrentTiling()->contents_scale_key()) {
      last_append_quads_scales_.push_back(
          iter.CurrentTiling()->contents_scale_key());
    }
  }
}

float TileDisplayLayerImpl::GetMaximumContentsScaleForUseInAppendQuads() {
  return tilings_.empty() ? 1.0 : tilings_.front()->contents_scale_key();
}

bool TileDisplayLayerImpl::IsDirectlyCompositedImage() const {
  return is_directly_composited_image_;
}

void TileDisplayLayerImpl::GetContentsResourceId(
    viz::ResourceId* resource_id,
    gfx::Size* resource_size,
    gfx::SizeF* resource_uv_size) const {
  *resource_id = viz::kInvalidResourceId;

  // We need contents resource for backdrop filter masks only.
  if (!is_backdrop_filter_mask()) {
    return;
  }

  // Masks are only supported if they fit on exactly one tile.
  if (tilings_.size() != 1u) {
    return;
  }

  const float max_contents_scale = tilings_.front()->contents_scale_key();
  gfx::Rect content_rect =
      gfx::ScaleToEnclosingRect(gfx::Rect(bounds()), max_contents_scale);
  auto iter = TilingSetCoverageIterator<TileDisplayLayerTiling>(
      tilings_, content_rect, max_contents_scale, GetIdealContentsScaleKey());

  // We cannot do anything if the mask resource was not provided.
  if (!iter || !*iter || !iter->resource()) {
    return;
  }

  DCHECK(iter.geometry_rect() == content_rect)
      << "iter rect " << iter.geometry_rect().ToString() << " content rect "
      << content_rect.ToString();

  *resource_id = iter->resource()->resource_id;
  *resource_size = iter->resource()->resource_size;
  gfx::SizeF requested_tile_size =
      gfx::SizeF(iter.CurrentTiling()->tile_size());
  *resource_uv_size =
      gfx::SizeF(requested_tile_size.width() / resource_size->width(),
                 requested_tile_size.height() / resource_size->height());
}

gfx::Rect TileDisplayLayerImpl::GetDamageRect() const {
  return damage_rect_;
}

void TileDisplayLayerImpl::ResetChangeTracking() {
  LayerImpl::ResetChangeTracking();
  damage_rect_.SetRect(0, 0, 0, 0);
}

gfx::ContentColorUsage TileDisplayLayerImpl::GetContentColorUsage() const {
  return content_color_usage_;
}

void TileDisplayLayerImpl::RecordDamage(const gfx::Rect& damage_rect) {
  damage_rect_.Union(damage_rect);
}

void TileDisplayLayerImpl::DiscardResource(viz::ResourceId resource) {
  layer_tree_impl()->host_impl()->resource_provider()->RemoveImportedResource(
      std::move(resource));
}

std::vector<float> TileDisplayLayerImpl::GetSafeToDeleteTilings() {
  std::vector<float> safe_to_delete_scales;
  for (float scale : proposed_tiling_scales_for_deletion_) {
    // Check if a tiling corresponding to the candidate scale is present in
    // |last_append_quads_scales_|.
    auto it = std::find(last_append_quads_scales_.begin(),
                        last_append_quads_scales_.end(), scale);
    if (it == last_append_quads_scales_.end()) {
      // If a tiling corresponding to the candidate scale is not present in
      // |last_append_quads_scales_|, then its safe to delete.
      safe_to_delete_scales.push_back(scale);
    }
  }
  proposed_tiling_scales_for_deletion_.clear();
  return safe_to_delete_scales;
}

float TileDisplayLayerImpl::GetIdealContentsScaleKey() const {
  const auto ideal_scale = GetIdealContentsScale();
  return std::max(ideal_scale.x(), ideal_scale.y());
}

void TileDisplayLayerImpl::AppendQuadsForResourcelessSoftwareDraw(
    const AppendQuadsContext& context,
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data,
    viz::SharedQuadState* shared_quad_state,
    const Occlusion& scaled_occlusion) {
  // `DRAW_MODE_RESOURCELESS_SOFTWARE` is a renderer-only software draw mode,
  // and its handling is thus specific to the renderer-side PictureLayerImpl. It
  // should never be propagated to the Viz side.
  NOTREACHED();
}

TilingSetCoverageIterator<TileDisplayLayerTiling> TileDisplayLayerImpl::Cover(
    const gfx::Rect& coverage_rect,
    float coverage_scale,
    float ideal_contents_scale) {
  return TilingSetCoverageIterator<TileDisplayLayerTiling>(
      tilings_, coverage_rect, coverage_scale, ideal_contents_scale);
}

TileBasedLayerImpl<TileDisplayLayerTiling>::TilingResolution
TileDisplayLayerImpl::GetTilingResolutionForDebugBorders(
    const TileDisplayLayerTiling* tiling) const {
  const float ideal_scale_key = GetIdealContentsScaleKey();
  if (MathUtil::IsFloatNearlyTheSame(tiling->contents_scale_key(),
                                     ideal_scale_key)) {
    // NOTE: The above check is not exactly the same computation as is
    // used by PictureLayerImpl, as high resolution tiles within
    // PictureLayerImpl use `raster_contents_scale_`, which is not
    // necessarily the ideal scale. However, we don't have the former
    // field here, so use the ideal scale as an approximation.
    // TODO(crbug.com/450651370): Determine whether we want to fix this.
    return TilingResolution::kHigh;
  }
  if (tiling->contents_scale_key() > ideal_scale_key) {
    return TilingResolution::kAboveHigh;
  }
  return TilingResolution::kBelowHigh;
}

}  // namespace cc
