// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/discardable_image_map.h"

#include <stddef.h>

#include <algorithm>
#include <limits>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/memory/stack_allocated.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_shader.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

class DiscardableImageMap::Generator {
  STACK_ALLOCATED();

 public:
  Generator(DiscardableImageMap& map,
            SkNoDrawCanvas& canvas,
            const PaintOpBuffer& buffer,
            const ScrollOffsetMap& raster_inducing_scroll_offsets)
      : map_(map),
        canvas_(canvas),
        raster_inducing_scroll_offsets_(raster_inducing_scroll_offsets) {
    GatherDiscardableImages(buffer, nullptr);
  }

 private:
  static constexpr int kMaxRectsSize = 256;

  class ImageGatheringProvider : public ImageProvider {
    STACK_ALLOCATED();

   public:
    ImageGatheringProvider(Generator& generator, const gfx::Rect& op_rect)
        : generator_(generator), op_rect_(op_rect) {}
    ~ImageGatheringProvider() override = default;

    ScopedResult GetRasterContent(const DrawImage& draw_image) override {
      generator_.AddImage(draw_image.paint_image(), false,
                          SkRect::Make(draw_image.src_rect()), op_rect_,
                          SkM44(), draw_image.filter_quality());
      return ScopedResult();
    }

   private:
    Generator& generator_;
    gfx::Rect op_rect_;
  };

  // Adds discardable images from |buffer| to the set of images tracked by
  // this generator. If |buffer| is being used in a DrawOp that requires
  // rasterization of the buffer as a pre-processing step for execution of the
  // op (for instance, with PaintRecord backed PaintShaders),
  // |top_level_op_rect| is set to the rect for that op. If provided, the
  // |top_level_op_rect| will be used as the rect for tracking the position of
  // this image in the top-level buffer.
  void GatherDiscardableImages(const PaintOpBuffer& buffer,
                               const gfx::Rect* top_level_op_rect) {
    if (!buffer.has_discardable_images()) {
      return;
    }

    // Prevent PaintOpBuffers from having side effects back into the canvas.
    SkAutoCanvasRestore save_restore(&canvas_, true);

    PlaybackParams params(nullptr, canvas_.getLocalToDevice());
    // TODO(khushalsagar): Optimize out save/restore blocks if there are no
    // images in the draw ops between them.
    for (const PaintOp& op : buffer) {
      // We need to play non-draw ops on the SkCanvas since they can affect the
      // transform/clip state.
      if (!op.IsDrawOp())
        op.Raster(&canvas_, params);

      if (!PaintOp::OpHasDiscardableImages(op))
        continue;

      gfx::Rect op_rect;
      if (top_level_op_rect) {
        op_rect = *top_level_op_rect;
      } else {
        const SkRect& clip_rect = SkRect::Make(canvas_.getDeviceClipBounds());
        const SkMatrix& ctm = canvas_.getTotalMatrix();

        op_rect = PaintOp::ComputePaintRect(op, clip_rect, ctm);
      }
      if (op_rect.IsEmpty()) {
        continue;
      }

      const SkM44& ctm = canvas_.getLocalToDevice();
      if (op.IsPaintOpWithFlags()) {
        AddImageFromFlags(op_rect,
                          static_cast<const PaintOpWithFlags&>(op).flags, ctm);
      }

      PaintOpType op_type = op.GetType();
      if (op_type == PaintOpType::kDrawImage) {
        const auto& image_op = static_cast<const DrawImageOp&>(op);
        AddImage(
            image_op.image, image_op.flags.useDarkModeForImage(),
            SkRect::MakeIWH(image_op.image.width(), image_op.image.height()),
            op_rect, ctm, image_op.flags.getFilterQuality());
      } else if (op_type == PaintOpType::kDrawImageRect) {
        const auto& image_rect_op = static_cast<const DrawImageRectOp&>(op);
        // TODO(crbug.com/40735471): Make a RectToRect method that uses SkM44s
        // in MathUtil.
        SkM44 matrix = ctm * SkM44(SkMatrix::RectToRect(image_rect_op.src,
                                                        image_rect_op.dst));
        AddImage(image_rect_op.image, image_rect_op.flags.useDarkModeForImage(),
                 image_rect_op.src, op_rect, matrix,
                 image_rect_op.flags.getFilterQuality());
      } else if (op_type == PaintOpType::kDrawSkottie) {
        const auto& skottie_op = static_cast<const DrawSkottieOp&>(op);
        for (const auto& image_pair : skottie_op.images) {
          const SkottieFrameData& frame_data = image_pair.second;
          // Add the whole image (no cropping).
          SkRect image_src_rect = SkRect::MakeIWH(frame_data.image.width(),
                                                  frame_data.image.height());
          // It is too difficult to tell which specific portion of the animation
          // frame this image will ultimately occupy. So just assume it occupies
          // the whole animation frame for the purposes of finding which images
          // overlap with a given rectangle on the screen.
          gfx::Rect dst_rect = op_rect;
          // Skottie ultimately takes care of scaling and positioning the image
          // internally within the animation frame. However, the image that gets
          // cached in the ImageDecodeCache should have dimensions that at least
          // roughly reflect the ultimate output both for cache space
          // consumption reasons and to make Skottie's scaling job easier
          // (performance). For this reason, the DrawImage submitted to the
          // cache is scaled by the same amount that the entire animation frame
          // itself is scaled. This should get the image dimensions in the right
          // ballpark in the event that the animation's native size and the
          // destination's size differ drastically.
          //
          // Do not allow stretching the image in 1 dimension when scaling. This
          // matches Skottie's scaling behavior.
          static constexpr SkMatrix::ScaleToFit kScalingMode =
              SkMatrix::kCenter_ScaleToFit;
          SkRect skottie_frame_native_size =
              SkRect::MakeSize(skottie_op.skottie->size());
          SkM44 matrix = ctm * SkM44(SkMatrix::RectToRect(
                                   skottie_frame_native_size,
                                   gfx::RectToSkRect(dst_rect), kScalingMode));
          AddImage(frame_data.image, /*use_dark_mode=*/false,
                   std::move(image_src_rect), std::move(dst_rect), matrix,
                   frame_data.quality);
        }
      } else if (op_type == PaintOpType::kDrawRecord) {
        GatherDiscardableImages(
            static_cast<const DrawRecordOp&>(op).record.buffer(),
            top_level_op_rect);
      } else if (op_type == PaintOpType::kDrawScrollingContents) {
        const auto& draw_scrolling_contents_op =
            static_cast<const DrawScrollingContentsOp&>(op);
        canvas_.save();
        gfx::PointF scroll_offset = raster_inducing_scroll_offsets_.at(
            draw_scrolling_contents_op.scroll_element_id);
        canvas_.translate(-scroll_offset.x(), -scroll_offset.y());
        GatherDiscardableImages(
            draw_scrolling_contents_op.display_item_list->paint_op_buffer(),
            top_level_op_rect);
        canvas_.restore();
      }
    }
  }

