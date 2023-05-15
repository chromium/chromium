// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer_serializer.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/clear_for_opaque_raster.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/paint/scoped_raster_flags.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {
namespace {

PlaybackParams MakeParams(const SkCanvas* canvas) {
  // We don't use an ImageProvider here since the ops are played onto a no-draw
  // canvas for state tracking and don't need decoded images.
  PlaybackParams params(nullptr, canvas->getLocalToDevice());
  params.is_analyzing = true;
  return params;
}

std::unique_ptr<SkCanvas> MakeAnalysisCanvas(
    const PaintOp::SerializeOptions& options) {
  // Use half of the max int as the extent for the SkNoDrawCanvas. The correct
  // clip is applied to the canvas during serialization.
  const int kMaxExtent = std::numeric_limits<int>::max() >> 1;

  return options.strike_server
             ? options.strike_server->makeAnalysisCanvas(
                   kMaxExtent, kMaxExtent,
                   skia::LegacyDisplayGlobals::ComputeSurfaceProps(
                       options.can_use_lcd_text),
                   options.color_space,
                   options.context_supports_distance_field_text)
             : std::make_unique<SkNoDrawCanvas>(kMaxExtent, kMaxExtent);
}

}  // namespace

PaintOpBufferSerializer::PaintOpBufferSerializer(
    SerializeCallback serialize_cb,
    void* callback_data,
    const PaintOp::SerializeOptions& options)
    : serialize_cb_(serialize_cb),
      callback_data_(callback_data),
      options_(options) {
  DCHECK(serialize_cb_);
}

PaintOpBufferSerializer::~PaintOpBufferSerializer() = default;

void PaintOpBufferSerializer::Serialize(const PaintOpBuffer& buffer,
                                        const std::vector<size_t>* offsets,
                                        const Preamble& preamble) {
  TRACE_EVENT_BEGIN1("cc", "PaintOpBufferSerializer::Serialize",
                     "total_op_count", buffer.total_op_count());
  DCHECK_EQ(serialized_op_count_, 0u);

  std::unique_ptr<SkCanvas> canvas = MakeAnalysisCanvas(options_);

  // These PlaybackParams use the initial (identity) canvas matrix, as they are
  // only used for serializing the preamble and the initial save / final restore
  // SerializeBuffer will create its own PlaybackParams based on the
  // post-preamble canvas.
  PlaybackParams params = MakeParams(canvas.get());

  int save_count = canvas->getSaveCount();
  Save(canvas.get(), params);
  SerializePreamble(canvas.get(), preamble, params);
  SerializeBuffer(canvas.get(), buffer, offsets);
  RestoreToCount(canvas.get(), save_count, params);
  TRACE_EVENT_END1("cc", "PaintOpBufferSerializer::Serialize",
                   "serialized_op_count", serialized_op_count_);
}

void PaintOpBufferSerializer::Serialize(const PaintOpBuffer& buffer) {
  std::unique_ptr<SkCanvas> canvas = MakeAnalysisCanvas(options_);
  SerializeBuffer(canvas.get(), buffer, nullptr);
}

void PaintOpBufferSerializer::Serialize(const PaintOpBuffer& buffer,
                                        const gfx::Rect& playback_rect,
                                        const gfx::SizeF& post_scale) {
  std::unique_ptr<SkCanvas> canvas = MakeAnalysisCanvas(options_);

  PlaybackParams params = MakeParams(canvas.get());

  // TODO(khushalsagar): remove this clip rect if it's not needed.
  if (!playback_rect.IsEmpty()) {
    ClipRectOp clip_op(gfx::RectToSkRect(playback_rect), SkClipOp::kIntersect,
                       false);
    SerializeOp(canvas.get(), clip_op, nullptr, params);
  }

  if (post_scale.width() != 1.f || post_scale.height() != 1.f) {
    ScaleOp scale_op(post_scale.width(), post_scale.height());
    SerializeOp(canvas.get(), scale_op, nullptr, params);
  }

  SerializeBuffer(canvas.get(), buffer, nullptr);
}

