// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_LAYER_IMPL_H_
#define CC_LAYERS_PICTURE_LAYER_IMPL_H_

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/tile_size_calculator.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/image_id.h"
#include "cc/raster/lcd_text_disallowed_reason.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/picture_layer_tiling_set.h"
#include "cc/tiles/tile_index.h"
#include "cc/tiles/tile_priority.h"
#include "cc/tiles/tiling_set_eviction_queue.h"
#include "cc/trees/image_animation_controller.h"

namespace cc {

class AppendQuadsData;
class MicroBenchmarkImpl;
class Tile;

class CC_EXPORT PictureLayerImpl
    : public LayerImpl,
      public PictureLayerTilingClient,
      public ImageAnimationController::AnimationDriver {
 public:
  static std::unique_ptr<PictureLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                  int id) {
    return base::WrapUnique(new PictureLayerImpl(tree_impl, id));
  }
  PictureLayerImpl(const PictureLayerImpl&) = delete;
  ~PictureLayerImpl() override;

  PictureLayerImpl& operator=(const PictureLayerImpl&) = delete;

  void SetIsBackdropFilterMask(bool is_backdrop_filter_mask) {
    is_backdrop_filter_mask_ = is_backdrop_filter_mask;
  }
  bool is_backdrop_filter_mask() const { return is_backdrop_filter_mask_; }

  // LayerImpl overrides.
  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void PushPropertiesTo(LayerImpl* layer) override;
  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  void NotifyTileStateChanged(const Tile* tile) override;
  gfx::Rect GetDamageRect() const override;
  void ResetChangeTracking() override;
  void ResetRasterScale();
  void DidBeginTracing() override;
  void ReleaseResources() override;
  void ReleaseTileResources() override;
  void RecreateTileResources() override;
  Region GetInvalidationRegionForDebugging() override;
  gfx::Rect GetEnclosingVisibleRectInTargetSpace() const override;
  gfx::ContentColorUsage GetContentColorUsage() const override;
  DamageReasonSet GetDamageReasons() const override;

  // PictureLayerTilingClient overrides.
  std::unique_ptr<Tile> CreateTile(const Tile::CreateInfo& info) override;
  gfx::Size CalculateTileSize(const gfx::Size& content_bounds) override;
  const Region* GetPendingInvalidation() override;
  const PictureLayerTiling* GetPendingOrActiveTwinTiling(
      const PictureLayerTiling* tiling) const override;
  bool HasValidTilePriorities() const override;
  bool RequiresHighResToDraw() const override;
  const PaintWorkletRecordMap& GetPaintWorkletRecords() const override;
  std::vector<const DrawImage*> GetDiscardableImagesInRect(
      const gfx::Rect& rect) const override;
  ScrollOffsetMap GetRasterInducingScrollOffsets() const override;
  const GlobalStateThatImpactsTilePriority& global_tile_state() const override;

  // ImageAnimationController::AnimationDriver overrides.
  bool ShouldAnimate(PaintImage::Id paint_image_id) const override;

  void set_gpu_raster_max_texture_size(gfx::Size gpu_raster_max_texture_size) {
    gpu_raster_max_texture_size_ = gpu_raster_max_texture_size;
  }

  gfx::Size gpu_raster_max_texture_size() {
    return gpu_raster_max_texture_size_;
  }

  void UpdateRasterSource(scoped_refptr<RasterSource> raster_source,
                          Region* new_invalidation);
  void RegenerateDiscardableImageMapIfNeeded();
  bool UpdateTiles();

  // Mask-related functions.
  void GetContentsResourceId(viz::ResourceId* resource_id,
                             gfx::Size* resource_size,
                             gfx::SizeF* resource_uv_size) const override;

  size_t GPUMemoryUsageInBytes() const override;

  void RunMicroBenchmark(MicroBenchmarkImpl* benchmark) override;

  bool CanHaveTilings() const;

  PictureLayerTilingSet* picture_layer_tiling_set() { return tilings_.get(); }

  // Functions used by tile manager.
  PictureLayerImpl* GetPendingOrActiveTwinLayer() const;
  bool IsOnActiveOrPendingTree() const;

  // Used for benchmarking
  RasterSource* GetRasterSource() const { return raster_source_.get(); }

  // This enum is the return value of the InvalidateRegionForImages() call. The
  // possible values represent the fact that there are no images on this layer
  // (kNoImages), the fact that the invalidation images don't cause an
  // invalidation on this layer (kNoInvalidation), or the fact that the layer
  // was invalidated (kInvalidated).
  enum class ImageInvalidationResult {
    kNoImages,
    kNoInvalidation,
    kInvalidated,
  };

  ImageInvalidationResult InvalidateRegionForImages(
      const PaintImageIdFlatSet& images_to_invalidate);

  void InvalidateRasterInducingScrolls(
      const base::flat_set<ElementId>& scrolls_to_invalidate);

