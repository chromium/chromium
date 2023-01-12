// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/rounded_corner_utils.h"

#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/wm/core/shadow_controller.h"

namespace ash {

void SetCornerRadius(aura::Window* shadow_window,
                     ui::Layer* layer,
                     int radius) {
  const gfx::RoundedCornersF rounded_corner_radii(radius);
  if (layer->rounded_corner_radii() != rounded_corner_radii) {
    layer->SetRoundedCornerRadius(rounded_corner_radii);
  }
  if (!layer->is_fast_rounded_corner()) {
    layer->SetIsFastRoundedCorner(true);
  }

  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(shadow_window);
  if (shadow)
    shadow->SetRoundedCornerRadius(radius);
}

}  // namespace ash
