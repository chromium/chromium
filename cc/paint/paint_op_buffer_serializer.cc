// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer_serializer.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/scoped_raster_flags.h"
#include "ui/gfx/skia_util.h"

#include <utility>

namespace cc {
namespace {

class ScopedFlagsOverride {
 public:
  ScopedFlagsOverride(PaintOp::SerializeOptions* options,
                      const PaintFlags* flags)
      : options_(options) {
    options_->flags_to_serialize = flags;
  }
  ~ScopedFlagsOverride() { options_->flags_to_serialize = nullptr; }

 private:
  PaintOp::SerializeOptions* options_;
};

// Copied from viz::ClientResourceProvider.
SkSurfaceProps ComputeSurfaceProps(bool can_use_lcd_text) {
  uint32_t flags = 0;
  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
  if (can_use_lcd_text) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props =
        SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  }
  return surface_props;
}

PlaybackParams MakeParams(const SkCanvas* canvas) {
  // We don't use an ImageProvider here since the ops are played onto a no-draw
  // canvas for state tracking and don't need decoded images.
  return PlaybackParams(nullptr, canvas->getTotalMatrix());
}

// Use half of the max int as the extent for the SkNoDrawCanvas. The correct
// clip is applied to the canvas during serialization.
const int kMaxExtent = std::numeric_limits<int>::max() >> 1;

}  // namespace

PaintOpBufferSerializer::PaintOpBufferSerializer(
    SerializeCallback serialize_cb,
    ImageProvider* image_provider,
    TransferCacheSerializeHelper* transfer_cache,
    ClientPaintCache* paint_cache,
    SkStrikeServer* strike_server,
    sk_sp<SkColorSpace> color_space,
    bool can_use_lcd_text,
    bool context_supports_distance_field_text,
    int max_texture_size,
    size_t max_texture_bytes)
    : serialize_cb_(std::move(serialize_cb)),
      image_provider_(image_provider),
      transfer_cache_(transfer_cache),
      paint_cache_(paint_cache),
      strike_server_(strike_server),
      color_space_(color_space),
      can_use_lcd_text_(can_use_lcd_text),
      context_supports_distance_field_text_(
          context_supports_distance_field_text),
      max_texture_size_(max_texture_size),
      max_texture_bytes_(max_texture_bytes),
      text_blob_canvas_(kMaxExtent,
                        kMaxExtent,
                        ComputeSurfaceProps(can_use_lcd_text),
                        strike_server,
                        std::move(color_space),
                        context_supports_distance_field_text) {
  DCHECK(serialize_cb_);
}

PaintOpBufferSerializer::~PaintOpBufferSerializer() = default;

void PaintOpBufferSerializer::Serialize(const PaintOpBuffer* buffer,
                                        const std::vector<size_t>* offsets,
                                        const Preamble& preamble) {
  DCHECK(text_blob_canvas_.getTotalMatrix().isIdentity());
  static const int kInitialSaveCount = 1;
  DCHECK_EQ(kInitialSaveCount, text_blob_canvas_.getSaveCount());

  // These SerializeOptions and PlaybackParams use the initial (identity) canvas
  // matrix, as they are only used for serializing the preamble and the initial
  // save / final restore. SerializeBuffer will create its own SerializeOptions
  // and PlaybackParams based on the post-preamble canvas.
  PaintOp::SerializeOptions options = MakeSerializeOptions();
  PlaybackParams params = MakeParams(&text_blob_canvas_);

  Save(options, params);
  SerializePreamble(preamble, options, params);
  SerializeBuffer(buffer, offsets);
  RestoreToCount(kInitialSaveCount, options, params);
}

void PaintOpBufferSerializer::Serialize(const PaintOpBuffer* buffer) {
  DCHECK(text_blob_canvas_.getTotalMatrix().isIdentity());

  SerializeBuffer(buffer, nullptr);
}