// This function needs to have the exact same behavior as
// RasterSource::ClearForOpaqueRaster.
void PaintOpBufferSerializer::ClearForOpaqueRaster(
    SkCanvas* canvas,
    const Preamble& preamble,
    const PlaybackParams& params) {
  gfx::Rect outer_rect;
  gfx::Rect inner_rect;
  if (!CalculateClearForOpaqueRasterRects(
          preamble.post_translation, preamble.post_scale, preamble.content_size,
          preamble.full_raster_rect, preamble.playback_rect, outer_rect,
          inner_rect))
    return;

  Save(canvas, params);
  ClipRectOp outer_clip_op(gfx::RectToSkRect(outer_rect), SkClipOp::kIntersect,
                           false);
  SerializeOp(canvas, outer_clip_op, nullptr, params);
  if (!inner_rect.IsEmpty()) {
    ClipRectOp inner_clip_op(gfx::RectToSkRect(inner_rect),
                             SkClipOp::kDifference, false);
    SerializeOp(canvas, inner_clip_op, nullptr, params);
  }
  DrawColorOp clear_op(preamble.background_color, SkBlendMode::kSrc);
  SerializeOp(canvas, clear_op, nullptr, params);
  RestoreToCount(canvas, 1, params);
}

void PaintOpBufferSerializer::SerializePreamble(SkCanvas* canvas,
                                                const Preamble& preamble,
                                                const PlaybackParams& params) {
  DCHECK(preamble.full_raster_rect.Contains(preamble.playback_rect))
      << "full: " << preamble.full_raster_rect.ToString()
      << ", playback: " << preamble.playback_rect.ToString();

  // NOTE: The following code should be kept consistent with
  // RasterSource::PlaybackToCanvas().
  bool is_partial_raster = preamble.full_raster_rect != preamble.playback_rect;
  if (!preamble.requires_clear) {
    ClearForOpaqueRaster(canvas, preamble, params);
  } else if (!is_partial_raster) {
    // If rastering the entire tile, clear to transparent pre-clip.  This is so
    // that any external texels outside of the playback rect also get cleared.
    // There's not enough information at this point to know if this texture is
    // being reused from another tile, so the external texels could have been
    // cleared to some wrong value.
    DrawColorOp clear(SkColors::kTransparent, SkBlendMode::kSrc);
    SerializeOp(canvas, clear, nullptr, params);
  }

  if (!preamble.full_raster_rect.OffsetFromOrigin().IsZero()) {
    TranslateOp translate_op(-preamble.full_raster_rect.x(),
                             -preamble.full_raster_rect.y());
    SerializeOp(canvas, translate_op, nullptr, params);
  }

  if (!preamble.playback_rect.IsEmpty()) {
    ClipRectOp clip_op(gfx::RectToSkRect(preamble.playback_rect),
                       SkClipOp::kIntersect, false);
    SerializeOp(canvas, clip_op, nullptr, params);
  }

  if (!preamble.post_translation.IsZero()) {
    TranslateOp translate_op(preamble.post_translation.x(),
                             preamble.post_translation.y());
    SerializeOp(canvas, translate_op, nullptr, params);
  }

  if (preamble.post_scale.x() != 1.f || preamble.post_scale.y() != 1.f) {
    ScaleOp scale_op(preamble.post_scale.x(), preamble.post_scale.y());
    SerializeOp(canvas, scale_op, nullptr, params);
  }

  // If tile is transparent and this is partial raster, just clear the
  // section that is being rastered.  If this is opaque, trust the raster
  // to write all the pixels inside of the full_raster_rect.
  if (preamble.requires_clear && is_partial_raster) {
    DrawColorOp clear_op(SkColors::kTransparent, SkBlendMode::kSrc);
    SerializeOp(canvas, clear_op, nullptr, params);
  }
}

