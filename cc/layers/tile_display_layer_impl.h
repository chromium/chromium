// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TILE_DISPLAY_LAYER_IMPL_H_
#define CC_LAYERS_TILE_DISPLAY_LAYER_IMPL_H_

#include <map>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "base/memory/raw_ref.h"
#include "cc/base/tiling_data.h"
#include "cc/cc_export.h"
#include "cc/layers/tile_based_layer_impl.h"
#include "cc/mojom/missing_tile_reason.mojom.h"
#include "cc/tiles/tile_draw_info.h"
#include "cc/tiles/tile_index.h"
#include "cc/tiles/tile_priority.h"
#include "cc/tiles/tiling_coverage_iterator.h"
#include "cc/tiles/tiling_set_coverage_iterator.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class TileDisplayLayerImpl;
class TileDisplayLayerTiling;
class DisplayTilingCoverageIterator;

struct TileDisplayLayerNoContents {
  mojom::MissingTileReason reason = mojom::MissingTileReason::kResourceNotReady;

  TileDisplayLayerNoContents() = default;
  explicit TileDisplayLayerNoContents(mojom::MissingTileReason r) : reason(r) {}
};

struct TileDisplayLayerTileResource {
  viz::ResourceId resource_id;
  gfx::Size resource_size;
};

using TileDisplayLayerTileContents = std::variant<TileDisplayLayerNoContents,
                                                  SkColor4f,
                                                  TileDisplayLayerTileResource>;

class CC_EXPORT TileDisplayLayerTile {
 public:
  explicit TileDisplayLayerTile(TileDisplayLayerImpl& layer,
                                const TileDisplayLayerTileContents& contents);
  ~TileDisplayLayerTile();
  TileDisplayLayerTile(TileDisplayLayerTile&&);

  TileDrawInfo::Mode draw_mode() {
    CHECK(IsReadyToDraw());
    if (solid_color()) {
      return TileDrawInfo::SOLID_COLOR_MODE;
    } else if (is_oom()) {
      return TileDrawInfo::OOM_MODE;
    } else {
      CHECK(resource());
      return TileDrawInfo::RESOURCE_MODE;
    }
  }

  const TileDisplayLayerTileContents& contents() const { return contents_; }

  std::optional<SkColor4f> solid_color() const {
    if (std::holds_alternative<SkColor4f>(contents_)) {
      return std::get<SkColor4f>(contents_);
    }
    return std::nullopt;
  }

  std::optional<TileDisplayLayerTileResource> resource() const {
    if (std::holds_alternative<TileDisplayLayerTileResource>(contents_)) {
      return std::get<TileDisplayLayerTileResource>(contents_);
    }
    return std::nullopt;
  }

  bool is_oom() const {
    if (std::holds_alternative<TileDisplayLayerNoContents>(contents_)) {
      return std::get<TileDisplayLayerNoContents>(contents_).reason ==
             mojom::MissingTileReason::kOutOfMemory;
    }
    return false;
  }

  bool IsReadyToDraw() const {
    return !std::holds_alternative<TileDisplayLayerNoContents>(contents_) ||
           is_oom();
  }

 private:
  const raw_ref<TileDisplayLayerImpl> layer_;
  TileDisplayLayerTileContents contents_;
};

class CC_EXPORT TileDisplayLayerTiling {
 public:
  using Tile = TileDisplayLayerTile;
  using TileMap = std::map<TileIndex, std::unique_ptr<Tile>>;
  using CoverageIterator = DisplayTilingCoverageIterator;

  explicit TileDisplayLayerTiling(TileDisplayLayerImpl& layer, float scale_key);
  ~TileDisplayLayerTiling();

  Tile* TileAt(const TileIndex& index) const;

  float contents_scale_key() const { return scale_key_; }
  TileResolution resolution() const { return HIGH_RESOLUTION; }
  const TilingData* tiling_data() const { return &tiling_data_; }
  gfx::Size raster_size() const;
  const gfx::AxisTransform2d& raster_transform() const {
    return raster_transform_;
  }
  const gfx::Size tile_size() const { return tiling_data_.max_texture_size(); }
  const gfx::Rect tiling_rect() const { return tiling_data_.tiling_rect(); }
  const TileMap& tiles() const { return tiles_; }

  void SetRasterTransform(const gfx::AxisTransform2d& transform);
  void SetTileSize(const gfx::Size& size);
  void SetTilingRect(const gfx::Rect& rect);
  void SetTileContents(const TileIndex& key,
                       const TileDisplayLayerTileContents& contents,
                       bool update_damage);

  CoverageIterator Cover(const gfx::Rect& coverage_rect,
                         float coverage_scale) const;

 private:
  const raw_ref<TileDisplayLayerImpl> layer_;
  const float scale_key_;
  gfx::AxisTransform2d raster_transform_;
  TilingData tiling_data_{gfx::Size(), gfx::Rect(), /*border_texels=*/1};
  TileMap tiles_;
};

class CC_EXPORT DisplayTilingCoverageIterator
    : public TilingCoverageIterator<TileDisplayLayerTiling> {
 public:
  using TilingCoverageIterator<TileDisplayLayerTiling>::TilingCoverageIterator;
};

