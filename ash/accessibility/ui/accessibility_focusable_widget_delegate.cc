// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_focusable_widget_delegate.h"

namespace ash {

AccessibilityFocusableWidgetDelegate::AccessibilityFocusableWidgetDelegate(
    bool register_with_focus_cycler)
    : AccessibilityFocusable<views::WidgetDelegate>(
          register_with_focus_cycler) {
  SetFocusTraversesOut(true);
}

}  // namespace ash
