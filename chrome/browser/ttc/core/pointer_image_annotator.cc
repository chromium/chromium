// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_image_annotator.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontTypes.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace ttc {

namespace {

// The marks, in DIPs: a red ring with a white halo and a red crosshair, and
// for several points a white badge below each with its number in red.
constexpr float kRingRadius = 14;
constexpr float kArmLength = 24;
constexpr float kLineWidth = 2;
constexpr float kHaloWidth = 4;
constexpr float kBadgeFontSize = 14;
constexpr float kBadgeHeight = 22;
constexpr float kBadgePadding = 12;
constexpr float kBadgeOffsetY = 18;
constexpr float kBadgeBaseline = 16;
constexpr SkColor kMarkColor = SK_ColorRED;
constexpr SkColor kHaloColor = SK_ColorWHITE;

constexpr int kJpegQuality = 85;

// Device scale factors are small. Anything else is a bug in the caller, and
// would make the sizes above overflow.
constexpr float kMaxScaleFactor = 100;

SkPaint Stroke(SkColor color, float width) {
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(color);
  paint.setStrokeWidth(width);
  paint.setAntiAlias(true);
  return paint;
}

SkPaint Fill(SkColor color) {
  SkPaint paint;
  paint.setColor(color);
  paint.setAntiAlias(true);
  return paint;
}

// Draws the ring and crosshair for `point`, in the canvas's pixels, with the
// sizes above scaled by `scale_factor`.
void DrawCrosshair(SkCanvas& canvas, SkPoint point, float scale_factor) {
  const float radius = kRingRadius * scale_factor;
  canvas.drawCircle(point, radius,
                    Stroke(kHaloColor, kHaloWidth * scale_factor));
  const SkPaint mark = Stroke(kMarkColor, kLineWidth * scale_factor);
  canvas.drawCircle(point, radius, mark);
  const float arm = kArmLength * scale_factor;
  canvas.drawLine(point.x() - arm, point.y(), point.x() + arm, point.y(), mark);
  canvas.drawLine(point.x(), point.y() - arm, point.x(), point.y() + arm, mark);
}

// Draws the badge with `number` for `point`: below the point, or above it if
// that would run off the bottom, and moved as needed to stay inside the image.
void DrawBadge(SkCanvas& canvas,
               const SkFont& font,
               SkPoint point,
               float scale_factor,
               size_t number) {
  const std::string text = base::NumberToString(number);
  const float width =
      font.measureText(text.data(), text.size(), SkTextEncoding::kUTF8) +
      kBadgePadding * scale_factor;
  const float height = kBadgeHeight * scale_factor;
  const SkISize image = canvas.getBaseLayerSize();
  float top = point.y() + kBadgeOffsetY * scale_factor;
  if (top + height > image.height()) {
    top = point.y() - kBadgeOffsetY * scale_factor - height;
  }
  // The image may be smaller than the badge.
  top = std::clamp(top, 0.0f, std::max(0.0f, image.height() - height));
  const float left = std::clamp(point.x() - width / 2, 0.0f,
                                std::max(0.0f, image.width() - width));
  canvas.drawRect(SkRect::MakeXYWH(left, top, width, height), Fill(kHaloColor));
  canvas.drawString(text.c_str(), left + kBadgePadding * scale_factor / 2,
                    top + kBadgeBaseline * scale_factor, font,
                    Fill(kMarkColor));
}

}  // namespace

std::optional<VisualContent> AnnotatePointerImage(
    const SkBitmap& viewport,
    const gfx::Rect& crop_dip,
    base::span<const gfx::PointF> points,
    float scale_factor) {
  // Written so that a scale factor that is not a number is rejected too.
  if (viewport.drawsNothing() ||
      !(scale_factor > 0 && scale_factor <= kMaxScaleFactor)) {
    return std::nullopt;
  }
  gfx::Rect crop = gfx::ScaleToEnclosingRect(crop_dip, scale_factor);
  crop.Intersect(gfx::Rect(viewport.width(), viewport.height()));
  if (crop.IsEmpty()) {
    return std::nullopt;
  }

  // Copy the crop out of the viewport, then draw on the copy, in device
  // pixels throughout.
  SkBitmap image;
  if (!image.tryAllocN32Pixels(crop.width(), crop.height(),
                               /*isOpaque=*/true) ||
      !viewport.readPixels(image.pixmap(), crop.x(), crop.y())) {
    return std::nullopt;
  }
  SkCanvas canvas(image);
  const auto to_pixels = [&](const gfx::PointF& point) {
    return SkPoint::Make(point.x() * scale_factor - crop.x(),
                         point.y() * scale_factor - crop.y());
  };
  // All the crosshairs first, so that none is drawn over a badge. Points that
  // are not numbers are left out; Skia does not accept them.
  for (const gfx::PointF& point : points) {
    if (const SkPoint pixels = to_pixels(point); pixels.isFinite()) {
      DrawCrosshair(canvas, pixels, scale_factor);
    }
  }
  if (points.size() > 1) {
    SkFont font = skia::DefaultFont();
    font.setSize(kBadgeFontSize * scale_factor);
    font.setEmbolden(true);
    for (size_t i = 0; i < points.size(); ++i) {
      if (const SkPoint pixels = to_pixels(points[i]); pixels.isFinite()) {
        DrawBadge(canvas, font, pixels, scale_factor, i + 1);
      }
    }
  }

  std::optional<std::vector<uint8_t>> jpeg =
      gfx::JPEGCodec::Encode(image, kJpegQuality);
  if (!jpeg) {
    return std::nullopt;
  }
  return VisualContent{.image_data = std::move(*jpeg),
                       .mime_type = "image/jpeg"};
}

}  // namespace ttc
