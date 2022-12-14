// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"

#include <algorithm>
#include <utility>

#include "base/notreached.h"
#include "base/types/optional_util.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/scoped_raster_flags.h"
#include "cc/paint/skottie_serialization_history.h"
#include "third_party/skia/include/gpu/GrRecordingContext.h"

namespace cc {

PlaybackParams::PlaybackParams(ImageProvider* image_provider)
    : PlaybackParams(image_provider, SkM44()) {}

PlaybackParams::PlaybackParams(ImageProvider* image_provider,
                               const SkM44& original_ctm,
                               CustomDataRasterCallback custom_callback,
                               DidDrawOpCallback did_draw_op_callback,
                               ConvertOpCallback convert_op_callback)
    : image_provider(image_provider),
      original_ctm(original_ctm),
      custom_callback(custom_callback),
      did_draw_op_callback(std::move(did_draw_op_callback)),
      convert_op_callback(std::move(convert_op_callback)) {}

PlaybackParams::~PlaybackParams() = default;

PlaybackParams::PlaybackParams(const PlaybackParams& other) = default;
PlaybackParams& PlaybackParams::operator=(const PlaybackParams& other) =
    default;

PaintOpBuffer::SerializeOptions::SerializeOptions(
    ImageProvider* image_provider,
    TransferCacheSerializeHelper* transfer_cache,
    ClientPaintCache* paint_cache,
    SkStrikeServer* strike_server,
    sk_sp<SkColorSpace> color_space,
    SkottieSerializationHistory* skottie_serialization_history,
    bool can_use_lcd_text,
    bool context_supports_distance_field_text,
    int max_texture_size)
    : image_provider(image_provider),
      transfer_cache(transfer_cache),
      paint_cache(paint_cache),
      strike_server(strike_server),
      color_space(std::move(color_space)),
      skottie_serialization_history(skottie_serialization_history),
      can_use_lcd_text(can_use_lcd_text),
      context_supports_distance_field_text(
          context_supports_distance_field_text),
      max_texture_size(max_texture_size) {}

PaintOpBuffer::SerializeOptions::SerializeOptions() = default;
PaintOpBuffer::SerializeOptions::SerializeOptions(const SerializeOptions&) =
    default;
PaintOpBuffer::SerializeOptions& PaintOpBuffer::SerializeOptions::operator=(
    const SerializeOptions&) = default;
PaintOpBuffer::SerializeOptions::~SerializeOptions() = default;

PaintOpBuffer::DeserializeOptions::DeserializeOptions(
    TransferCacheDeserializeHelper* transfer_cache,
    ServicePaintCache* paint_cache,
    SkStrikeClient* strike_client,
    std::vector<uint8_t>* scratch_buffer,
    bool is_privileged,
    SharedImageProvider* shared_image_provider)
    : transfer_cache(transfer_cache),
      paint_cache(paint_cache),
      strike_client(strike_client),
      scratch_buffer(scratch_buffer),
      is_privileged(is_privileged),
      shared_image_provider(shared_image_provider) {
  DCHECK(scratch_buffer);
}

PaintOpBuffer::PaintOpBuffer()
    : has_non_aa_paint_(false),
      has_discardable_images_(false),
      has_draw_ops_(false),
      has_draw_text_ops_(false),
      has_save_layer_ops_(false),
      has_save_layer_alpha_ops_(false),
      has_effects_preventing_lcd_text_for_save_layer_alpha_(false) {}

PaintOpBuffer::PaintOpBuffer(PaintOpBuffer&& other) {
  *this = std::move(other);
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
  has_discardable_images_ = other.has_discardable_images_;
  has_draw_ops_ = other.has_draw_ops_;
  has_draw_text_ops_ = other.has_draw_text_ops_;
  has_save_layer_ops_ = other.has_save_layer_ops_;
  has_save_layer_alpha_ops_ = other.has_save_layer_alpha_ops_;
  has_effects_preventing_lcd_text_for_save_layer_alpha_ =
      other.has_effects_preventing_lcd_text_for_save_layer_alpha_;

  // Make sure the other pob can destruct safely or is ready for reuse.
  other.reserved_ = 0;
  other.ResetRetainingBuffer();
  return *this;
}

void PaintOpBuffer::DestroyOps() {
  if (data_) {
    for (size_t offset = 0; offset < used_;) {
      auto* op = reinterpret_cast<PaintOp*>(data_.get() + offset);
      offset += op->skip;
      op->DestroyThis();
    }
  }
}

void PaintOpBuffer::Reset() {
  DestroyOps();
  // Leave data_ allocated, reserved_ unchanged. ShrinkToFit() will take care
  // of that if called.
  ResetRetainingBuffer();
}

void PaintOpBuffer::ResetRetainingBuffer() {
  used_ = 0;
  op_count_ = 0;
  num_slow_paths_up_to_min_for_MSAA_ = 0;
  has_non_aa_paint_ = false;
  subrecord_bytes_used_ = 0;
  subrecord_op_count_ = 0;
  has_discardable_images_ = false;
  has_draw_ops_ = false;
  has_draw_text_ops_ = false;
  has_save_layer_ops_ = false;
  has_save_layer_alpha_ops_ = false;
  has_effects_preventing_lcd_text_for_save_layer_alpha_ = false;
}

void PaintOpBuffer::Playback(SkCanvas* canvas) const {
  Playback(canvas, PlaybackParams(nullptr), nullptr);
}

void PaintOpBuffer::Playback(SkCanvas* canvas,
                             const PlaybackParams& params) const {
  Playback(canvas, params, nullptr);
}

sk_sp<PaintRecord> PaintOpBuffer::MoveRetainingBufferIfPossible() {
  const size_t old_reserved = reserved_;
  sk_sp<PaintRecord> result = sk_make_sp<PaintRecord>(std::move(*this));
  if (BufferDataPtr old_data = result->ReallocIfNeededToFit()) {
    // Reuse the original buffer for future recording.
    data_ = std::move(old_data);
    reserved_ = old_reserved;
  }
  return result;
}

void PaintOpBuffer::Playback(SkCanvas* canvas,
                             const PlaybackParams& params,
                             const std::vector<size_t>* offsets) const {
  if (!op_count_)
    return;
  if (offsets && offsets->empty())
    return;
  // Prevent PaintOpBuffers from having side effects back into the canvas.
  SkAutoCanvasRestore save_restore(canvas, true);

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
  PlaybackParams new_params(params.image_provider, canvas->getLocalToDevice(),
                            params.custom_callback, params.did_draw_op_callback,
                            params.convert_op_callback);
  new_params.save_layer_alpha_should_preserve_lcd_text =
      save_layer_alpha_should_preserve_lcd_text;
  new_params.is_analyzing = params.is_analyzing;
  for (PlaybackFoldingIterator iter(this, offsets); iter; ++iter) {
    const PaintOp* op = iter.get();
    if (params.convert_op_callback) {
      op = params.convert_op_callback.Run(*op);
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
      const auto& flags_op = static_cast<const PaintOpWithFlags&>(*op);
      auto* context = canvas->recordingContext();
      const ScopedRasterFlags scoped_flags(
          &flags_op.flags, new_params.image_provider, canvas->getTotalMatrix(),
          context ? context->maxTextureSize() : 0, iter.alpha() / 255.0f);
      if (const auto* raster_flags = scoped_flags.flags())
        flags_op.RasterWithFlags(canvas, raster_flags, new_params);
    } else {
      DCHECK_EQ(iter.alpha(), 255);
      op->Raster(canvas, new_params);
    }

    if (!new_params.did_draw_op_callback.is_null())
      new_params.did_draw_op_callback.Run();
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
  std::unique_ptr<char, base::AlignedFreeDeleter> new_data(
      static_cast<char*>(base::AlignedAlloc(new_size, kPaintOpAlign)));
  if (data_)
    memcpy(new_data.get(), data_.get(), used_);
  BufferDataPtr old_data = std::move(data_);
  data_ = std::move(new_data);
  reserved_ = new_size;
  return old_data;
}

void* PaintOpBuffer::AllocatePaintOp(size_t skip) {
  DCHECK_LT(skip, PaintOp::kMaxSkip);
  if (used_ + skip > reserved_) {
    // Start reserved_ at kInitialBufferSize and then double.
    // ShrinkToFit() can make this smaller afterwards.
    size_t new_size = reserved_ ? reserved_ : kInitialBufferSize;
    while (used_ + skip > new_size)
      new_size *= 2;
    ReallocBuffer(new_size);
  }
  DCHECK_LE(used_ + skip, reserved_);

  void* op = data_.get() + used_;
  used_ += skip;
  op_count_++;
  return op;
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

bool PaintOpBuffer::operator==(const PaintOpBuffer& other) const {
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

  Iterator left_iter(this);
  Iterator right_iter(&other);

  for (; left_iter != left_iter.end(); ++left_iter, ++right_iter) {
    if (*left_iter != *right_iter)
      return false;
  }

  DCHECK(left_iter == left_iter.end());
  DCHECK(right_iter == right_iter.end());
  return true;
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

const PaintOp* PaintOpBuffer::GetOpAtForTesting(size_t index,
                                                PaintOpType type) const {
  size_t i = 0;
  for (const auto& op : *this) {
    if (i == index) {
      return op.GetType() == type ? &op : nullptr;
    }
    i++;
  }
  return nullptr;
}

PaintOpBuffer::Iterator PaintOpBuffer::begin() const {
  return Iterator(this);
}

PaintOpBuffer::Iterator PaintOpBuffer::end() const {
  return Iterator(this).end();
}

}  // namespace cc