void PaintOpBufferSerializer::Serialize(
    const PaintOpBuffer* buffer,
    const gfx::Rect& playback_rect,
    const gfx::SizeF& post_scale,
    const SkMatrix& post_matrix_for_analysis) {
  DCHECK(text_blob_canvas_.getTotalMatrix().isIdentity());

  PaintOp::SerializeOptions options = MakeSerializeOptions();
  PlaybackParams params = MakeParams(&text_blob_canvas_);

  // TODO(khushalsagar): remove this clip rect if it's not needed.
  if (!playback_rect.IsEmpty()) {
    ClipRectOp clip_op(gfx::RectToSkRect(playback_rect), SkClipOp::kIntersect,
                       false);
    SerializeOp(&clip_op, options, params);
  }

  if (post_scale.width() != 1.f || post_scale.height() != 1.f) {
    ScaleOp scale_op(post_scale.width(), post_scale.height());
    SerializeOp(&scale_op, options, params);
  }

  text_blob_canvas_.concat(post_matrix_for_analysis);
  SerializeBuffer(buffer, nullptr);
}

// This function needs to have the exact same behavior as
// RasterSource::ClearForOpaqueRaster.
void PaintOpBufferSerializer::ClearForOpaqueRaster(
    const Preamble& preamble,
    const PaintOp::SerializeOptions& options,
    const PlaybackParams& params) {
  // Clear opaque raster sources.  Opaque rasters sources guarantee that all
  // pixels inside the opaque region are painted.  However, due to scaling
  // it's possible that the last row and column might include pixels that
  // are not painted.  Because this raster source is required to be opaque,
  // we may need to do extra clearing outside of the clip.  This needs to
  // be done for both full and partial raster.

  // The last texel of this content is not guaranteed to be fully opaque, so
  // inset by one to generate the fully opaque coverage rect.  This rect is
  // in device space.
  SkIRect coverage_device_rect = SkIRect::MakeWH(
      preamble.content_size.width() - preamble.full_raster_rect.x() - 1,
      preamble.content_size.height() - preamble.full_raster_rect.y() - 1);

  // If not fully covered, we need to clear one texel inside the coverage
  // rect (because of blending during raster) and one texel outside the canvas
  // bitmap rect (because of bilinear filtering during draw).  See comments
  // in RasterSource.
  SkIRect device_column = SkIRect::MakeXYWH(coverage_device_rect.right(), 0, 2,
                                            coverage_device_rect.bottom());
  // row includes the corner, column excludes it.
  SkIRect device_row = SkIRect::MakeXYWH(0, coverage_device_rect.bottom(),
                                         coverage_device_rect.right() + 2, 2);

  bool right_edge =
      preamble.content_size.width() == preamble.playback_rect.right();
  bool bottom_edge =
      preamble.content_size.height() == preamble.playback_rect.bottom();

  // If the playback rect is touching either edge of the content rect
  // extend it by one pixel to include the extra texel outside the canvas
  // bitmap rect that was added to device column and row above.
  SkIRect playback_device_rect = SkIRect::MakeXYWH(
      preamble.playback_rect.x() - preamble.full_raster_rect.x(),
      preamble.playback_rect.y() - preamble.full_raster_rect.y(),
      preamble.playback_rect.width() + (right_edge ? 1 : 0),
      preamble.playback_rect.height() + (bottom_edge ? 1 : 0));

  // Intersect the device column and row with the playback rect and only
  // clear inside of that rect if needed.
  if (device_column.intersect(playback_device_rect)) {
    Save(options, params);
    ClipRectOp clip_op(SkRect::Make(device_column), SkClipOp::kIntersect,
                       false);
    SerializeOp(&clip_op, options, params);
    DrawColorOp clear_op(preamble.background_color, SkBlendMode::kSrc);
    SerializeOp(&clear_op, options, params);
    RestoreToCount(1, options, params);
  }
  if (device_row.intersect(playback_device_rect)) {
    Save(options, params);
    ClipRectOp clip_op(SkRect::Make(device_row), SkClipOp::kIntersect, false);
    SerializeOp(&clip_op, options, params);
    DrawColorOp clear_op(preamble.background_color, SkBlendMode::kSrc);
    SerializeOp(&clear_op, options, params);
    RestoreToCount(1, options, params);
  }
}

