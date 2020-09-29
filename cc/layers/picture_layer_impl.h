// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_LAYER_IMPL_H_
#define CC_LAYERS_PICTURE_LAYER_IMPL_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/tile_size_calculator.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/image_id.h"
#include "cc/raster/lcd_text_disallowed_reason.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/picture_layer_tiling_set.h"
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
  const char* LayerTypeAsString() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
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
  gfx::Rect GetEnclosingRectInTargetSpace() const override;
  gfx::ContentColorUsage GetContentColorUsage() const override;

  // PictureLayerTilingClient overrides.
  std::unique_ptr<Tile> CreateTile(const Tile::CreateInfo& info) override;
  gfx::Size CalculateTileSize(const gfx::Size& content_bounds) override;
  const Region* GetPendingInvalidation() override;
  const PictureLayerTiling* GetPendingOrActiveTwinTiling(
      const PictureLayerTiling* tiling) const override;
  bool HasValidTilePriorities() const override;
  bool RequiresHighResToDraw() const override;
  const PaintWorkletRecordMap& GetPaintWorkletRecords() const override;
  bool IsDirectlyCompositedImage() const override;

  // ImageAnimationController::AnimationDriver overrides.
  bool ShouldAnimate(PaintImage::Id paint_image_id) const override;

  void set_gpu_raster_max_texture_size(gfx::Size gpu_raster_max_texture_size) {
    gpu_raster_max_texture_size_ = gpu_raster_max_texture_size;
  }

  gfx::Size gpu_raster_max_texture_size() {
    return gpu_raster_max_texture_size_;
  }

  void UpdateRasterSource(
      scoped_refptr<RasterSource> raster_source,
      Region* new_invalidation,
      const PictureLayerTilingSet* pending_set,
      const PaintWorkletRecordMap* pending_paint_worklet_records);
  bool UpdateTiles();

  // Mask-related functions.
  void GetContentsResourceId(viz::ResourceId* resource_id,
                             gfx::Size* resource_size,
                             gfx::SizeF* resource_uv_size) const override;

  void SetNearestNeighbor(bool nearest_neighbor);

  void SetDirectlyCompositedImageSize(base::Optional<gfx::Size>);

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
                             sk_sp<PaintRecord>);

  // Retrieve the map of PaintWorkletInputs to their painted results
  // (PaintRecords). If a PaintWorkletInput has not been painted yet, it will
  // map to nullptr.
  const PaintWorkletRecordMap& GetPaintWorkletRecordMap() const {
    return paint_worklet_records_;
  }

  gfx::Size content_bounds() { return content_bounds_; }

  // Invalidates all PaintWorklets in this layer who depend on the given
  // property to be painted. Used when the value for the property is changed by
  // an animation, at which point the PaintWorklet must be re-painted.
  void InvalidatePaintWorklets(const PaintWorkletInput::PropertyKey& key);

  void SetContentsScaleForTesting(float scale) {
    ideal_contents_scale_ = raster_contents_scale_ = scale;
  }

 protected:
  PictureLayerImpl(LayerTreeImpl* tree_impl, int id);
  PictureLayerTiling* AddTiling(const gfx::AxisTransform2d& contents_transform);
  void RemoveAllTilings();
  bool CanRecreateHighResTilingForLCDTextAndRasterTranslation(
      const PictureLayerTiling& high_res) const;
  void UpdateTilingsForRasterScaleAndTranslation(bool adjusted_raster_scale);
  void AddLowResolutionTilingIfNeeded();
  bool ShouldAdjustRasterScale() const;
  void RecalculateRasterScales();
  // Returns false if raster translation is not applicable.
  bool CalculateRasterTranslation(gfx::Vector2dF& raster_translation) const;
  void CleanUpTilingsOnActiveLayer(
      const std::vector<PictureLayerTiling*>& used_tilings);
  float MinimumContentsScale() const;
  float MaximumContentsScale() const;
  void UpdateViewportRectForTilePriorityInContentSpace();
  PictureLayerImpl* GetRecycledTwinLayer() const;
  bool ShouldDirectlyCompositeImage(float raster_scale) const;

  // Returns the default raster scale used for current layer bounds and directly
  // composited image size. To avoid re-raster on scale changes, this may be
  // different than the used raster scale, see: |RecalculateRasterScales()| and
  // |CalculateDirectlyCompositedImageRasterScale()|.
  float GetDefaultDirectlyCompositedImageRasterScale() const;

  // Returns the raster scale that should be used for a directly composited
  // image. This takes into account the ideal contents scale to ensure we don't
  // use too much memory for layers that are small due to contents scale
  // factors, and bumps up the reduced scale if those layers end up increasing
  // their contents scale.
  float CalculateDirectlyCompositedImageRasterScale() const;

  void SanityCheckTilingState() const;

  void GetDebugBorderProperties(SkColor* color, float* width) const override;
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

  // TODO(crbug.com/1114504): For now this checks the immediate transform node
  // only. The callers may actually want to know if this layer or ancestor has
  // will change transform.
  bool HasWillChangeTransformHint() const;

  PictureLayerImpl* twin_layer_;

  std::unique_ptr<PictureLayerTilingSet> tilings_;
  scoped_refptr<RasterSource> raster_source_;
  Region invalidation_;

  // Ideal scales are calcuated from the transforms applied to the layer. They
  // represent the best known scale from the layer to the final output.
  // Page scale is from user pinch/zoom.
  float ideal_page_scale_;
  // Device scale is from screen dpi, and it comes from device scale facter.
  float ideal_device_scale_;
  // Source scale comes from javascript css scale.
  float ideal_source_scale_;
  // Contents scale = device scale * page scale * source scale.
  float ideal_contents_scale_;

  // Raster scales are set from ideal scales. They are scales we choose to
  // raster at. They may not match the ideal scales at times to avoid raster for
  // performance reasons.
  float raster_page_scale_;
  float raster_device_scale_;
  float raster_source_scale_;
  float raster_contents_scale_;
  float low_res_raster_contents_scale_;

  bool is_backdrop_filter_mask_ : 1;

  bool was_screen_space_transform_animating_ : 1;
  bool only_used_low_res_last_append_quads_ : 1;

  bool nearest_neighbor_ : 1;

  LCDTextDisallowedReason lcd_text_disallowed_reason_;

  // The intrinsic size of the directly composited image. A directly composited
  // image is an image which is the only thing drawn into a layer. In these
  // cases we attempt to raster the image at its intrinsic size.
  base::Optional<gfx::Size> directly_composited_image_size_;

  // The default raster source scale for a directly composited image, the last
  // time raster scales were calculated. This will be the same as
  // |raster_source_scale_| if no adjustments were made in
  // |CalculateDirectlyCompositedImageRasterScale()|.
  float directly_composited_image_initial_raster_scale_;

  // Use this instead of |visible_layer_rect()| for tiling calculations. This
  // takes external viewport and transform for tile priority into account.
  gfx::Rect viewport_rect_for_tile_priority_in_content_space_;

  gfx::Size gpu_raster_max_texture_size_;

  // List of tilings that were used last time we appended quads. This can be
  // used as an optimization not to remove tilings if they are still being
  // drawn. Note that accessing this vector should only be done in the context
  // of comparing pointers, since objects pointed to are not guaranteed to
  // exist.
  std::vector<PictureLayerTiling*> last_append_quads_tilings_;

  // The set of PaintWorkletInputs that are part of this PictureLayerImpl, and
  // their painted results (if any). During commit, Blink hands us a set of
  // PaintWorkletInputs that are part of this layer. These are then painted
  // asynchronously on a worklet thread, triggered from
  // |LayerTreeHostImpl::UpdateSyncTreeAfterCommitOrImplSideInvalidation|.
  PaintWorkletRecordMap paint_worklet_records_;

  gfx::Size content_bounds_;
  TileSizeCalculator tile_size_calculator_;

  // Denotes an area that is damaged and needs redraw. This is in the layer's
  // space.
  gfx::Rect damage_rect_;
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_LAYER_IMPL_H_
