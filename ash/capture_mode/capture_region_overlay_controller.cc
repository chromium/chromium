// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_region_overlay_controller.h"

namespace ash {

CaptureRegionOverlayController::CaptureRegionOverlayController() {}

CaptureRegionOverlayController::~CaptureRegionOverlayController() = default;

void CaptureRegionOverlayController::PaintCaptureRegionOverlay(
    gfx::Canvas& canvas,
    const gfx::Rect& region_bounds_in_canvas) const {
  // TODO(b/363101913): Implement the overlay UI.
}

}  // namespace ash