bool PaintOpBufferSerializer::WillSerializeNextOp(const PaintOp& op,
                                                  SkCanvas* canvas,
                                                  const PlaybackParams& params,
                                                  float alpha) {
  // Skip ops outside the current clip if they have images. This saves
  // performing an unnecessary expensive decode.
  bool skip_op = PaintOp::OpHasDiscardableImages(op) &&
                 PaintOp::QuickRejectDraw(op, canvas);
  // Skip text ops if there is no SkStrikeServer.
  skip_op |=
      op.GetType() == PaintOpType::DrawTextBlob && !options_.strike_server;
  if (skip_op)
    return true;

  if (op.GetType() == PaintOpType::DrawRecord) {
    int save_count = canvas->getSaveCount();
    Save(canvas, params);
    SerializeBuffer(
        canvas, static_cast<const DrawRecordOp&>(op).record.buffer(), nullptr);
    RestoreToCount(canvas, save_count, params);
    return true;
  }

  if (op.GetType() == PaintOpType::DrawImageRect &&
      static_cast<const DrawImageRectOp&>(op).image.IsPaintWorklet()) {
    DCHECK(options_.image_provider);
    const DrawImageRectOp& draw_op = static_cast<const DrawImageRectOp&>(op);
    ImageProvider::ScopedResult result =
        options_.image_provider->GetRasterContent(DrawImage(draw_op.image));
    if (!result || !result.has_paint_record()) {
      return true;
    }

    int save_count = canvas->getSaveCount();
    Save(canvas, params);
    // The following ops are copying the canvas's ops from
    // DrawImageRectOp::RasterWithFlags.
    SkM44 trans = SkM44(SkMatrix::RectToRect(draw_op.src, draw_op.dst));
    ConcatOp concat_op(trans);
    bool success = SerializeOp(canvas, concat_op, nullptr, params);

    if (!success)
      return false;

    ClipRectOp clip_rect_op(draw_op.src, SkClipOp::kIntersect, false);
    success = SerializeOp(canvas, clip_rect_op, nullptr, params);
    if (!success)
      return false;

    // In DrawImageRectOp::RasterWithFlags, the save layer uses the
    // flags_to_serialize or default (PaintFlags()) flags. At this point in the
    // serialization, flags_to_serialize is always null as well.
    SaveLayerOp save_layer_op(draw_op.src, PaintFlags());
    success = SerializeOpWithFlags(canvas, save_layer_op, params, 255);
    if (!success)
      return false;

    SerializeBuffer(canvas, result.ReleaseAsRecord().buffer(), nullptr);
    RestoreToCount(canvas, save_count, params);
    return true;
  } else {
    if (op.IsPaintOpWithFlags()) {
      return SerializeOpWithFlags(
          canvas, static_cast<const PaintOpWithFlags&>(op), params, alpha);
    } else {
      return SerializeOp(canvas, op, nullptr, params);
    }
  }
}

void PaintOpBufferSerializer::SerializeBuffer(
    SkCanvas* canvas,
    const PaintOpBuffer& buffer,
    const std::vector<size_t>* offsets) {
  // This updates the original_ctm to reflect the canvas transformation at
  // start of this call to SerializeBuffer.
  PlaybackParams params = MakeParams(canvas);

  for (PaintOpBuffer::PlaybackFoldingIterator iter(buffer, offsets); iter;
       ++iter) {
    const PaintOp& op = *iter;
    if (!WillSerializeNextOp(op, canvas, params, iter.alpha())) {
      return;
    }
  }
}

