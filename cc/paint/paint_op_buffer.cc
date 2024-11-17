// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/paint_op_buffer.h"

#include <algorithm>
#include <utility>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/types/optional_util.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/scoped_raster_flags.h"
#include "cc/paint/skottie_serialization_history.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/gpu/ganesh/GrRecordingContext.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/private/chromium/Slug.h"

namespace cc {

PlaybackCallbacks::PlaybackCallbacks() = default;
PlaybackCallbacks::~PlaybackCallbacks() = default;
PlaybackCallbacks::PlaybackCallbacks(const PlaybackCallbacks&) = default;
PlaybackCallbacks& PlaybackCallbacks::operator=(const PlaybackCallbacks&) =
    default;

PlaybackParams::PlaybackParams(ImageProvider* image_provider,
                               const SkM44& original_ctm,
                               const PlaybackCallbacks& callbacks)
    : image_provider(image_provider),
      original_ctm(original_ctm),
      callbacks(callbacks) {}

PlaybackParams::~PlaybackParams() = default;

PaintOpBuffer::SerializeOptions::SerializeOptions(
    ImageProvider* image_provider,
    TransferCacheSerializeHelper* transfer_cache,
    ClientPaintCache* paint_cache,
    SkStrikeServer* strike_server,
    sk_sp<SkColorSpace> color_space,
    SkottieSerializationHistory* skottie_serialization_history,
    bool can_use_lcd_text,
    bool context_supports_distance_field_text,
    int max_texture_size,
    const ScrollOffsetMap* raster_inducing_scroll_offsets)
    : image_provider(image_provider),
      transfer_cache(transfer_cache),
      paint_cache(paint_cache),
      strike_server(strike_server),
      color_space(std::move(color_space)),
      skottie_serialization_history(skottie_serialization_history),
      can_use_lcd_text(can_use_lcd_text),
      context_supports_distance_field_text(
          context_supports_distance_field_text),
      max_texture_size(max_texture_size),
      raster_inducing_scroll_offsets(raster_inducing_scroll_offsets) {}

PaintOpBuffer::SerializeOptions::SerializeOptions() = default;
PaintOpBuffer::SerializeOptions::SerializeOptions(const SerializeOptions&) =
    default;
PaintOpBuffer::SerializeOptions& PaintOpBuffer::SerializeOptions::operator=(
    const SerializeOptions&) = default;
PaintOpBuffer::SerializeOptions::~SerializeOptions() = default;

PaintOpBuffer::PaintOpBuffer() = default;

PaintOpBuffer::PaintOpBuffer(PaintOpBuffer&& other) {
  *this = std::move(other);
}

PaintRecord PaintOpBuffer::DeepCopyAsRecord() {
  auto result = sk_make_sp<PaintOpBuffer>();
  if (data_) {
    result->ReallocBuffer(used_);
  }

  for (const PaintOp& op : *this) {
    switch (op.GetType()) {
      case PaintOpType::kAnnotate: {
        const auto& o = static_cast<const AnnotateOp&>(op);
        result->push<AnnotateOp>(o.annotation_type, o.rect, o.data);
      } break;
      case PaintOpType::kClipPath: {
        const auto& o = static_cast<const ClipPathOp&>(op);
        result->push<ClipPathOp>(o.path, o.op, o.antialias, o.use_cache);
      } break;
      case PaintOpType::kClipRect: {
        const auto& o = static_cast<const ClipRectOp&>(op);
        result->push<ClipRectOp>(o.rect, o.op, o.antialias);
      } break;
      case PaintOpType::kClipRRect: {
        const auto& o = static_cast<const ClipRRectOp&>(op);
        result->push<ClipRRectOp>(o.rrect, o.op, o.antialias);
      } break;
      case PaintOpType::kConcat: {
        const auto& o = static_cast<const ConcatOp&>(op);
        result->push<ConcatOp>(o.matrix);
      } break;
      case PaintOpType::kCustomData: {
        const auto& o = static_cast<const CustomDataOp&>(op);
        result->push<CustomDataOp>(o.id);
      } break;
      case PaintOpType::kDrawArc: {
        const auto& o = static_cast<const DrawArcOp&>(op);
        result->push<DrawArcOp>(o.oval, o.start_angle_degrees,
                                o.sweep_angle_degrees, o.flags);
      } break;
      case PaintOpType::kDrawArcLite: {
        const auto& o = static_cast<const DrawArcLiteOp&>(op);
        result->push<DrawArcLiteOp>(o.oval, o.start_angle_degrees,
                                    o.sweep_angle_degrees, o.core_paint_flags);
      } break;
      case PaintOpType::kDrawColor: {
        const auto& o = static_cast<const DrawColorOp&>(op);
        result->push<DrawColorOp>(o.color, o.mode);
      } break;
      case PaintOpType::kDrawDRRect: {
        const auto& o = static_cast<const DrawDRRectOp&>(op);
        result->push<DrawDRRectOp>(o.outer, o.inner, o.flags);
      } break;
      case PaintOpType::kDrawImage: {
        const auto& o = static_cast<const DrawImageOp&>(op);
        result->push<DrawImageOp>(o.image, o.left, o.top, o.sampling, &o.flags);
      } break;
      case PaintOpType::kDrawImageRect: {
        const auto& o = static_cast<const DrawImageRectOp&>(op);
        result->push<DrawImageRectOp>(o.image, o.src, o.dst, o.sampling,
                                      &o.flags, o.constraint);
      } break;
      case PaintOpType::kDrawIRect: {
        const auto& o = static_cast<const DrawIRectOp&>(op);
        result->push<DrawIRectOp>(o.rect, o.flags);
      } break;
      case PaintOpType::kDrawLine: {
        const auto& o = static_cast<const DrawLineOp&>(op);
        result->push<DrawLineOp>(o.x0, o.y0, o.x1, o.y1, o.flags);
      } break;
      case PaintOpType::kDrawLineLite: {
        const auto& o = static_cast<const DrawLineLiteOp&>(op);
        result->push<DrawLineLiteOp>(o.x0, o.y0, o.x1, o.y1,
                                     o.core_paint_flags);
      } break;
      case PaintOpType::kDrawOval: {
        const auto& o = static_cast<const DrawOvalOp&>(op);
        result->push<DrawOvalOp>(o.oval, o.flags);
      } break;
      case PaintOpType::kDrawPath: {
        const auto& o = static_cast<const DrawPathOp&>(op);
        result->push<DrawPathOp>(o.path, o.flags, o.use_cache);
      } break;
      case PaintOpType::kDrawRecord: {
        const auto& o = static_cast<const DrawRecordOp&>(op);
        result->push<DrawRecordOp>(o.record, o.local_ctm);
      } break;
      case PaintOpType::kDrawRect: {
        const auto& o = static_cast<const DrawRectOp&>(op);
        result->push<DrawRectOp>(o.rect, o.flags);
      } break;
      case PaintOpType::kDrawRRect: {
        const auto& o = static_cast<const DrawRRectOp&>(op);
        result->push<DrawRRectOp>(o.rrect, o.flags);
      } break;
      case PaintOpType::kDrawScrollingContents: {
        const auto& o = static_cast<const DrawScrollingContentsOp&>(op);
        result->push<DrawScrollingContentsOp>(o.scroll_element_id,
                                              o.display_item_list);
      } break;
      case PaintOpType::kDrawSkottie: {
        const auto& o = static_cast<const DrawSkottieOp&>(op);
        result->push<DrawSkottieOp>(o.skottie, o.dst, o.t, o.images,
                                    o.color_map, o.text_map);
      } break;
      case PaintOpType::kDrawSlug: {
        const auto& o = static_cast<const DrawSlugOp&>(op);
        result->push<DrawSlugOp>(o.slug, o.flags);
      } break;
      case PaintOpType::kDrawTextBlob: {
        const auto& o = static_cast<const DrawTextBlobOp&>(op);
        result->push<DrawTextBlobOp>(o.blob, o.x, o.y, o.node_id, o.flags);
      } break;
      case PaintOpType::kDrawVertices: {
        const auto& o = static_cast<const DrawVerticesOp&>(op);
        result->push<DrawVerticesOp>(o.vertices, o.uvs, o.indices, o.flags);
      } break;
      case PaintOpType::kNoop: {
        result->push<NoopOp>();
      } break;
      case PaintOpType::kRestore: {
        result->push<RestoreOp>();
      } break;
      case PaintOpType::kRotate: {
        const auto& o = static_cast<const RotateOp&>(op);
        result->push<RotateOp>(o.degrees);
      } break;
      case PaintOpType::kSave: {
        result->push<SaveOp>();
      } break;
      case PaintOpType::kSaveLayer: {
        const auto& o = static_cast<const SaveLayerOp&>(op);
        result->push<SaveLayerOp>(o.bounds, o.flags);
      } break;
      case PaintOpType::kSaveLayerAlpha: {
        const auto& o = static_cast<const SaveLayerAlphaOp&>(op);
        result->push<SaveLayerAlphaOp>(o.bounds, o.alpha);
      } break;
      case PaintOpType::kSaveLayerFilters: {
        const auto& o = static_cast<const SaveLayerFiltersOp&>(op);
        auto f = o.filters;
        result->push<SaveLayerFiltersOp>(std::move(f), o.flags);
      } break;
      case PaintOpType::kScale: {
        const auto& o = static_cast<const ScaleOp&>(op);
        result->push<ScaleOp>(o.sx, o.sy);
      } break;
      case PaintOpType::kSetMatrix: {
        const auto& o = static_cast<const SetMatrixOp&>(op);
        result->push<SetMatrixOp>(o.matrix);
      } break;
      case PaintOpType::kSetNodeId: {
        const auto& o = static_cast<const SetNodeIdOp&>(op);
        result->push<SetNodeIdOp>(o.node_id);
      } break;
      case PaintOpType::kTranslate: {
        const auto& o = static_cast<const TranslateOp&>(op);
        result->push<TranslateOp>(o.dx, o.dy);
      } break;
    }
  }

  return PaintRecord(std::move(result));
}

PaintOpBuffer::~PaintOpBuffer() {
  DestroyOps();
}

PaintOpBuffer& PaintOpBuffer::operator=(PaintOpBuffer&& other) {
  data_ = std::move(other.data_);
  DCHECK(!other.data_);
  used_ = other.used_;
  reserved_ = other.reserved_;
  op_count_ = other.op_count_;
  num_slow_paths_up_to_min_for_MSAA_ = other.num_slow_paths_up_to_min_for_MSAA_;
  subrecord_bytes_used_ = other.subrecord_bytes_used_;
  subrecord_op_count_ = other.subrecord_op_count_;
  has_non_aa_paint_ = other.has_non_aa_paint_;
  has_draw_ops_ = other.has_draw_ops_;
  has_draw_text_ops_ = other.has_draw_text_ops_;
  has_save_layer_ops_ = other.has_save_layer_ops_;
  has_save_layer_alpha_ops_ = other.has_save_layer_alpha_ops_;
  has_effects_preventing_lcd_text_for_save_layer_alpha_ =
      other.has_effects_preventing_lcd_text_for_save_layer_alpha_;
  has_discardable_images_ = other.has_discardable_images_;
  content_color_usage_ = other.content_color_usage_;

  // Make sure the other pob can destruct safely or is ready for reuse.
  other.reserved_ = 0;
  other.ResetRetainingBuffer();
  return *this;
}

void PaintOpBuffer::DestroyOps() {
  if (data_) {
    for (size_t offset = 0; offset < used_;) {
      auto* op = reinterpret_cast<PaintOp*>(data_.get() + offset);
      offset += op->AlignedSize();
      op->DestroyThis();
    }
  }
}

void PaintOpBuffer::Reset() {
  DCHECK(is_mutable());
  DestroyOps();
  // Leave data_ allocated, reserved_ unchanged. ShrinkToFit() will take care
  // of that if called.
  ResetRetainingBuffer();
}

void PaintOpBuffer::ResetRetainingBuffer() {
  DCHECK(is_mutable());
  used_ = 0;
  op_count_ = 0;
  num_slow_paths_up_to_min_for_MSAA_ = 0;
  has_non_aa_paint_ = false;
  subrecord_bytes_used_ = 0;
  subrecord_op_count_ = 0;
  has_draw_ops_ = false;
  has_draw_text_ops_ = false;
  has_save_layer_ops_ = false;
  has_save_layer_alpha_ops_ = false;
  has_effects_preventing_lcd_text_for_save_layer_alpha_ = false;
  has_discardable_images_ = false;
  content_color_usage_ = gfx::ContentColorUsage::kSRGB;
}

void PaintOpBuffer::Playback(SkCanvas* canvas) const {
  Playback(canvas, PlaybackParams(nullptr), /*local_ctm=*/true,
           /*offsets=*/nullptr);
}

void PaintOpBuffer::Playback(SkCanvas* canvas,
                             const PlaybackParams& params,
                             bool local_ctm) const {
  Playback(canvas, params, local_ctm, /*offsets=*/nullptr);
}

PaintRecord PaintOpBuffer::ReleaseAsRecord() {
  DCHECK(is_mutable());
  const size_t old_reserved = reserved_;
  auto result = sk_make_sp<PaintOpBuffer>(std::move(*this));
  if (BufferDataPtr old_data = result->ReallocIfNeededToFit()) {
    // Reuse the original buffer for future recording.
    data_ = std::move(old_data);
    reserved_ = old_reserved;
  }
  return PaintRecord(std::move(result));
}

void PaintOpBuffer::Playback(SkCanvas* canvas,
                             const PlaybackParams& params,
                             bool local_ctm,
                             const std::vector<size_t>* offsets) const {
  if (!op_count_)
    return;
  if (offsets && offsets->empty())
    return;
  // Make sure the there are no pending saves after we are done. Add a save if
  // this `PaintOpBuffer` isn't meant to impact the global CTM (to prevent
  // PaintOps from having side effects back into the canvas)
  SkAutoCanvasRestore save_restore(canvas, /*doSave=*/local_ctm);

  bool save_layer_alpha_should_preserve_lcd_text =
      (!params.save_layer_alpha_should_preserve_lcd_text.has_value() ||
       *params.save_layer_alpha_should_preserve_lcd_text) &&
      has_draw_text_ops_ &&
      !has_effects_preventing_lcd_text_for_save_layer_alpha_;
  if (save_layer_alpha_should_preserve_lcd_text) {
    // Check if the canvas supports LCD text.
    SkSurfaceProps props;
    canvas->getProps(&props);
    if (props.pixelGeometry() == kUnknown_SkPixelGeometry)
      save_layer_alpha_should_preserve_lcd_text = false;
  }

  // TODO(enne): a PaintRecord that contains a SetMatrix assumes that the
  // SetMatrix is local to that PaintRecord itself.  Said differently, if you
  // translate(x, y), then draw a paint record with a SetMatrix(identity),
  // the translation should be preserved instead of clobbering the top level
  // transform.  This could probably be done more efficiently.
  PlaybackParams new_params = params;
  if (local_ctm) {
    new_params.original_ctm = canvas->getLocalToDevice();
  }
  new_params.save_layer_alpha_should_preserve_lcd_text =
      save_layer_alpha_should_preserve_lcd_text;
  for (PlaybackFoldingIterator iter(*this, offsets); iter; ++iter) {
    const PaintOp* op = iter.get();
    if (params.callbacks.convert_op_callback) {
      op = params.callbacks.convert_op_callback.Run(*op);
      if (!op)
        continue;
    }

    // This is an optimization to replicate the behaviour in SkCanvas
    // which rejects ops that draw outside the current clip. In the
    // general case we defer this to the SkCanvas but if we will be
    // using an ImageProvider for pre-decoding images, we can save
    // performing an expensive decode that will never be rasterized.
    const bool skip_op = new_params.image_provider &&
                         PaintOp::OpHasDiscardableImages(*op) &&
                         PaintOp::QuickRejectDraw(*op, canvas);
    if (skip_op)
      continue;

    if (op->IsPaintOpWithFlags()) {
      int max_texture_size;
      if (auto* context = canvas->recordingContext()) {
        max_texture_size = context->maxTextureSize();
      } else if (auto* recorder = canvas->recorder()) {
        max_texture_size = recorder->maxTextureSize();
      } else {
        // This can happen in tests.
        max_texture_size = 0;
      }

      const auto& flags_op = static_cast<const PaintOpWithFlags&>(*op);
      const ScopedRasterFlags scoped_flags(
          &flags_op.flags, new_params.image_provider, canvas->getTotalMatrix(),
          max_texture_size, iter.alpha());
      if (const auto* raster_flags = scoped_flags.flags())
        flags_op.RasterWithFlags(canvas, raster_flags, new_params);
    } else {
      DCHECK_EQ(iter.alpha(), 1.0f);
      op->Raster(canvas, new_params);
    }

    if (!new_params.callbacks.did_draw_op_callback.is_null()) {
      new_params.callbacks.did_draw_op_callback.Run();
    }
  }
}

bool PaintOpBuffer::Deserialize(const volatile void* input,
                                size_t input_size,
                                const PaintOp::DeserializeOptions& options) {
  size_t total_bytes_read = 0u;
  while (total_bytes_read < input_size) {
    const volatile void* next_op =
        static_cast<const volatile char*>(input) + total_bytes_read;
    size_t read_bytes = 0;
    if (!PaintOp::DeserializeIntoPaintOpBuffer(next_op,
                                               input_size - total_bytes_read,
                                               this, &read_bytes, options)) {
      return false;
    }
    total_bytes_read += read_bytes;
  }

  DCHECK_GT(size(), 0u);
  return true;
}

// static
sk_sp<PaintOpBuffer> PaintOpBuffer::MakeFromMemory(
    const volatile void* input,
    size_t input_size,
    const PaintOp::DeserializeOptions& options) {
  auto buffer = sk_make_sp<PaintOpBuffer>();
  if (input_size == 0)
    return buffer;
  if (!buffer->Deserialize(input, input_size, options))
    return nullptr;
  return buffer;
}

// static
SkRect PaintOpBuffer::GetFixedScaleBounds(const SkMatrix& ctm,
                                          const SkRect& bounds,
                                          int max_texture_size) {
  SkSize scale;
  if (!ctm.decomposeScale(&scale)) {
    // Decomposition failed, use an approximation.
    scale.set(SkScalarSqrt(ctm.getScaleX() * ctm.getScaleX() +
                           ctm.getSkewX() * ctm.getSkewX()),
              SkScalarSqrt(ctm.getScaleY() * ctm.getScaleY() +
                           ctm.getSkewY() * ctm.getSkewY()));
  }

  SkScalar raster_width = bounds.width() * scale.width();
  SkScalar raster_height = bounds.height() * scale.height();
  SkScalar tile_area = raster_width * raster_height;
  // Clamp the tile area to about 4M pixels, and per-dimension max texture size
  // if it's provided.
  static const SkScalar kMaxTileArea = 2048 * 2048;
  SkScalar down_scale = 1.f;
  if (tile_area > kMaxTileArea) {
    down_scale = SkScalarSqrt(kMaxTileArea / tile_area);
  }
  if (max_texture_size > 0) {
    // This only updates down_scale if the tile is larger than the texture size
    // after ensuring its area is less than kMaxTileArea
    down_scale = std::min(
        down_scale, max_texture_size / std::max(raster_width, raster_height));
  }

  if (down_scale < 1.f) {
    scale.set(down_scale * scale.width(), down_scale * scale.height());
  }
  return SkRect::MakeXYWH(
      bounds.fLeft * scale.width(), bounds.fTop * scale.height(),
      SkScalarCeilToInt(SkScalarAbs(scale.width() * bounds.width())),
      SkScalarCeilToInt(SkScalarAbs(scale.height() * bounds.height())));
}

PaintOpBuffer::BufferDataPtr PaintOpBuffer::ReallocBuffer(size_t new_size) {
  DCHECK_GE(new_size, used_);
  DCHECK(is_mutable());

  std::unique_ptr<char, base::AlignedFreeDeleter> new_data(
      static_cast<char*>(base::AlignedAlloc(new_size, kPaintOpAlign)));
  if (data_)
    memcpy(new_data.get(), data_.get(), used_);
  BufferDataPtr old_data = std::move(data_);
  data_ = std::move(new_data);
  reserved_ = new_size;
  return old_data;
}

void* PaintOpBuffer::AllocatePaintOpSlowPath(uint16_t aligned_size) {
  DCHECK(is_mutable());

  size_t required_size = used_ + aligned_size;
  DCHECK_GT(required_size, reserved_) << "Should not have hit the slow path";
  // Start reserved_ at kInitialBufferSize and then double.
  // ShrinkToFit() can make this smaller afterwards.
  size_t new_size = reserved_ ? reserved_ : kInitialBufferSize;
  while (required_size > new_size) {
    new_size *= 2;
  }
  ReallocBuffer(new_size);
  DCHECK_LE(required_size, reserved_);

  return AllocatePaintOp(aligned_size);
}

void PaintOpBuffer::ShrinkToFit() {
  ReallocIfNeededToFit();
}

PaintOpBuffer::BufferDataPtr PaintOpBuffer::ReallocIfNeededToFit() {
  if (used_ == reserved_) {
    return nullptr;
  }
  if (!used_) {
    reserved_ = 0;
    return std::move(data_);
  }
  return ReallocBuffer(used_);
}

bool PaintOpBuffer::EqualsForTesting(const PaintOpBuffer& other) const {
  // Check status fields first, which is faster than checking equality of
  // paint operations. This doesn't need to be complete, and should not check
  // data buffer capacity related fields because they don't affect equality.
  if (op_count_ != other.op_count_ || used_ != other.used_ ||
      num_slow_paths_up_to_min_for_MSAA_ !=
          other.num_slow_paths_up_to_min_for_MSAA_ ||
      subrecord_op_count_ != other.subrecord_op_count_ ||
      has_draw_ops_ != other.has_draw_ops_ ||
      has_draw_text_ops_ != other.has_draw_text_ops_ ||
      has_effects_preventing_lcd_text_for_save_layer_alpha_ !=
          other.has_effects_preventing_lcd_text_for_save_layer_alpha_ ||
      has_non_aa_paint_ != other.has_non_aa_paint_ ||
      has_discardable_images_ != other.has_discardable_images_) {
    return false;
  }

  return base::ranges::equal(*this, other,
                             [](const PaintOp& a, const PaintOp& b) {
                               return a.EqualsForTesting(b);  // IN-TEST
                             });
}

bool PaintOpBuffer::NeedsAdditionalInvalidationForLCDText(
    const PaintOpBuffer& old_buffer) const {
  // We need this in addition to blink's raster invalidation because change of
  // has_effects_preventing_lcd_text_for_save_layer_alpha() can affect
  // all SaveLayerAlphaOps of the PaintOpBuffer, not just the area that the
  // changed effects affected.
  if (!has_draw_text_ops() || !has_save_layer_alpha_ops())
    return false;
  if (!old_buffer.has_draw_text_ops() || !old_buffer.has_save_layer_alpha_ops())
    return false;
  return has_effects_preventing_lcd_text_for_save_layer_alpha() !=
         old_buffer.has_effects_preventing_lcd_text_for_save_layer_alpha();
}

void PaintOpBuffer::UpdateSaveLayerBounds(size_t offset, const SkRect& bounds) {
  CHECK_LT(offset, used_);
  CHECK_LE(offset + sizeof(PaintOp), used_);

  auto* op = reinterpret_cast<PaintOp*>(data_.get() + offset);
  switch (op->GetType()) {
    case SaveLayerOp::kType:
      CHECK_LE(offset + sizeof(SaveLayerOp), used_);
      static_cast<SaveLayerOp*>(op)->bounds = bounds;
      break;
    case SaveLayerAlphaOp::kType:
      CHECK_LE(offset + sizeof(SaveLayerAlphaOp), used_);
      static_cast<SaveLayerAlphaOp*>(op)->bounds = bounds;
      break;
    default:
      NOTREACHED();
  }
}

PaintOpBuffer::Iterator PaintOpBuffer::begin() const {
  return Iterator(*this);
}

PaintOpBuffer::Iterator PaintOpBuffer::end() const {
  return Iterator(*this).end();
}

const PaintOp& PaintOpBuffer::GetOpAtForTesting(size_t index) const {
  for (const auto& op : *this) {
    if (!index) {
      return op;
    }
    --index;
  }
  NOTREACHED();
}

}  // namespace cc
