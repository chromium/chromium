// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_region_overlay_controller.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/scanner/scanner_text.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"

namespace ash {

namespace {

// TODO(b/367548979): Use correct style constants for detected text regions.
constexpr SkColor kDetectedTextRegionColor = SK_ColorCYAN;
constexpr float kDetectedTextRegionOpacity = 0.3f;

// Translates and rotates `canvas` so that `center_rotated_box` is upright and
// centered on the canvas. The components of `center_rotated_box` should be
// specified as coordinates relative to `region`.
void TranslateAndRotateCanvas(
    gfx::Canvas& canvas,
    const gfx::Rect& region,
    const ScannerText::CenterRotatedBox& center_rotated_box) {
  canvas.Translate(center_rotated_box.center.OffsetFromOrigin() +
                   region.origin().OffsetFromOrigin());
  canvas.sk_canvas()->rotate(center_rotated_box.rotation);
}

// Gets a rect with size `size` centered at the origin (offset to the nearest
// integer coordinates if `size` has odd width or odd height).
gfx::Rect GetRectCenteredAtOrigin(const gfx::Size& size) {
  return gfx::Rect(-size.width() / 2, -size.height() / 2, size.width(),
                   size.height());
}

}  // namespace

CaptureRegionOverlayController::CaptureRegionOverlayController() {}

CaptureRegionOverlayController::~CaptureRegionOverlayController() = default;

void CaptureRegionOverlayController::OnTextDetected(
    std::optional<ScannerText> detected_text) {
  detected_text_ = std::move(detected_text);
}

void CaptureRegionOverlayController::PaintCaptureRegionOverlay(
    gfx::Canvas& canvas,
    const gfx::Rect& region_bounds_in_canvas) const {
  PaintDetectedTextRegions(canvas, region_bounds_in_canvas);

  // TODO(b/367549273): Paint the translated text UI.
}

void CaptureRegionOverlayController::PaintDetectedTextRegions(
    gfx::Canvas& canvas,
    const gfx::Rect& region_bounds_in_canvas) const {
  if (!detected_text_.has_value()) {
    return;
  }

  // TODO(b/367548979): Paint detected text regions with correct styling.
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(kDetectedTextRegionColor);
  flags.setAlphaf(kDetectedTextRegionOpacity);
  for (const ScannerText::Paragraph& paragraph : detected_text_->paragraphs()) {
    for (const ScannerText::Line& line : paragraph.lines()) {
      canvas.Save();
      TranslateAndRotateCanvas(canvas, region_bounds_in_canvas,
                               line.bounding_box());
      canvas.DrawRect(GetRectCenteredAtOrigin(line.bounding_box().size), flags);
      canvas.Restore();
    }
  }
}

}  // namespace ash
