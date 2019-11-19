// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/discardable_image_map.h"

#include <stddef.h>

#include <algorithm>
#include <limits>

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {
const int kMaxRectsSize = 256;

SkRect MapRect(const SkMatrix& matrix, const SkRect& src) {
  SkRect dst;
  matrix.mapRect(&dst, src);
  return dst;
}

// This canvas is used only for tracking transform/clip/filter state from the
// non-drawing ops.
class PaintTrackingCanvas final : public SkNoDrawCanvas {
 public:
  PaintTrackingCanvas(int width, int height) : SkNoDrawCanvas(width, height) {}
  ~PaintTrackingCanvas() override = default;

  bool ComputePaintBounds(const SkRect& rect,
                          const SkPaint* current_paint,
                          SkRect* paint_bounds) {
    *paint_bounds = rect;
    if (current_paint) {
      if (!current_paint->canComputeFastBounds())
        return false;
      *paint_bounds =
          current_paint->computeFastBounds(*paint_bounds, paint_bounds);
    }

    for (const auto& paint : base::Reversed(saved_paints_)) {
      if (!paint.canComputeFastBounds())
        return false;
      *paint_bounds = paint.computeFastBounds(*paint_bounds, paint_bounds);
    }

    return true;
  }

 private:
  // SkNoDrawCanvas overrides.
  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override {
    saved_paints_.push_back(rec.fPaint ? *rec.fPaint : SkPaint());
    return SkNoDrawCanvas::getSaveLayerStrategy(rec);
  }

  void willSave() override {
    saved_paints_.push_back(SkPaint());
    return SkNoDrawCanvas::willSave();
  }

  void willRestore() override {
    DCHECK_GT(saved_paints_.size(), 0u);
    saved_paints_.pop_back();
    SkNoDrawCanvas::willRestore();
  }

  std::vector<SkPaint> saved_paints_;
};

class DiscardableImageGenerator {
 public:
  DiscardableImageGenerator(int width,
                            int height,
                            const PaintOpBuffer* buffer) {
    PaintTrackingCanvas canvas(width, height);
    GatherDiscardableImages(buffer, nullptr, &canvas);
  }
  ~DiscardableImageGenerator() = default;

  std::vector<std::pair<DrawImage, gfx::Rect>> TakeImages() {
    return std::move(image_set_);
  }
  base::flat_map<PaintImage::Id, DiscardableImageMap::Rects>
  TakeImageIdToRectsMap() {
    return std::move(image_id_to_rects_);
  }
  base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
  TakeDecodingModeMap() {
    return std::move(decoding_mode_map_);
  }
  std::vector<DiscardableImageMap::AnimatedImageMetadata>
  TakeAnimatedImagesMetadata() {
    return std::move(animated_images_metadata_);
  }
  std::vector<DiscardableImageMap::PaintWorkletInputWithImageId>
  TakePaintWorkletInputs() {
    return std::move(paint_worklet_inputs_);
  }

  void RecordColorHistograms() const {
    if (color_stats_total_image_count_ > 0) {
      int srgb_image_percent = (100 * color_stats_srgb_image_count_) /
                               color_stats_total_image_count_;
      UMA_HISTOGRAM_PERCENTAGE("Renderer4.ImagesPercentSRGB",
                               srgb_image_percent);
    }

    base::CheckedNumeric<int> srgb_pixel_percent =
        100 * color_stats_srgb_pixel_count_ / color_stats_total_pixel_count_;
    if (srgb_pixel_percent.IsValid()) {
      UMA_HISTOGRAM_PERCENTAGE("Renderer4.ImagePixelsPercentSRGB",
                               srgb_pixel_percent.ValueOrDie());
    }
  }

  bool all_images_are_srgb() const {
    return color_stats_srgb_image_count_ == color_stats_total_image_count_;
  }

 private:
  class ImageGatheringProvider : public ImageProvider {
   public:
    ImageGatheringProvider(DiscardableImageGenerator* generator,
                           const gfx::Rect& op_rect)
        : generator_(generator), op_rect_(op_rect) {}
    ~ImageGatheringProvider() override = default;

    ScopedResult GetRasterContent(const DrawImage& draw_image) override {
      generator_->AddImage(draw_image.paint_image(),
                           SkRect::Make(draw_image.src_rect()), op_rect_,
                           SkMatrix::I(), draw_image.filter_quality());
      return ScopedResult();
    }

   private:
    DiscardableImageGenerator* generator_;
    gfx::Rect op_rect_;
  };

  // Adds discardable images from |buffer| to the set of images tracked by
  // this generator. If |buffer| is being used in a DrawOp that requires
  // rasterization of the buffer as a pre-processing step for execution of the
  // op (for instance, with PaintRecord backed PaintShaders),
  // |top_level_op_rect| is set to the rect for that op. If provided, the
  // |top_level_op_rect| will be used as the rect for tracking the position of
  // this image in the top-level buffer.
  void GatherDiscardableImages(const PaintOpBuffer* buffer,
                               const gfx::Rect* top_level_op_rect,
                               PaintTrackingCanvas* canvas) {
    if (!buffer->HasDiscardableImages())
      return;

    // Prevent PaintOpBuffers from having side effects back into the canvas.
    SkAutoCanvasRestore save_restore(canvas, true);

    PlaybackParams params(nullptr, canvas->getTotalMatrix());
    // TODO(khushalsagar): Optimize out save/restore blocks if there are no
    // images in the draw ops between them.
    for (auto* op : PaintOpBuffer::Iterator(buffer)) {
      // We need to play non-draw ops on the SkCanvas since they can affect the
      // transform/clip state.
      if (!op->IsDrawOp())
        op->Raster(canvas, params);

      if (!PaintOp::OpHasDiscardableImages(op))
        continue;

      gfx::Rect op_rect;
      base::Optional<gfx::Rect> local_op_rect;

      if (top_level_op_rect) {
        op_rect = *top_level_op_rect;
      } else {
        local_op_rect = ComputePaintRect(op, canvas);
        if (local_op_rect.value().IsEmpty())
          continue;

        op_rect = local_op_rect.value();
      }

      const SkMatrix& ctm = canvas->getTotalMatrix();
      if (op->IsPaintOpWithFlags()) {
        AddImageFromFlags(op_rect,
                          static_cast<const PaintOpWithFlags*>(op)->flags, ctm);
      }

      PaintOpType op_type = static_cast<PaintOpType>(op->type);
      if (op_type == PaintOpType::DrawImage) {
        auto* image_op = static_cast<DrawImageOp*>(op);
        AddImage(
            image_op->image,
            SkRect::MakeIWH(image_op->image.width(), image_op->image.height()),
            op_rect, ctm, image_op->flags.getFilterQuality());
      } else if (op_type == PaintOpType::DrawImageRect) {
        auto* image_rect_op = static_cast<DrawImageRectOp*>(op);
        SkMatrix matrix = ctm;
        matrix.postConcat(SkMatrix::MakeRectToRect(image_rect_op->src,
                                                   image_rect_op->dst,
                                                   SkMatrix::kFill_ScaleToFit));
        AddImage(image_rect_op->image, image_rect_op->src, op_rect, matrix,
                 image_rect_op->flags.getFilterQuality());
      } else if (op_type == PaintOpType::DrawRecord) {
        GatherDiscardableImages(
            static_cast<const DrawRecordOp*>(op)->record.get(),
            top_level_op_rect, canvas);
      }
    }
  }

  // Given the |op_rect|, which is the rect for the draw op, returns the
  // transformed rect accounting for the current transform, clip and paint
  // state on |canvas_|.
  gfx::Rect ComputePaintRect(const PaintOp* op, PaintTrackingCanvas* canvas) {
    const SkRect& clip_rect = SkRect::Make(canvas->getDeviceClipBounds());
    const SkMatrix& ctm = canvas->getTotalMatrix();

    gfx::Rect transformed_rect;
    SkRect op_rect;
    if (!op->IsDrawOp() || !PaintOp::GetBounds(op, &op_rect)) {
      // If we can't provide a conservative bounding rect for the op, assume it
      // covers the complete current clip.
      // TODO(khushalsagar): See if we can do something better for non-draw ops.
      transformed_rect = gfx::ToEnclosingRect(gfx::SkRectToRectF(clip_rect));
    } else {
      const PaintFlags* flags =
          op->IsPaintOpWithFlags()
              ? &static_cast<const PaintOpWithFlags*>(op)->flags
              : nullptr;
      SkPaint paint;
      if (flags)
        paint = flags->ToSkPaint();

      SkRect paint_rect = MapRect(ctm, op_rect);
      bool computed_paint_bounds =
          canvas->ComputePaintBounds(paint_rect, &paint, &paint_rect);
      if (!computed_paint_bounds) {
        // TODO(vmpstr): UMA this case.
        paint_rect = clip_rect;
      }

      // Clamp the image rect by the current clip rect.
      if (!paint_rect.intersect(clip_rect))
        return gfx::Rect();

      transformed_rect = gfx::ToEnclosingRect(gfx::SkRectToRectF(paint_rect));
    }

    // During raster, we use the device clip bounds on the canvas, which outsets
    // the actual clip by 1 due to the possibility of antialiasing. Account for
    // this here by outsetting the image rect by 1. Note that this only affects
    // queries into the rtree, which will now return images that only touch the
    // bounds of the query rect.
    //
    // Note that it's not sufficient for us to inset the device clip bounds at
    // raster time, since we might be sending a larger-than-one-item display
    // item to skia, which means that skia will internally determine whether to
    // raster the picture (using device clip bounds that are outset).
    transformed_rect.Inset(-1, -1);
    return transformed_rect;
  }

  void AddImageFromFlags(const gfx::Rect& op_rect,
                         const PaintFlags& flags,
                         const SkMatrix& ctm) {
    AddImageFromShader(op_rect, flags.getShader(), ctm,
                       flags.getFilterQuality());
    AddImageFromFilter(op_rect, flags.getImageFilter().get());
  }

  void AddImageFromShader(const gfx::Rect& op_rect,
                          const PaintShader* shader,
                          const SkMatrix& ctm,
                          SkFilterQuality filter_quality) {
    if (!shader || !shader->has_discardable_images())
      return;

    if (shader->shader_type() == PaintShader::Type::kImage) {
      const PaintImage& paint_image = shader->paint_image();
      SkMatrix matrix = ctm;
      matrix.postConcat(shader->GetLocalMatrix());
      AddImage(paint_image,
               SkRect::MakeWH(paint_image.width(), paint_image.height()),
               op_rect, matrix, filter_quality);
      return;
    }

    if (shader->shader_type() == PaintShader::Type::kPaintRecord) {
      // For record backed shaders, only analyze them if they have animated
      // images.
      if (shader->image_analysis_state() ==
          ImageAnalysisState::kNoAnimatedImages) {
        return;
      }

      SkRect scaled_tile_rect;
      if (!shader->GetRasterizationTileRect(ctm, &scaled_tile_rect)) {
        return;
      }

      PaintTrackingCanvas canvas(scaled_tile_rect.width(),
                                 scaled_tile_rect.height());
      canvas.setMatrix(SkMatrix::MakeRectToRect(
          shader->tile(), scaled_tile_rect, SkMatrix::kFill_ScaleToFit));
      base::AutoReset<bool> auto_reset(&only_gather_animated_images_, true);
      size_t prev_image_set_size = image_set_.size();
      GatherDiscardableImages(shader->paint_record().get(), &op_rect, &canvas);

      // We only track animated images for PaintShaders. If we added any entry
      // to the |image_set_|, this shader any has animated images.
      // Note that it is thread-safe to set the |has_animated_images| bit on
      // PaintShader here since the analysis is done on the main thread, before
      // the PaintOpBuffer is used for rasterization.
      DCHECK_GE(image_set_.size(), prev_image_set_size);
      const bool has_animated_images = image_set_.size() > prev_image_set_size;
      const_cast<PaintShader*>(shader)->set_has_animated_images(
          has_animated_images);
    }
  }

  void AddImageFromFilter(const gfx::Rect& op_rect, const PaintFilter* filter) {
    // Only analyze filters if they have animated images.
    if (!filter || !filter->has_discardable_images() ||
        filter->image_analysis_state() ==
            ImageAnalysisState::kNoAnimatedImages) {
      return;
    }

    base::AutoReset<bool> auto_reset(&only_gather_animated_images_, true);
    size_t prev_image_set_size = image_set_.size();
    ImageGatheringProvider image_provider(this, op_rect);
    filter->SnapshotWithImages(&image_provider);

    DCHECK_GE(image_set_.size(), prev_image_set_size);
    const bool has_animated_images = image_set_.size() > prev_image_set_size;
    const_cast<PaintFilter*>(filter)->set_has_animated_images(
        has_animated_images);
  }

  void AddImage(PaintImage paint_image,
                const SkRect& src_rect,
                const gfx::Rect& image_rect,
                const SkMatrix& matrix,
                SkFilterQuality filter_quality) {
    if (paint_image.IsTextureBacked())
      return;

    SkIRect src_irect;
    src_rect.roundOut(&src_irect);

    if (paint_image.IsPaintWorklet()) {
      paint_worklet_inputs_.push_back(std::make_pair(
          paint_image.paint_worklet_input(), paint_image.stable_id()));
    } else {
      // Make a note if any image was originally specified in a non-sRGB color
      // space. PaintWorklets do not have the concept of a color space, so
      // should not be used to accumulate either counter.
      SkColorSpace* source_color_space = paint_image.color_space();
      color_stats_total_pixel_count_ += image_rect.size().GetCheckedArea();
      color_stats_total_image_count_++;
      if (!source_color_space || source_color_space->isSRGB()) {
        color_stats_srgb_pixel_count_ += image_rect.size().GetCheckedArea();
        color_stats_srgb_image_count_++;
      }
    }

    auto& rects = image_id_to_rects_[paint_image.stable_id()];
    if (rects->size() >= kMaxRectsSize)
      rects->back().Union(image_rect);
    else
      rects->push_back(image_rect);

    auto decoding_mode_it = decoding_mode_map_.find(paint_image.stable_id());
    // Use the decoding mode if we don't have one yet, otherwise use the more
    // conservative one of the two existing ones.
    if (decoding_mode_it == decoding_mode_map_.end()) {
      decoding_mode_map_[paint_image.stable_id()] = paint_image.decoding_mode();
    } else {
      decoding_mode_it->second = PaintImage::GetConservative(
          decoding_mode_it->second, paint_image.decoding_mode());
    }

    if (paint_image.ShouldAnimate()) {
      animated_images_metadata_.emplace_back(
          paint_image.stable_id(), paint_image.completion_state(),
          paint_image.GetFrameMetadata(), paint_image.repetition_count(),
          paint_image.reset_animation_sequence_id());
    }

    bool add_image = true;
    if (paint_image.IsPaintWorklet()) {
      // PaintWorklet-backed images don't go through the image decode pipeline
      // (they are painted pre-raster from LayerTreeHostImpl), so do not need to
      // be added to the |image_set_|.
      add_image = false;
    } else if (only_gather_animated_images_) {
      // If we are iterating images in a record shader, only track them if they
      // are animated. We defer decoding of images in record shaders to skia,
      // but we still need to track animated images to invalidate and advance
      // the animation in cc.
      add_image = paint_image.ShouldAnimate();
    }

    if (add_image) {
      image_set_.emplace_back(
          DrawImage(std::move(paint_image), src_irect, filter_quality, matrix),
          image_rect);
    }
  }

  std::vector<std::pair<DrawImage, gfx::Rect>> image_set_;
  base::flat_map<PaintImage::Id, DiscardableImageMap::Rects> image_id_to_rects_;
  std::vector<DiscardableImageMap::AnimatedImageMetadata>
      animated_images_metadata_;
  std::vector<DiscardableImageMap::PaintWorkletInputWithImageId>
      paint_worklet_inputs_;
  PaintImageIdFlatSet paint_worklet_image_ids_;
  base::flat_map<PaintImage::Id, PaintImage::DecodingMode> decoding_mode_map_;
  bool only_gather_animated_images_ = false;

  // Statistics about the number of images and pixels that will require color
  // conversion if the target color space is not sRGB.
  int color_stats_srgb_image_count_ = 0;
  int color_stats_total_image_count_ = 0;
  base::CheckedNumeric<int64_t> color_stats_srgb_pixel_count_ = 0;
  base::CheckedNumeric<int64_t> color_stats_total_pixel_count_ = 0;
};

}  // namespace

