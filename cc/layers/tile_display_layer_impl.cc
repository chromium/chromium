// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_display_layer_impl.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "cc/layers/append_quads_data.h"
#include "cc/tiles/tiling_set_coverage_iterator.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace cc {

namespace {

class TilingOrder {
 public:
  bool operator()(const std::unique_ptr<TileDisplayLayerImpl::Tiling>& left,
                  const std::unique_ptr<TileDisplayLayerImpl::Tiling>& right) {
    return left->contents_scale_key() > right->contents_scale_key();
  }
};

}  // namespace

TileDisplayLayerImpl::TileResource::TileResource(
    const viz::TransferableResource& resource,
    bool is_premultiplied,
    bool is_checkered)
    : resource(resource),
      is_premultiplied(is_premultiplied),
      is_checkered(is_checkered) {}

TileDisplayLayerImpl::TileResource::TileResource(const TileResource&) = default;

TileDisplayLayerImpl::TileResource&
TileDisplayLayerImpl::TileResource::operator=(const TileResource&) = default;

TileDisplayLayerImpl::TileResource::~TileResource() = default;

TileDisplayLayerImpl::Tile::Tile() = default;

TileDisplayLayerImpl::Tile::Tile(const TileContents& contents)
    : contents_(contents) {
  DCHECK(!absl::holds_alternative<NoContents>(contents_));
}

TileDisplayLayerImpl::Tile::Tile(Tile&&) = default;

TileDisplayLayerImpl::Tile& TileDisplayLayerImpl::Tile::operator=(Tile&&) =
    default;

TileDisplayLayerImpl::Tile::~Tile() = default;

TileDisplayLayerImpl::Tiling::Tiling(TileDisplayLayerImpl& layer,
                                     float scale_key)
    : layer_(layer), scale_key_(scale_key) {}

TileDisplayLayerImpl::Tiling::~Tiling() = default;

TileDisplayLayerImpl::Tile* TileDisplayLayerImpl::Tiling::TileAt(
    const TileIndex& index) const {
  auto it = tiles_.find(index);
  if (it == tiles_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void TileDisplayLayerImpl::Tiling::SetRasterTransform(
    const gfx::AxisTransform2d& transform) {
  DCHECK_EQ(std::max(transform.scale().x(), transform.scale().y()), scale_key_);
  raster_transform_ = transform;
}

void TileDisplayLayerImpl::Tiling::SetTileSize(const gfx::Size& size) {
  if (size == tiling_data_.max_texture_size()) {
    return;
  }

  tiling_data_.SetMaxTextureSize(size);
  tiles_.clear();
}

void TileDisplayLayerImpl::Tiling::SetTilingRect(const gfx::Rect& rect) {
  if (rect == tiling_data_.tiling_rect()) {
    return;
  }

  tiling_data_.SetTilingRect(rect);
  tiles_.clear();
}

void TileDisplayLayerImpl::Tiling::SetTileContents(
    const TileIndex& key,
    const TileContents& contents) {
  std::unique_ptr<Tile> old_tile;
  if (absl::holds_alternative<NoContents>(contents)) {
    auto it = tiles_.find(key);
    if (it != tiles_.end()) {
      old_tile = std::move(it->second);
      tiles_.erase(it);
    }
  } else {
    old_tile = std::exchange(tiles_[key], std::make_unique<Tile>(contents));
  }

  if (old_tile) {
    if (auto* resource = absl::get_if<TileResource>(&old_tile->contents())) {
      layer_->discarded_resources_.push_back(resource->resource);
    }
  }
}

TileDisplayLayerImpl::DisplayTilingCoverageIterator
TileDisplayLayerImpl::Tiling::Cover(const gfx::Rect& coverage_rect,
                                    float coverage_scale) const {
  return DisplayTilingCoverageIterator(this, coverage_scale, coverage_rect);
}

TileDisplayLayerImpl::TileDisplayLayerImpl(Client& client,
                                           LayerTreeImpl& tree,
                                           int id)
    : LayerImpl(&tree, id), client_(client) {}

TileDisplayLayerImpl::~TileDisplayLayerImpl() = default;

TileDisplayLayerImpl::Tiling&
TileDisplayLayerImpl::GetOrCreateTilingFromScaleKey(float scale_key) {
  auto it = std::find_if(tilings_.begin(), tilings_.end(),
                         [scale_key](const auto& tiling) {
                           return tiling->contents_scale_key() == scale_key;
                         });
  if (it != tilings_.end()) {
    return **it;
  }

  tilings_.push_back(std::make_unique<Tiling>(*this, scale_key));
  Tiling& tiling = *tilings_.back();
  std::sort(tilings_.begin(), tilings_.end(), TilingOrder());
  return tiling;
}

mojom::LayerType TileDisplayLayerImpl::GetLayerType() const {
  return mojom::LayerType::kTileDisplay;
}

std::unique_ptr<LayerImpl> TileDisplayLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  NOTREACHED_NORETURN();
}

void TileDisplayLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  NOTREACHED_NORETURN();
}

