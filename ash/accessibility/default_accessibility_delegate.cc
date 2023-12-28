// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/default_accessibility_delegate.h"

#include <limits>

#include "ash/accessibility/accessibility_controller.h"
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
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  return controller->spoken_feedback().enabled() || screen_magnifier_enabled_ ||
         controller->autoclick().enabled() ||
         controller->virtual_keyboard().enabled() ||
         controller->mono_audio().enabled() ||
         controller->large_cursor().enabled() ||
         controller->high_contrast().enabled();
}

void DefaultAccessibilityDelegate::SaveScreenMagnifierScale(double scale) {}

double DefaultAccessibilityDelegate::GetSavedScreenMagnifierScale() {
  return std::numeric_limits<double>::min();
}

}  // namespace ash