DiscardableImageMap::DiscardableImageMap() = default;
DiscardableImageMap::~DiscardableImageMap() = default;

void DiscardableImageMap::Generate(const PaintOpBuffer* paint_op_buffer,
                                   const gfx::Rect& bounds) {
  TRACE_EVENT0("cc", "DiscardableImageMap::Generate");

  if (!paint_op_buffer->HasDiscardableImages())
    return;

  DiscardableImageGenerator generator(bounds.right(), bounds.bottom(),
                                      paint_op_buffer);
  generator.RecordColorHistograms();
  image_id_to_rects_ = generator.TakeImageIdToRectsMap();
  animated_images_metadata_ = generator.TakeAnimatedImagesMetadata();
  paint_worklet_inputs_ = generator.TakePaintWorkletInputs();
  decoding_mode_map_ = generator.TakeDecodingModeMap();
  all_images_are_srgb_ = generator.all_images_are_srgb();
  auto images = generator.TakeImages();
  images_rtree_.Build(
      images,
      [](const std::vector<std::pair<DrawImage, gfx::Rect>>& items,
         size_t index) { return items[index].second; },
      [](const std::vector<std::pair<DrawImage, gfx::Rect>>& items,
         size_t index) { return items[index].first; });
}

base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
DiscardableImageMap::TakeDecodingModeMap() {
  return std::move(decoding_mode_map_);
}