void TileDisplayLayerImpl::AppendQuads(viz::CompositorRenderPass* render_pass,
                                       AppendQuadsData* append_quads_data) {
  if (tilings_.empty()) {
    return;
  }

  const float max_contents_scale =
      tilings_.empty() ? 1.0f : tilings_.back()->contents_scale_key();
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateScaledSharedQuadState(shared_quad_state, max_contents_scale,
                                contents_opaque());
  const Occlusion scaled_occlusion =
      draw_properties()
          .occlusion_in_content_space.GetOcclusionWithGivenDrawTransform(
              shared_quad_state->quad_to_target_transform);

  // If we're doing a regular AppendQuads (ie, not solid color or resourceless
  // software draw, and if the visible rect is scrolled far enough away, then we
  // may run into a floating point precision in AA calculations in the renderer.
  // See crbug.com/765297. In order to avoid this, we shift the quads up from
  // where they logically reside and adjust the shared_quad_state's transform
  // instead. We only do this in a scale/translate matrices to ensure the math
  // is correct.
  gfx::Vector2d quad_offset;
  if (shared_quad_state->quad_to_target_transform.IsScaleOrTranslation()) {
    const auto& visible_rect = shared_quad_state->visible_quad_layer_rect;
    quad_offset = gfx::Vector2d(-visible_rect.x(), -visible_rect.y());
  }

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

  std::vector<viz::TransferableResource> used_resources;
  const auto ideal_scale = GetIdealContentsScale();
  const float ideal_scale_key = std::max(ideal_scale.x(), ideal_scale.y());
  for (auto iter = TilingSetCoverageIterator<Tiling>(
           tilings_, shared_quad_state->visible_quad_layer_rect,
           max_contents_scale, ideal_scale_key);
       iter; ++iter) {
    const gfx::Rect geometry_rect = iter.geometry_rect();
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
                     resource->resource.id, texture_rect,
                     iter.CurrentTiling()->tile_size(),
                     resource->is_premultiplied, /*nearest_neighbor=*/false,
                     /*enable_edge_aa=*/false);
        used_resources.push_back(resource->resource);
        has_draw_quad = true;
      } else if (auto color = iter->solid_color()) {
        has_draw_quad = true;
        const float alpha = color->fA * shared_quad_state->opacity;
        if (alpha >= std::numeric_limits<float>::epsilon()) {
          auto* quad =
              render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
          quad->SetNew(shared_quad_state, offset_geometry_rect,
                       offset_visible_geometry_rect, *color,
                       /*enable_edge_aa=*/false);
        }
      }
    }
    if (!has_draw_quad) {
      // Checkerboard due to missing raster.
      SkColor4f color = safe_opaque_background_color();
      auto* quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      quad->SetNew(shared_quad_state, offset_geometry_rect,
                   offset_visible_geometry_rect, color, false);
    }
  }

  // Adjust shared_quad_state with the quad_offset, since we've adjusted each
  // quad we've appended by it.
  shared_quad_state->quad_to_target_transform.Translate(-quad_offset);
  shared_quad_state->quad_layer_rect.Offset(quad_offset);
  shared_quad_state->visible_quad_layer_rect.Offset(quad_offset);

  client_->DidAppendQuadsWithResources(used_resources);
}

}  // namespace cc
