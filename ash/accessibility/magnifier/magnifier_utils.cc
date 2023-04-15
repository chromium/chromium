// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/magnifier_utils.h"

#include <algorithm>
#include <cmath>

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/shell.h"
#include "base/check_op.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/ime_bridge.h"

namespace ash {
namespace magnifier_utils {

namespace {

// Converts the given |scale| to an index such that
// `kMagnificationScaleFactor ^ index = scale`.
int IndexFromScale(float scale) {
  // Remember from the Logarithm rules:
  // logBar(Foo) = log2(Foo) / log2(Bar).
  return std::round(std::log(scale) / std::log(kMagnificationScaleFactor));
}

}  // namespace

float GetScaleFromScroll(float linear_offset,
                         float current_scale,
                         float min_scale,
                         float max_scale) {
  DCHECK_GE(current_scale, min_scale);
  DCHECK_LE(current_scale, max_scale);

  // Convert the current scale back to its corresponding linear scale according
  // to the formula `scale = (max - min) * offset ^ 2 + min`.
  const float scale_range = max_scale - min_scale;
  float linear_adjustment =
      std::sqrt((current_scale - min_scale) / scale_range);
  // Add the new linear offset.
  linear_adjustment += linear_offset;

  // Convert back to the exponential scale.
  return scale_range * linear_adjustment * linear_adjustment + min_scale;
}

float GetNextMagnifierScaleValue(int delta_index,
                                 float current_scale,
                                 float min_scale,
                                 float max_scale) {
  const int current_index = IndexFromScale(current_scale);
  const int new_scale_index = current_index + delta_index;
  const float new_scale = std::pow(kMagnificationScaleFactor, new_scale_index);
  return std::clamp(new_scale, min_scale, max_scale);
}

gfx::Rect GetViewportWidgetBoundsInRoot(aura::Window* root,
                                        float screen_height_divisor) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  auto root_bounds = root->GetBoundsInRootWindow();
  root_bounds.set_height(root_bounds.height() / screen_height_divisor);
  return root_bounds;
}

void MaybeUpdateActiveMagnifierFocus(const gfx::Point& point_in_screen) {
  DockedMagnifierController* docked_magnifier =
      Shell::Get()->docked_magnifier_controller();
  FullscreenMagnifierController* fullscreen_magnifier =
      Shell::Get()->fullscreen_magnifier_controller();
  if (docked_magnifier->GetEnabled())
    docked_magnifier->CenterOnPoint(point_in_screen);
  else if (fullscreen_magnifier->IsEnabled())
    fullscreen_magnifier->CenterOnPoint(point_in_screen);
}

}  // namespace magnifier_utils
}  // namespace ash