void DiscardableImageMap::GetDiscardableImagesInRect(
    const gfx::Rect& rect,
    std::vector<const DrawImage*>* images) const {
  images_rtree_.SearchRefs(rect, images);
}

const DiscardableImageMap::Rects& DiscardableImageMap::GetRectsForImage(
    PaintImage::Id image_id) const {
  static const base::NoDestructor<Rects> kEmptyRects;
  auto it = image_id_to_rects_.find(image_id);
  return it == image_id_to_rects_.end() ? *kEmptyRects : it->second;
}

void DiscardableImageMap::Reset() {
  image_id_to_rects_.clear();
  image_id_to_rects_.shrink_to_fit();
  images_rtree_.Reset();
}

DiscardableImageMap::AnimatedImageMetadata::AnimatedImageMetadata(
    PaintImage::Id paint_image_id,
    PaintImage::CompletionState completion_state,
    std::vector<FrameMetadata> frames,
    int repetition_count,
    PaintImage::AnimationSequenceId reset_animation_sequence_id)
    : paint_image_id(paint_image_id),
      completion_state(completion_state),
      frames(std::move(frames)),
      repetition_count(repetition_count),
      reset_animation_sequence_id(reset_animation_sequence_id) {}

DiscardableImageMap::AnimatedImageMetadata::~AnimatedImageMetadata() = default;

DiscardableImageMap::AnimatedImageMetadata::AnimatedImageMetadata(
    const AnimatedImageMetadata& other) = default;

}  // namespace cc
