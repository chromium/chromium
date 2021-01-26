// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/solid_color_analyzer.h"

#include <cmath>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {
namespace {

SkColor DoSrcOverAlphaBlend(SkColor src, SkColor dst) {
  if (SkColorGetA(src) == 0)
    return dst;
  if (SkColorGetA(src) == 255)
    return src;

  // Note: using alpha blending formulas adapted from
  // https://en.wikipedia.org/wiki/Alpha_compositing:
  //
  // outA = srcA + dstA * (1 - srcA)
  // outRGB = srcRGB * (srcA / outA) + dstRGB * [dstA * (1 - srcA) / outA]
  const float src_alpha = SkColorGetA(src) / 255.0f;
  const float src_alpha_complement = (255.0f - SkColorGetA(src)) / 255.0f;
  const float dst_alpha = SkColorGetA(dst) / 255.0f;
  const float out_alpha = src_alpha + dst_alpha * src_alpha_complement;
  if (out_alpha == 0.0f)
    return SK_ColorTRANSPARENT;

  const float inverse_out_alpha = 1.0f / out_alpha;
  const float src_weight = src_alpha * inverse_out_alpha;
  const float dst_weight = dst_alpha * src_alpha_complement * inverse_out_alpha;
  const float out_red =
      (SkColorGetR(src) * src_weight + SkColorGetR(dst) * dst_weight);
  const float out_green =
      (SkColorGetG(src) * src_weight + SkColorGetG(dst) * dst_weight);
  const float out_blue =
      (SkColorGetB(src) * src_weight + SkColorGetB(dst) * dst_weight);

  return SkColorSetARGB(static_cast<U8CPU>(std::floor(out_alpha * 255.0f)),
                        static_cast<U8CPU>(std::floor(out_red)),
                        static_cast<U8CPU>(std::floor(out_green)),
                        static_cast<U8CPU>(std::floor(out_blue)));
}

bool ActsLikeClear(SkBlendMode mode, unsigned src_alpha) {
  switch (mode) {
    case SkBlendMode::kClear:
      return true;
    case SkBlendMode::kSrc:
    case SkBlendMode::kSrcIn:
    case SkBlendMode::kDstIn:
    case SkBlendMode::kSrcOut:
    case SkBlendMode::kDstATop:
      return src_alpha == 0;
    case SkBlendMode::kDstOut:
      return src_alpha == 0xFF;
    default:
      return false;
  }
}

bool IsSolidColorBlendMode(SkBlendMode blendmode) {
  return blendmode == SkBlendMode::kSrc || blendmode == SkBlendMode::kSrcOver;
}

bool IsSolidColorPaint(const PaintFlags& flags) {
  SkBlendMode blendmode = flags.getBlendMode();

  // Paint is solid color if the following holds:
  // - Style is fill, and there are no special effects.
  // - Blend mode is either kSrc or kSrcOver.
  bool is_solid_color =
      IsSolidColorBlendMode(blendmode) && !flags.HasShader() &&
      !flags.getLooper() && !flags.getMaskFilter() && !flags.getColorFilter() &&
      !flags.getImageFilter() && flags.getStyle() == PaintFlags::kFill_Style;

#if defined(OS_MAC)
  // Additionally, on Mac, we require that the color is opaque due to
  // https://crbug.com/922899.
  // TODO(andrescj): remove this condition once that bug is fixed.
  is_solid_color = (is_solid_color && SkColorGetA(flags.getColor()) == 255);
#endif  // OS_MAC

  return is_solid_color;
}

// Returns true if the specified |drawn_shape| will cover the entire canvas
// and that the canvas is not clipped (i.e. it covers ALL of the canvas).
// We expect this method to return false most of the time so we take
// conservative early-outs when possible.
template <typename T>
bool IsFullQuad(const SkCanvas& canvas, const T& drawn_shape) {
  if (!canvas.isClipRect())
    return false;

  SkIRect clip_bounds;
  if (!canvas.getDeviceClipBounds(&clip_bounds))
    return false;

  // if the clip is smaller than the canvas, we're partly clipped, so abort.
  if (!clip_bounds.contains(SkIRect::MakeSize(canvas.getBaseLayerSize())))
    return false;

  const SkM44& matrix = canvas.getLocalToDevice();
  // If the transform results in a non-axis aligned rectangle, then be
  // conservative and return false.
  if (!MathUtil::SkM44Preserves2DAxisAlignment(matrix))
    return false;

  SkM44 inverse;
  if (!matrix.invert(&inverse))
    return false;

  // Check that the drawn shape contains the canvas bounds when those bounds
  // are transformed into the shape's coordinate space. Since we know the
  // transform is axis aligned we only need to test two corners.
  SkV4 upper_left = inverse.map(clip_bounds.left(), clip_bounds.top(), 0, 1);
  SkV4 lower_right =
      inverse.map(clip_bounds.right(), clip_bounds.bottom(), 0, 1);
  SkRect transformed_clip_bounds = SkRect::MakeLTRB(
      upper_left.x, upper_left.y, lower_right.x, lower_right.y);
  return drawn_shape.contains(transformed_clip_bounds);
}

void CalculateSolidColor(SkColor src_color,
                         SkBlendMode blendmode,
                         SkColor* dst_color,
                         bool* is_solid_color) {
  if (blendmode == SkBlendMode::kSrc) {
    // In the Src mode, we don't have to worry about what's in the canvas
    // because we'll replace it with |src_color|.
    *dst_color = src_color;
    *is_solid_color = true;
  } else {
    DCHECK_EQ(SkBlendMode::kSrcOver, blendmode);

    // When using the SrcOver mode, we must ensure that either a) we're
    // completely occluding what's in the canvas with an opaque color, or
    // b) whatever is in the canvas is already a solid color.
    if (SkColorGetA(src_color) == 255 || *is_solid_color) {
      *dst_color = DoSrcOverAlphaBlend(src_color, *dst_color);
      *is_solid_color = true;
    }
  }
}

void CheckIfSolidColor(const SkCanvas& canvas,
                       SkColor color,
                       SkBlendMode blendmode,
                       bool* is_solid_color,
                       bool* is_transparent,
                       SkColor* out_color) {
  SkRect rect;
  if (!canvas.getLocalClipBounds(&rect)) {
    *is_transparent = false;
    *is_solid_color = false;
    return;
  }

  bool does_cover_canvas = IsFullQuad(canvas, rect);
  uint8_t alpha = SkColorGetA(color);
  if (does_cover_canvas && ActsLikeClear(blendmode, alpha))
    *is_transparent = true;
  else if (alpha != 0 || blendmode != SkBlendMode::kSrc)
    *is_transparent = false;

  bool solid_color_candidate =
      does_cover_canvas && IsSolidColorBlendMode(blendmode);

#if defined(OS_MAC)
  // Additionally, on Mac, we require that the color is opaque due to
  // https://crbug.com/922899.
  // TODO(andrescj): remove this condition once that bug is fixed.
  solid_color_candidate = (solid_color_candidate && alpha == 255);
#endif  // OS_MAC

  if (solid_color_candidate) {
    CalculateSolidColor(color /* src_color */, blendmode,
                        out_color /* dst_color */, is_solid_color);
  } else {
    *is_solid_color = false;
  }
}

template <typename T>
void CheckIfSolidShape(const SkCanvas& canvas,
                       const T& shape,
                       const PaintFlags& flags,
                       bool* is_solid_color,
                       bool* is_transparent,
                       SkColor* color) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SolidColorAnalyzer::CheckIfSolidShape");
  if (flags.nothingToDraw())
    return;

