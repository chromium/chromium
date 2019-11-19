// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/switch_access_panel.h"

#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/no_destructor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace {

const char kWidgetName[] = "SwitchAccessMenu";
const int kFocusRingBuffer = 5;

const std::string& UrlForContent() {
  static const base::NoDestructor<std::string> url(
      std::string(EXTENSION_PREFIX) + extension_misc::kSwitchAccessExtensionId +
      "/menu_panel.html");
  return *url;
}

}  // namespace

SwitchAccessPanel::SwitchAccessPanel(content::BrowserContext* browser_context)
    : AccessibilityPanel(browser_context, UrlForContent(), kWidgetName) {
  Hide();
}

void SwitchAccessPanel::Show(const gfx::Rect& element_bounds,
                             int width,
                             int height,
                             bool back_button_only) {
  // TODO(crbug/893752): Support multiple displays
  gfx::Rect screen_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  gfx::Rect panel_bounds;

  if (back_button_only) {
    int x = element_bounds.right();
    int y = element_bounds.y() - height;
    panel_bounds.SetRect(x, y, width, height);
    panel_bounds.AdjustToFit(screen_bounds);
  } else {
    panel_bounds =
        CalculatePanelBounds(element_bounds, screen_bounds, width, height);
  }

  ash::AccessibilityController::Get()->SetAccessibilityPanelBounds(
      panel_bounds, ash::AccessibilityPanelState::BOUNDED);
}

void SwitchAccessPanel::Hide() {
  // This isn't set to (0, 0, 0, 0) because the drop shadow remains visible.
  // TODO(crbug/911344): Find the root cause and fix it.
  gfx::Rect bounds(-1, -1, 1, 1);
  ash::AccessibilityController::Get()->SetAccessibilityPanelBounds(
      bounds, ash::AccessibilityPanelState::BOUNDED);
}

const gfx::Rect SwitchAccessPanel::CalculatePanelBounds(
    const gfx::Rect& element_bounds,
    const gfx::Rect& screen_bounds,
    const int panel_width,
    const int panel_height) {
  gfx::Rect padded_element_bounds = element_bounds;
  padded_element_bounds.Inset(-GetFocusRingBuffer(), -GetFocusRingBuffer());

  // Decide if the horizontal position should be to the right of the element, to
  // the left of the element, or if neither is possible, against the right edge
  // of the screen.
  int panel_x = padded_element_bounds.right();
  if (padded_element_bounds.right() + panel_width > screen_bounds.right()) {
    if (padded_element_bounds.x() - panel_width > screen_bounds.x())
      panel_x = padded_element_bounds.x() - panel_width;
    else
      panel_x = screen_bounds.right() - panel_width;
  }

  // Decide if the vertical position should be below the element, above the
  // element, or if neither is possible, against the bottom edge of the screen.
  int panel_y = padded_element_bounds.bottom();
  if (padded_element_bounds.bottom() + panel_height > screen_bounds.bottom()) {
    if (padded_element_bounds.y() - panel_height > screen_bounds.y())
      panel_y = padded_element_bounds.y() - panel_height;
    else
      panel_y = screen_bounds.bottom() - panel_height;
  }

  return gfx::Rect(panel_x, panel_y, panel_width, panel_height);
}

int SwitchAccessPanel::GetFocusRingBuffer() {
  return kFocusRingBuffer;
}
