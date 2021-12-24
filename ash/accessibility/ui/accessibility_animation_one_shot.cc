// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_animation_one_shot.h"

#include "ash/shell.h"
#include "base/debug/crash_logging.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

AccessibilityAnimationOneShot::AccessibilityAnimationOneShot(
    const gfx::Rect& bounds_in_dip,
    base::RepeatingCallback<bool(base::TimeTicks)> callback)
    : callback_(callback) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds_in_dip);
  // Crash keys for https://crbug.com/1254275
  SCOPED_CRASH_KEY_STRING32("Accessibility", "BoundsInDip",
                            bounds_in_dip.ToString());
  SCOPED_CRASH_KEY_STRING32("Accessibility", "Display", display.ToString());
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display.id());
  SCOPED_CRASH_KEY_STRING32(
      "Accessibility", "RootWindow",
      !root_window ? "Root window is null" : "Root window is valid");
  ui::Compositor* compositor = root_window->layer()->GetCompositor();
  animation_observation_.Observe(compositor);
}

AccessibilityAnimationOneShot::~AccessibilityAnimationOneShot() = default;

void AccessibilityAnimationOneShot::OnAnimationStep(base::TimeTicks timestamp) {
  if (callback_.Run(timestamp))
    animation_observation_.Reset();
}

void AccessibilityAnimationOneShot::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  if (compositor && animation_observation_.IsObservingSource(compositor))
    animation_observation_.Reset();
}

}  // namespace ash
