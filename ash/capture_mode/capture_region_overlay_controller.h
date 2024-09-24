// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_REGION_OVERLAY_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_REGION_OVERLAY_CONTROLLER_H_

#include "ash/ash_export.h"

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

  // Paints the capture region overlay onto `canvas`. `region_bounds_in_canvas`
  // specifies the coordinates of `canvas` to paint the overlay.
  void PaintCaptureRegionOverlay(
      gfx::Canvas& canvas,
      const gfx::Rect& region_bounds_in_canvas) const;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_REGION_OVERLAY_CONTROLLER_H_