  void AddImageFromFlags(const gfx::Rect& op_rect,
                         const PaintFlags& flags,
                         const SkM44& ctm) {
    // TODO(prashant.n): Add dark mode support for images from shaders/filters.
    AddImageFromShader(op_rect, flags.getShader(), ctm,
                       flags.getFilterQuality());
    AddImageFromFilter(op_rect, flags.getImageFilter().get());
  }

  void AddImageFromShader(const gfx::Rect& op_rect,
                          const PaintShader* shader,
                          const SkM44& ctm,
                          PaintFlags::FilterQuality filter_quality) {
    if (!shader || !shader->HasDiscardableImages()) {
      return;
    }

    if (shader->shader_type() == PaintShader::Type::kImage) {
      const PaintImage& paint_image = shader->paint_image();
      SkM44 matrix = ctm * SkM44(shader->GetLocalMatrix());
      // TODO(prashant.n): Add dark mode support for images from shader.
      AddImage(paint_image, false,
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
      if (!shader->GetRasterizationTileRect(ctm.asM33(), &scaled_tile_rect)) {
        return;
      }

      SkNoDrawCanvas canvas(scaled_tile_rect.width(),
                            scaled_tile_rect.height());
      canvas.setMatrix(SkMatrix::RectToRect(shader->tile(), scaled_tile_rect));
      base::AutoReset<bool> auto_reset(&only_gather_animated_images_, true);
      size_t prev_images_size = map_.images_.size();
      GatherDiscardableImages(shader->paint_record()->buffer(), &op_rect);

      // We only track animated images for PaintShaders. If we added any entry
      // to the |map_.images_|, this shader any has animated images.
      // Note that it is thread-safe to set the |has_animated_images| bit on
      // PaintShader here since the analysis is done on the main thread, before
      // the PaintOpBuffer is used for rasterization.
      DCHECK_GE(map_.images_.size(), prev_images_size);
      const bool has_animated_images = map_.images_.size() > prev_images_size;
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
    size_t prev_images_size = map_.images_.size();
    ImageGatheringProvider image_provider(*this, op_rect);
    filter->SnapshotWithImages(&image_provider);

    DCHECK_GE(map_.images_.size(), prev_images_size);
    const bool has_animated_images = map_.images_.size() > prev_images_size;
    const_cast<PaintFilter*>(filter)->set_has_animated_images(
        has_animated_images);
  }

  void AddImage(PaintImage paint_image,
                bool use_dark_mode,
                const SkRect& src_rect,
                const gfx::Rect& image_rect,
                const SkM44& matrix,
                PaintFlags::FilterQuality filter_quality) {
    if (paint_image.IsTextureBacked())
      return;

    SkIRect src_irect;
    src_rect.roundOut(&src_irect);

    if (paint_image.IsPaintWorklet()) {
      map_.paint_worklet_inputs_.emplace_back(
          paint_image.GetPaintWorkletInput(), paint_image.stable_id());
    }

    auto& rects = map_.image_id_to_rects_[paint_image.stable_id()];
    if (rects.size() >= kMaxRectsSize) {
      rects.back().Union(image_rect);
    } else {
      rects.push_back(image_rect);
    }

    if (paint_image.IsLazyGenerated()) {
      auto decoding_mode_it =
          map_.decoding_mode_map_.find(paint_image.stable_id());
      // Use the decoding mode if we don't have one yet, otherwise use the more
      // conservative one of the two existing ones.
      if (decoding_mode_it == map_.decoding_mode_map_.end()) {
        map_.decoding_mode_map_[paint_image.stable_id()] =
            paint_image.decoding_mode();
      } else {
        decoding_mode_it->second = PaintImage::GetConservative(
            decoding_mode_it->second, paint_image.decoding_mode());
      }
    }

    if (paint_image.ShouldAnimate()) {
      map_.animated_images_metadata_.emplace_back(
          paint_image.stable_id(), paint_image.completion_state(),
          paint_image.GetFrameMetadata(), paint_image.repetition_count(),
          paint_image.reset_animation_sequence_id());
    }

    bool add_image = true;
    if (paint_image.IsPaintWorklet()) {
      // PaintWorklet-backed images don't go through the image decode pipeline
      // (they are painted pre-raster from LayerTreeHostImpl), so do not need to
      // be added to the |map_.images_|.
      add_image = false;
    } else if (only_gather_animated_images_) {
      // If we are iterating images in a record shader, only track them if they
      // are animated. We defer decoding of images in record shaders to skia,
      // but we still need to track animated images to invalidate and advance
      // the animation in cc.
      add_image = paint_image.ShouldAnimate();
    }

    if (add_image) {
      map_.images_.emplace_back(DrawImage(std::move(paint_image), use_dark_mode,
                                          src_irect, filter_quality, matrix),
                                image_rect);
    }
  }

  DiscardableImageMap& map_;
  SkNoDrawCanvas& canvas_;
  const ScrollOffsetMap& raster_inducing_scroll_offsets_;
  bool only_gather_animated_images_ = false;
};  // DiscardableImageMap::Generator

DiscardableImageMap::DiscardableImageMap() = default;

// Once a DiscardableImage is generated, it's hold in a ref-counted
// DisplayItemList which may be destructed from any thread.
DiscardableImageMap::~DiscardableImageMap() = default;

scoped_refptr<DiscardableImageMap> DiscardableImageMap::Generate(
    const PaintOpBuffer& paint_op_buffer,
    const gfx::Rect& bounds,
    const ScrollOffsetMap& raster_inducing_scroll_offsets) {
  TRACE_EVENT0("cc", "DiscardableImageMap::Generate");
  scoped_refptr<DiscardableImageMap> image_map(new DiscardableImageMap());
  if (!paint_op_buffer.has_discardable_images()) {
    return image_map;
  }

  SkNoDrawCanvas canvas(bounds.right(), bounds.bottom());
  Generator generator(*image_map, canvas, paint_op_buffer,
                      raster_inducing_scroll_offsets);
  CHECK(!image_map->images_rtree_);
  return image_map;
}

base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
DiscardableImageMap::TakeDecodingModeMap() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::move(decoding_mode_map_);
}

std::vector<const DrawImage*> DiscardableImageMap::GetDiscardableImagesInRect(
    const gfx::Rect& rect) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<const DrawImage*> result;
  if (!images_rtree_) {
    images_rtree_ = std::make_unique<RTree<const DrawImage*>>();

    images_rtree_->Build(
        images_.size(), [this](size_t index) { return images_[index].second; },
        [this](size_t index) { return &images_[index].first; });
  }
  images_rtree_->Search(rect, &result);
  return result;
}

const DiscardableImageMap::Rects& DiscardableImageMap::GetRectsForImage(
    PaintImage::Id image_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static const base::NoDestructor<Rects> kEmptyRects;
  auto it = image_id_to_rects_.find(image_id);
  return it == image_id_to_rects_.end() ? *kEmptyRects : it->second;
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