void PaintOpBufferSerializer::SerializePreamble(
    const Preamble& preamble,
    const PaintOp::SerializeOptions& options,
    const PlaybackParams& params) {
  DCHECK(preamble.full_raster_rect.Contains(preamble.playback_rect))
      << "full: " << preamble.full_raster_rect.ToString()
      << ", playback: " << preamble.playback_rect.ToString();

  bool is_partial_raster = preamble.full_raster_rect != preamble.playback_rect;
  if (!preamble.requires_clear) {
    ClearForOpaqueRaster(preamble, options, params);
  } else if (!is_partial_raster) {
    // If rastering the entire tile, clear to transparent pre-clip.  This is so
    // that any external texels outside of the playback rect also get cleared.
    // There's not enough information at this point to know if this texture is
    // being reused from another tile, so the external texels could have been
    // cleared to some wrong value.
    DrawColorOp clear(SK_ColorTRANSPARENT, SkBlendMode::kSrc);
    SerializeOp(&clear, options, params);
  }

  if (!preamble.full_raster_rect.OffsetFromOrigin().IsZero()) {
    TranslateOp translate_op(-preamble.full_raster_rect.x(),
                             -preamble.full_raster_rect.y());
    SerializeOp(&translate_op, options, params);
  }

  if (!preamble.playback_rect.IsEmpty()) {
    ClipRectOp clip_op(gfx::RectToSkRect(preamble.playback_rect),
                       SkClipOp::kIntersect, false);
    SerializeOp(&clip_op, options, params);
  }

  if (!preamble.post_translation.IsZero()) {
    TranslateOp translate_op(preamble.post_translation.x(),
                             preamble.post_translation.y());
    SerializeOp(&translate_op, options, params);
  }

  if (preamble.post_scale.width() != 1.f ||
      preamble.post_scale.height() != 1.f) {
    ScaleOp scale_op(preamble.post_scale.width(), preamble.post_scale.height());
    SerializeOp(&scale_op, options, params);
  }

  // If tile is transparent and this is partial raster, just clear the
  // section that is being rastered.  If this is opaque, trust the raster
  // to write all the pixels inside of the full_raster_rect.
  if (preamble.requires_clear && is_partial_raster) {
    DrawColorOp clear_op(SK_ColorTRANSPARENT, SkBlendMode::kSrc);
    SerializeOp(&clear_op, options, params);
  }
}

void PaintOpBufferSerializer::SerializeBuffer(
    const PaintOpBuffer* buffer,
    const std::vector<size_t>* offsets) {
  DCHECK(buffer);
  PaintOp::SerializeOptions options = MakeSerializeOptions();
  PlaybackParams params = MakeParams(&text_blob_canvas_);

  for (PaintOpBuffer::PlaybackFoldingIterator iter(buffer, offsets); iter;
       ++iter) {
    const PaintOp* op = *iter;

    // Skip ops outside the current clip if they have images. This saves
    // performing an unnecessary expensive decode.
    const bool skip_op = PaintOp::OpHasDiscardableImages(op) &&
                         PaintOp::QuickRejectDraw(op, &text_blob_canvas_);
    if (skip_op)
      continue;

    if (op->GetType() != PaintOpType::DrawRecord) {
      bool success = false;
      if (op->IsPaintOpWithFlags()) {
        success = SerializeOpWithFlags(static_cast<const PaintOpWithFlags*>(op),
                                       &options, params, iter.alpha());
      } else {
        success = SerializeOp(op, options, params);
      }

      if (!success)
        return;
      continue;
    }

    int save_count = text_blob_canvas_.getSaveCount();
    Save(options, params);
    SerializeBuffer(static_cast<const DrawRecordOp*>(op)->record.get(),
                    nullptr);
    RestoreToCount(save_count, options, params);
  }
}

bool PaintOpBufferSerializer::SerializeOpWithFlags(
    const PaintOpWithFlags* flags_op,
    PaintOp::SerializeOptions* options,
    const PlaybackParams& params,
    uint8_t alpha) {
  // We use a null |image_provider| here because images are decoded during
  // serialization.
  const ScopedRasterFlags scoped_flags(&flags_op->flags, nullptr,
                                       options->canvas->getTotalMatrix(),
                                       max_texture_size_, alpha);
  const PaintFlags* flags_to_serialize = scoped_flags.flags();
  if (!flags_to_serialize)
    return true;

  ScopedFlagsOverride override_flags(options, flags_to_serialize);
  return SerializeOp(flags_op, *options, params);
}

