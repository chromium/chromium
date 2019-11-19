// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/default_accessibility_delegate.h"

#include <limits>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"

namespace ash {

DefaultAccessibilityDelegate::DefaultAccessibilityDelegate() = default;

DefaultAccessibilityDelegate::~DefaultAccessibilityDelegate() = default;

void DefaultAccessibilityDelegate::SetMagnifierEnabled(bool enabled) {
  screen_magnifier_enabled_ = enabled;
}

bool DefaultAccessibilityDelegate::IsMagnifierEnabled() const {
  return screen_magnifier_enabled_;
}

bool DefaultAccessibilityDelegate::ShouldShowAccessibilityMenu() const {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  return controller->spoken_feedback_enabled() || screen_magnifier_enabled_ ||
         controller->autoclick_enabled() ||
         controller->virtual_keyboard_enabled() ||
         controller->mono_audio_enabled() ||
         controller->large_cursor_enabled() ||
         controller->high_contrast_enabled();
}

void DefaultAccessibilityDelegate::SaveScreenMagnifierScale(double scale) {}

double DefaultAccessibilityDelegate::GetSavedScreenMagnifierScale() {
  return std::numeric_limits<double>::min();
}

}  // namespace ash