  bool can_use_lcd_text() const {
    return lcd_text_disallowed_reason_ == LCDTextDisallowedReason::kNone;
  }
  LCDTextDisallowedReason lcd_text_disallowed_reason() const {
    return lcd_text_disallowed_reason_;
  }
  LCDTextDisallowedReason ComputeLCDTextDisallowedReasonForTesting() const;

  const Region& InvalidationForTesting() const { return invalidation_; }

  // Set the paint result (PaintRecord) for a given PaintWorkletInput.
  void SetPaintWorkletRecord(scoped_refptr<const PaintWorkletInput>,
                             PaintRecord);

  // Retrieve the map of PaintWorkletInputs to their painted results
  // (PaintRecords). If a PaintWorkletInput has not been painted yet, it will
  // map to nullptr.
  const PaintWorkletRecordMap& GetPaintWorkletRecordMap() const {
    return paint_worklet_records_;
  }

  // Invalidates all PaintWorklets in this layer who depend on the given
  // property to be painted. Used when the value for the property is changed by
  // an animation, at which point the PaintWorklet must be re-painted.
  void InvalidatePaintWorklets(const PaintWorkletInput::PropertyKey& key,
                               const PaintWorkletInput::PropertyValue& prev,
                               const PaintWorkletInput::PropertyValue& next);

  void SetContentsScaleForTesting(float scale) {
    ideal_contents_scale_ = raster_contents_scale_ =
        gfx::Vector2dF(scale, scale);
  }

  void AddLastAppendQuadsTilingForTesting(PictureLayerTiling* tiling) {
    last_append_quads_tilings_.push_back(tiling);
  }

  void set_has_non_animated_image_update_rect() {
    has_non_animated_image_update_rect_ = true;
  }

  // Returns the set of tiles which have been updated since the last call to
  // this method. This returns tile indices for each updated tile, grouped by
  // the scale key of their respective tiling. Beware that this is not pruned,
  // so tilings or tiles identified within may no longer exist.
  using TileUpdateSet = std::map<float, std::set<TileIndex>>;
  TileUpdateSet TakeUpdatedTiles();

 protected:
  friend class RasterizeAndRecordBenchmarkImpl;

  PictureLayerImpl(LayerTreeImpl* tree_impl, int id);
  PictureLayerTiling* AddTiling(const gfx::AxisTransform2d& contents_transform);
  void RemoveAllTilings();
  bool CanRecreateHighResTilingForLCDTextAndRasterTransform(
      const PictureLayerTiling& high_res) const;
  void UpdateTilingsForRasterScaleAndTranslation(bool adjusted_raster_scale);
  void AddLowResolutionTilingIfNeeded();
  bool ShouldAdjustRasterScale() const;
  void RecalculateRasterScales();
  void AdjustRasterScaleForTransformAnimation(
      const gfx::Vector2dF& preserved_raster_contents_scale);
  float MinimumRasterContentsScaleForWillChangeTransform() const;
  // Returns false if raster translation is not applicable.
  bool CalculateRasterTranslation(gfx::Vector2dF& raster_translation) const;
  void CleanUpTilingsOnActiveLayer(
      const std::vector<raw_ptr<PictureLayerTiling, VectorExperimental>>&
          used_tilings);
  float MinimumContentsScale() const;
  float MaximumContentsScale() const;
  void UpdateViewportRectForTilePriorityInContentSpace();
  PictureLayerImpl* GetRecycledTwinLayer() const;
  bool ShouldDirectlyCompositeImage(float raster_scale) const;

  // Returns the raster scale that should be used for a directly composited
  // image. This takes into account the ideal contents scale to ensure we don't
  // use too much memory for layers that are small due to contents scale
  // factors, and bumps up the reduced scale if those layers end up increasing
  // their contents scale.
  float CalculateDirectlyCompositedImageRasterScale() const;

  void UpdateRasterSourceInternal(
      scoped_refptr<RasterSource> raster_source,
      Region* new_invalidation,
      const PictureLayerTilingSet* pending_set,
      const PaintWorkletRecordMap* pending_paint_worklet_records,
      const DiscardableImageMap* pending_discardable_image_map);

  bool IsDirectlyCompositedImage() const;
  void UpdateDirectlyCompositedImageFromRasterSource();

  void SanityCheckTilingState() const;

  void GetDebugBorderProperties(SkColor4f* color, float* width) const override;
  void GetAllPrioritizedTilesForTracing(
      std::vector<PrioritizedTile>* prioritized_tiles) const override;
  void AsValueInto(base::trace_event::TracedValue* dict) const override;

  void UpdateIdealScales();
  float MaximumTilingContentsScale() const;
  std::unique_ptr<PictureLayerTilingSet> CreatePictureLayerTilingSet();

  void RegisterAnimatedImages();
  void UnregisterAnimatedImages();

  // Set the collection of PaintWorkletInput as well as their PaintImageId that
  // are part of this layer.
  void SetPaintWorkletInputs(
      const std::vector<DiscardableImageMap::PaintWorkletInputWithImageId>&
          inputs);

