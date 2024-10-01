// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_REGION_OVERLAY_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_REGION_OVERLAY_CONTROLLER_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/scanner/scanner_text.h"

namespace gfx {
class Canvas;
class Rect;
}  // namespace gfx

namespace ash {

// Controls the overlay shown on the capture region to indicate detected text,
// translations, etc.
class ASH_EXPORT CaptureRegionOverlayController {
 public:
  CaptureRegionOverlayController();
  CaptureRegionOverlayController(const CaptureRegionOverlayController&) =
      delete;
  CaptureRegionOverlayController& operator=(
      const CaptureRegionOverlayController&) = delete;
  ~CaptureRegionOverlayController();

  // Notifies the controller of text detected on the capture region. The
  // controller will track `detected_text`, e.g. to paint later if needed.
  // TODO(b/367548979): This method is currently only used in tests. It should
  // be called from backend services once the backend implementation is ready.
  void OnTextDetected(std::optional<ScannerText> detected_text);

  // Notifies the controller of translated text to show on the overlay. The
  // controller will track `translated_text`, e.g. to paint later if needed.
  // TODO(b/367549273): This method is currently only used in tests. It should
  // be called from backend services once the backend implementation is ready.
  void OnTranslatedTextFetched(std::optional<ScannerText> translated_text);

  // Paints the capture region overlay onto `canvas`. `region_bounds_in_canvas`
  // specifies the coordinates of `canvas` to paint the overlay.
  void PaintCaptureRegionOverlay(
      gfx::Canvas& canvas,
      const gfx::Rect& region_bounds_in_canvas) const;

 private:
  // Paints detected text regions in the overlay. `region_bounds_in_canvas`
  // specifies the coordinates of `canvas` which should contain the overlay.
  void PaintDetectedTextRegions(gfx::Canvas& canvas,
                                const gfx::Rect& region_bounds_in_canvas) const;

  // Paints translated text onto the overlay. `region_bounds_in_canvas`
  // specifies the coordinates of `canvas` which should contain the overlay.
  void PaintTranslatedText(gfx::Canvas& canvas,
                           const gfx::Rect& region_bounds_in_canvas) const;

  std::optional<ScannerText> detected_text_;

  std::optional<ScannerText> translated_text_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_REGION_OVERLAY_CONTROLLER_H_
