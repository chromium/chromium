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
  float radius_f = radius;
  layer->SetRoundedCornerRadius({radius_f, radius_f, radius_f, radius_f});
  layer->SetIsFastRoundedCorner(true);

  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(shadow_window);
  if (shadow)
    shadow->SetRoundedCornerRadius(radius);
}

}  // namespace ash