bool PaintOpBufferSerializer::SerializeOpWithFlags(
    SkCanvas* canvas,
    const PaintOpWithFlags& flags_op,
    const PlaybackParams& params,
    float alpha) {
  // We use a null |image_provider| here because images are decoded during
  // serialization.
  const ScopedRasterFlags scoped_flags(&flags_op.flags, nullptr,
                                       canvas->getTotalMatrix(),
                                       options_.max_texture_size, alpha);
  const PaintFlags* flags_to_serialize = scoped_flags.flags();
  if (!flags_to_serialize)
    return true;

  return SerializeOp(canvas, flags_op, flags_to_serialize, params);
}

bool PaintOpBufferSerializer::SerializeOp(SkCanvas* canvas,
                                          const PaintOp& op,
                                          const PaintFlags* flags_to_serialize,
                                          const PlaybackParams& params) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "PaintOpBufferSerializer::SerializeOp", "op",
               PaintOpTypeToString(op.GetType()));
  if (!valid_)
    return false;

  // Playback on analysis canvas first to make sure the canvas transform is set
  // correctly for analysis of records in filters.
  PlaybackOnAnalysisCanvas(canvas, op, flags_to_serialize, params);

  size_t bytes = serialize_cb_(callback_data_, op, options_, flags_to_serialize,
                               canvas->getLocalToDevice(), params.original_ctm);
  if (!bytes) {
    valid_ = false;
    return false;
  }

  ++serialized_op_count_;
  DCHECK_GE(bytes, PaintOpWriter::kHeaderBytes);
  return true;
}

void PaintOpBufferSerializer::PlaybackOnAnalysisCanvas(
    SkCanvas* canvas,
    const PaintOp& op,
    const PaintFlags* flags_to_serialize,
    const PlaybackParams& params) {
  // Only 2 types of ops need to played on the analysis canvas.
  // 1) Non-draw ops which affect the transform/clip state on the canvas, since
  //    we need the correct ctm at which text and images will be rasterized, and
  //    the clip rect so we can skip sending data for ops which will not be
  //    rasterized.
  // 2) DrawTextBlob ops since they need to be analyzed by the cache diff canvas
  //    to serialize/lock the requisite glyphs for this op.
  if (op.IsDrawOp() && op.GetType() != PaintOpType::DrawTextBlob)
    return;

  if (op.IsPaintOpWithFlags() && flags_to_serialize) {
    static_cast<const PaintOpWithFlags&>(op).RasterWithFlags(
        canvas, flags_to_serialize, params);
  } else {
    op.Raster(canvas, params);
  }
}

void PaintOpBufferSerializer::Save(SkCanvas* canvas,
                                   const PlaybackParams& params) {
  SaveOp save_op;
  SerializeOp(canvas, save_op, nullptr, params);
}

void PaintOpBufferSerializer::RestoreToCount(SkCanvas* canvas,
                                             int count,
                                             const PlaybackParams& params) {
  RestoreOp restore_op;
  while (canvas->getSaveCount() > count) {
    if (!SerializeOp(canvas, restore_op, nullptr, params))
      return;
  }
}

SimpleBufferSerializer::SimpleBufferSerializer(
    void* memory,
    size_t size,
    const PaintOp::SerializeOptions& options)
    : PaintOpBufferSerializer(&SimpleBufferSerializer::SerializeToMemory,
                              this,
                              options),
      memory_(memory),
      total_(size) {}

SimpleBufferSerializer::~SimpleBufferSerializer() = default;

size_t SimpleBufferSerializer::SerializeToMemoryImpl(
    const PaintOp& op,
    const PaintOp::SerializeOptions& options,
    const PaintFlags* flags_to_serialize,
    const SkM44& current_ctm,
    const SkM44& original_ctm) {
  if (written_ == total_)
    return 0u;

  size_t bytes =
      op.Serialize(static_cast<char*>(memory_) + written_, total_ - written_,
                   options, flags_to_serialize, current_ctm, original_ctm);
  if (!bytes)
    return 0u;

  written_ += bytes;
  DCHECK_GE(total_, written_);
  return bytes;
}

}  // namespace cc