// Viz-side counterpart to a client-side PictureLayerImpl when TreesInViz is
// enabled. Clients push tiling information and tile contents from a picture
// layer down to Viz, and this layer uses that information to draw tile quads.
class CC_EXPORT TileDisplayLayerImpl
    : public TileBasedLayerImpl<TileDisplayLayerTiling> {
 public:
  using NoContents = TileDisplayLayerNoContents;
  using TileResource = TileDisplayLayerTileResource;
  using TileContents = TileDisplayLayerTileContents;

  TileDisplayLayerImpl(LayerTreeImpl& tree, int id);
  ~TileDisplayLayerImpl() override;

  TileDisplayLayerTiling& GetOrCreateTilingFromScaleKey(float scale_key);
  void RemoveTiling(float scale_key);
  void SetIsDirectlyCompositedImage(bool is_directly_composited_image) {
    is_directly_composited_image_ = is_directly_composited_image;
  }
  void SetNearestNeighbor(bool nearest_neighbor) {
    nearest_neighbor_ = nearest_neighbor;
  }
  void SetRecordedBounds(const gfx::Rect& bounds) { recorded_bounds_ = bounds; }
  bool IsDirectlyCompositedImage() const override;
  void SetProposedTilingScalesForDeletion(
      std::vector<float> proposed_tiling_scales) {
    proposed_tiling_scales_for_deletion_ = std::move(proposed_tiling_scales);
  }
  bool nearest_neighbor() const { return nearest_neighbor_; }

  // LayerImpl overrides:
  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void PushPropertiesTo(LayerImpl* layer) override;
  void GetContentsResourceId(viz::ResourceId* resource_id,
                             gfx::Size* resource_size,
                             gfx::SizeF* resource_uv_size) const override;
  gfx::Rect GetDamageRect() const override;
  void ResetChangeTracking() override;
  gfx::ContentColorUsage GetContentColorUsage() const override;

  void SetContentColorUsage(gfx::ContentColorUsage content_color_usage) {
    content_color_usage_ = content_color_usage;
  }

  void RecordDamage(const gfx::Rect& damage_rect);

  const TileDisplayLayerTiling* GetTilingForTesting(float scale_key) const;
  void DiscardResource(viz::ResourceId resource);

  // Returns a list of tiling scales that were proposed for deletion by
  // renderer and were *not* used in the most recent frame, meaning they
  // safe to clean up.
  std::vector<float> GetSafeToDeleteTilings();

  // For testing
  std::optional<SkColor4f> solid_color_for_testing() const {
    return solid_color();
  }
  std::vector<float>& LastAppendQuadsScalesForTesting() {
    return last_append_quads_scales_;
  }

 private:
  // TileBasedLayerImpl:
  void AppendQuadsSpecialization(const AppendQuadsContext& context,
                                 viz::CompositorRenderPass* render_pass,
                                 AppendQuadsData* append_quads_data,
                                 viz::SharedQuadState* shared_quad_state,
                                 const Occlusion& scaled_occlusion,
                                 const gfx::Vector2d& quad_offset) override;
  float GetMaximumContentsScaleForUseInAppendQuads() override;
  float GetIdealContentsScaleKey() const override;
  void AppendQuadsForResourcelessSoftwareDraw(
      const AppendQuadsContext& context,
      viz::CompositorRenderPass* render_pass,
      AppendQuadsData* append_quads_data,
      viz::SharedQuadState* shared_quad_state,
      const Occlusion& scaled_occlusion) override;
  TilingSetCoverageIterator<TileDisplayLayerTiling> Cover(
      const gfx::Rect& coverage_rect,
      float coverage_scale,
      float ideal_contents_scale) override;
  TilingResolution GetTilingResolutionForDebugBorders(
      const TileDisplayLayerTiling* tiling) const override;

  bool is_directly_composited_image_ = false;
  bool nearest_neighbor_ = false;
  gfx::ContentColorUsage content_color_usage_ = gfx::ContentColorUsage::kSRGB;
  gfx::Rect recorded_bounds_;

  // Denotes an area that is damaged and needs redraw. This is in the layer's
  // space.
  gfx::Rect damage_rect_;
  std::vector<std::unique_ptr<TileDisplayLayerTiling>> tilings_;

  // List of tiling scales that were used last time we appended quads. This is
  // used as an optimization not to remove tilings if they are still being
  // drawn. The renderer will propose list of candidate tilings for deletion to
  // Viz represented by |proposed_tiling_scales_for_deletion_|, and the
  // Viz process will confirm which of those are safe to delete
  // (i.e. not used in the last frame) before the renderer actually removes
  // them. This keeps the renderer’s tile management logic close to its
  // current behavior and prevents premature deletion of tiles still needed by
  // Viz. Note that unlike PictureLayerImpl, we have last appended quads scales
  // here instead of tiling ptr since its not needed in this case.
  std::vector<float> last_append_quads_scales_;

  // A list of tiling scale keys that the client has nominated for deletion.
  // This allows the client to suggest cleanup, but Viz makes the final
  // decision. After each frame, we determine which of these candidate scales
  // were *not* used for drawing (by checking against
  // `last_append_quads_scales_`). That final set of unused scales is then sent
  // back to the client, confirming that the corresponding tilings can be safely
  // destroyed.
  std::vector<float> proposed_tiling_scales_for_deletion_;
};

}  // namespace cc

#endif  // CC_LAYERS_TILE_DISPLAY_LAYER_IMPL_H_