  bool does_cover_canvas = IsFullQuad(canvas, shape);
  SkBlendMode blendmode = flags.getBlendMode();
  if (does_cover_canvas && ActsLikeClear(blendmode, flags.getAlpha()))
    *is_transparent = true;
  else if (flags.getAlpha() != 0 || blendmode != SkBlendMode::kSrc)
    *is_transparent = false;

  if (does_cover_canvas && IsSolidColorPaint(flags)) {
    CalculateSolidColor(flags.getColor() /* src_color */, flags.getBlendMode(),
                        color /* dst_color */, is_solid_color);
  } else {
    *is_solid_color = false;
  }
}

bool CheckIfRRectClipCoversCanvas(const SkCanvas& canvas,
                                  const SkRRect& rrect) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SolidColorAnalyzer::CheckIfRRectClipCoversCanvas");
  return IsFullQuad(canvas, rrect);
}

}  // namespace

base::Optional<SkColor> SolidColorAnalyzer::DetermineIfSolidColor(
    const PaintOpBuffer* buffer,
    const gfx::Rect& rect,
    int max_ops_to_analyze,
    const std::vector<size_t>* offsets) {
  if (buffer->size() == 0 || (offsets && offsets->empty()))
    return SK_ColorTRANSPARENT;

  bool is_solid = true;
  bool is_transparent = true;
  SkColor color = SK_ColorTRANSPARENT;

  struct Frame {
    Frame(PaintOpBuffer::CompositeIterator iter,
          const SkM44& original_ctm,
          int save_count)
        : iter(iter), original_ctm(original_ctm), save_count(save_count) {}

    PaintOpBuffer::CompositeIterator iter;
    const SkM44 original_ctm;
    int save_count = 0;
  };

  SkNoDrawCanvas canvas(rect.width(), rect.height());
  canvas.translate(-rect.x(), -rect.y());
  canvas.clipRect(gfx::RectToSkRect(rect), SkClipOp::kIntersect, false);

  std::vector<Frame> stack;
  // We expect to see at least one DrawRecordOp because of the way items are
  // constructed. Reserve this to 2, and go from there.
  stack.reserve(2);
  stack.emplace_back(PaintOpBuffer::CompositeIterator(buffer, offsets),
                     canvas.getLocalToDevice(), canvas.getSaveCount());

  int num_draw_ops = 0;
  while (!stack.empty()) {
    auto& frame = stack.back();
    if (!frame.iter) {
      canvas.restoreToCount(frame.save_count);
      stack.pop_back();
      if (!stack.empty())
        ++stack.back().iter;
      continue;
    }

    const PaintOp* op = *frame.iter;
    PlaybackParams params(nullptr, SkM44(frame.original_ctm));
    switch (op->GetType()) {
      case PaintOpType::DrawRecord: {
        const DrawRecordOp* record_op = static_cast<const DrawRecordOp*>(op);
        stack.emplace_back(
            PaintOpBuffer::CompositeIterator(record_op->record.get(), nullptr),
            canvas.getLocalToDevice(), canvas.getSaveCount());
        continue;
      }

      // Any of the following ops result in non solid content.
      case PaintOpType::DrawDRRect:
      case PaintOpType::DrawImage:
      case PaintOpType::DrawImageRect:
      case PaintOpType::DrawIRect:
      case PaintOpType::DrawLine:
      case PaintOpType::DrawOval:
      case PaintOpType::DrawPath:
        return base::nullopt;
      // TODO(vmpstr): Add more tests on exceeding max_ops_to_analyze.
      case PaintOpType::DrawRRect: {
        if (++num_draw_ops > max_ops_to_analyze)
          return base::nullopt;
        const DrawRRectOp* rrect_op = static_cast<const DrawRRectOp*>(op);
        CheckIfSolidShape(canvas, rrect_op->rrect, rrect_op->flags, &is_solid,
                          &is_transparent, &color);
        break;
      }
      case PaintOpType::DrawSkottie:
      case PaintOpType::DrawTextBlob:
      // Anything that has to do a save layer is probably not solid. As it will
      // likely need more than one draw op.
      // TODO(vmpstr): We could investigate handling these.
      case PaintOpType::SaveLayer:
      case PaintOpType::SaveLayerAlpha:
      // Complex clips will probably result in non solid color as it might not
      // cover the canvas.
      // TODO(vmpstr): We could investigate handling these.
      case PaintOpType::ClipPath:
        return base::nullopt;
      case PaintOpType::ClipRRect: {
        const ClipRRectOp* rrect_op = static_cast<const ClipRRectOp*>(op);
        bool does_cover_canvas =
            CheckIfRRectClipCoversCanvas(canvas, rrect_op->rrect);
        // If the clip covers the full canvas, we can treat it as if there's no
        // clip at all and continue, otherwise this is no longer a solid color.
        if (!does_cover_canvas)
          return base::nullopt;
        break;
      }
      case PaintOpType::DrawRect: {
        if (++num_draw_ops > max_ops_to_analyze)
          return base::nullopt;
        const DrawRectOp* rect_op = static_cast<const DrawRectOp*>(op);
        CheckIfSolidShape(canvas, rect_op->rect, rect_op->flags, &is_solid,
                          &is_transparent, &color);
        break;
      }
      case PaintOpType::DrawColor: {
        if (++num_draw_ops > max_ops_to_analyze)
          return base::nullopt;
        const DrawColorOp* color_op = static_cast<const DrawColorOp*>(op);
        CheckIfSolidColor(canvas, color_op->color, color_op->mode, &is_solid,
                          &is_transparent, &color);
        break;
      }
      case PaintOpType::ClipRect: {
        // SolidColorAnalyzer uses an SkNoDrawCanvas which uses an
        // SkNoPixelsDevice which says (without looking) that the canvas's
        // clip is always a rect.  So, if this clip could result in not
        // a rect, this is no longer solid color.
        const ClipRectOp* clip_op = static_cast<const ClipRectOp*>(op);
        if (clip_op->op == SkClipOp::kDifference)
          return base::nullopt;
        op->Raster(&canvas, params);
        break;
      }

      // Don't affect the canvas, so ignore.
      case PaintOpType::Annotate:
      case PaintOpType::CustomData:
      case PaintOpType::SetNodeId:
      case PaintOpType::Noop:
        break;

      // The rest of the ops should only affect our state canvas.
      case PaintOpType::Concat:
      case PaintOpType::Scale:
      case PaintOpType::SetMatrix:
      case PaintOpType::Restore:
      case PaintOpType::Rotate:
      case PaintOpType::Save:
      case PaintOpType::Translate:
        op->Raster(&canvas, params);
        break;
    }
    ++frame.iter;
  }

  if (is_transparent)
    return SK_ColorTRANSPARENT;
  if (is_solid)
    return color;
  return base::nullopt;
}

}  // namespace cc