bool PaintOpBufferSerializer::SerializeOp(
    const PaintOp* op,
    const PaintOp::SerializeOptions& options,
    const PlaybackParams& params) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "PaintOpBufferSerializer::SerializeOp", "op",
               PaintOpTypeToString(op->GetType()));
  if (!valid_)
    return false;

  // Playback on analysis canvas first to make sure the canvas transform is set
  // correctly for analysis of records in filters.
  PlaybackOnAnalysisCanvas(op, options, params);

  size_t bytes = serialize_cb_.Run(op, options);
  if (!bytes) {
    valid_ = false;
    return false;
  }

  DCHECK_GE(bytes, 4u);
  DCHECK_EQ(bytes % PaintOpBuffer::PaintOpAlign, 0u);
  return true;
}

void PaintOpBufferSerializer::PlaybackOnAnalysisCanvas(
    const PaintOp* op,
    const PaintOp::SerializeOptions& options,
    const PlaybackParams& params) {
  // Only 2 types of ops need to played on the analysis canvas.
  // 1) Non-draw ops which affect the transform/clip state on the canvas, since
  //    we need the correct ctm at which text and images will be rasterized, and
  //    the clip rect so we can skip sending data for ops which will not be
  //    rasterized.
  // 2) DrawTextBlob ops since they need to be analyzed by the cache diff canvas
  //    to serialize/lock the requisite glyphs for this op.
  if (op->IsDrawOp() && op->GetType() != PaintOpType::DrawTextBlob)
    return;

  if (op->IsPaintOpWithFlags() && options.flags_to_serialize) {
    static_cast<const PaintOpWithFlags*>(op)->RasterWithFlags(
        &text_blob_canvas_, options.flags_to_serialize, params);
  } else {
    op->Raster(&text_blob_canvas_, params);
  }
}

void PaintOpBufferSerializer::Save(const PaintOp::SerializeOptions& options,
                                   const PlaybackParams& params) {
  SaveOp save_op;
  SerializeOp(&save_op, options, params);
}

void PaintOpBufferSerializer::RestoreToCount(
    int count,
    const PaintOp::SerializeOptions& options,
    const PlaybackParams& params) {
  RestoreOp restore_op;
  while (text_blob_canvas_.getSaveCount() > count) {
    if (!SerializeOp(&restore_op, options, params))
      return;
  }
}

PaintOp::SerializeOptions PaintOpBufferSerializer::MakeSerializeOptions() {
  return PaintOp::SerializeOptions(
      image_provider_, transfer_cache_, paint_cache_, &text_blob_canvas_,
      strike_server_, color_space_, can_use_lcd_text_,
      context_supports_distance_field_text_, max_texture_size_,
      max_texture_bytes_, text_blob_canvas_.getTotalMatrix());
}

SimpleBufferSerializer::SimpleBufferSerializer(
    void* memory,
    size_t size,
    ImageProvider* image_provider,
    TransferCacheSerializeHelper* transfer_cache,
    ClientPaintCache* paint_cache,
    SkStrikeServer* strike_server,
    sk_sp<SkColorSpace> color_space,
    bool can_use_lcd_text,
    bool context_supports_distance_field_text,
    int max_texture_size,
    size_t max_texture_bytes)
    : PaintOpBufferSerializer(
          base::BindRepeating(&SimpleBufferSerializer::SerializeToMemory,
                              base::Unretained(this)),
          image_provider,
          transfer_cache,
          paint_cache,
          strike_server,
          std::move(color_space),
          can_use_lcd_text,
          context_supports_distance_field_text,
          max_texture_size,
          max_texture_bytes),
      memory_(memory),
      total_(size) {}

SimpleBufferSerializer::~SimpleBufferSerializer() = default;

size_t SimpleBufferSerializer::SerializeToMemory(
    const PaintOp* op,
    const PaintOp::SerializeOptions& options) {
  if (written_ == total_)
    return 0u;

  size_t bytes = op->Serialize(static_cast<char*>(memory_) + written_,
                               total_ - written_, options);
  if (!bytes)
    return 0u;

  written_ += bytes;
  DCHECK_GE(total_, written_);
  return bytes;
}

}  // namespace cc