  LCDTextDisallowedReason ComputeLCDTextDisallowedReason(
      bool raster_translation_aligns_pixels) const;
  void UpdateCanUseLCDText(bool raster_translation_aligns_pixels);

  // Whether the transform node for this layer, or any ancestor transform
  // node, has a will-change hint for one of the transform properties.
  bool AffectedByWillChangeTransformHint() const;

  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of speedometer3).
  RAW_PTR_EXCLUSION PictureLayerImpl* twin_layer_ = nullptr;

  std::unique_ptr<PictureLayerTilingSet> tilings_ =
      CreatePictureLayerTilingSet();
  scoped_refptr<RasterSource> raster_source_;
  Region invalidation_;
  scoped_refptr<const DiscardableImageMap> discardable_image_map_;

  // Ideal scales are calcuated from the transforms applied to the layer. They
  // represent the best known scale from the layer to the final output.
  // Page scale is from user pinch/zoom.
  float ideal_page_scale_ = 0.f;
  // Device scale is from screen dpi, and it comes from device scale facter.
  float ideal_device_scale_ = 0.f;
  // Source scale comes from javascript css scale.
  gfx::Vector2dF ideal_source_scale_;
  // Contents scale = device scale * page scale * source scale.
  gfx::Vector2dF ideal_contents_scale_;

  // Raster scales are set from ideal scales. They are scales we choose to
  // raster at. They may not match the ideal scales at times to avoid raster for
  // performance reasons.
  float raster_page_scale_ = 0.f;
  float raster_device_scale_ = 0.f;
  gfx::Vector2dF raster_source_scale_;
  gfx::Vector2dF raster_contents_scale_;
  float low_res_raster_contents_scale_ = 0.f;

  float ideal_source_scale_key() const {
    return std::max(ideal_source_scale_.x(), ideal_source_scale_.y());
  }
  float ideal_contents_scale_key() const {
    return std::max(ideal_contents_scale_.x(), ideal_contents_scale_.y());
  }
  float raster_source_scale_key() const {
    return std::max(raster_source_scale_.x(), raster_source_scale_.y());
  }
  float raster_contents_scale_key() const {
    return std::max(raster_contents_scale_.x(), raster_contents_scale_.y());
  }

  bool is_backdrop_filter_mask_ : 1 = false;

  bool was_screen_space_transform_animating_ : 1 = false;
  bool only_used_low_res_last_append_quads_ : 1 = false;

  bool nearest_neighbor_ : 1 = false;

  // This is set by UpdateRasterSource() on change of raster source size. It's
  // used to recalculate raster scale for will-chagne:transform. It's reset to
  // false after raster scale update.
  bool raster_source_size_changed_ : 1 = false;

  bool directly_composited_image_default_raster_scale_changed_ : 1 = false;

  bool needs_regenerate_discardable_image_map_ : 1 = false;

  // Keep track of if a non-empty update_rect is due to animated image or other
  // reasons.
  bool has_animated_image_update_rect_ : 1 = false;
  bool has_non_animated_image_update_rect_ : 1 = false;

  LCDTextDisallowedReason lcd_text_disallowed_reason_ =
      LCDTextDisallowedReason::kNoText;

  // If this scale is not zero, it indicates that this layer is a directly
  // composited image layer (i.e. the only thing drawn into this layer is an
  // image). The rasterized pixels will be the same as the image's original
  // pixels if this scale is used as the raster scale.
  // To avoid re-raster on scale changes, this may be different than the used
  // raster scale, see: |RecalculateRasterScales()| and
  // |CalculateDirectlyCompositedImageRasterScale()|.
  // TODO(crbug.com/40176440): Support 2D scales in directly composited images.
  float directly_composited_image_default_raster_scale_ = 0;

  // Use this instead of |visible_layer_rect()| for tiling calculations. This
  // takes external viewport and transform for tile priority into account.
  gfx::Rect viewport_rect_for_tile_priority_in_content_space_;

  gfx::Size gpu_raster_max_texture_size_;

  // List of tilings that were used last time we appended quads. This can be
  // used as an optimization not to remove tilings if they are still being
  // drawn. Note that accessing this vector should only be done in the context
  // of comparing pointers, since objects pointed to are not guaranteed to
  // exist.
  std::vector<raw_ptr<PictureLayerTiling, VectorExperimental>>
      last_append_quads_tilings_;

  // The set of PaintWorkletInputs that are part of this PictureLayerImpl, and
  // their painted results (if any). During commit, Blink hands us a set of
  // PaintWorkletInputs that are part of this layer. These are then painted
  // asynchronously on a worklet thread, triggered from
  // |LayerTreeHostImpl::UpdateSyncTreeAfterCommitOrImplSideInvalidation|.
  PaintWorkletRecordMap paint_worklet_records_;

  TileSizeCalculator tile_size_calculator_{this};

  // Denotes an area that is damaged and needs redraw. This is in the layer's
  // space.
  gfx::Rect damage_rect_;

  // Tracks tiles changed since the last call to TakeUpdatedTiles().
  TileUpdateSet updated_tiles_;
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_LAYER_IMPL_H_
