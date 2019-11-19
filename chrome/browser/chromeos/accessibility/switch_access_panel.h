// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_PANEL_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/accessibility/accessibility_panel.h"
#include "chrome/common/extensions/extension_constants.h"

class SwitchAccessPanelTest;

// Shows a context menu of controls for Switch Access users
class SwitchAccessPanel : public AccessibilityPanel {
 public:
  explicit SwitchAccessPanel(content::BrowserContext* browser_context);
  void Show(const gfx::Rect& element_bounds,
            int width,
            int height,
            bool back_button_only);
  void Hide();
  ~SwitchAccessPanel() override = default;

 private:
  friend class SwitchAccessPanelTest;

  static const gfx::Rect CalculatePanelBounds(const gfx::Rect& element_bounds,
                                              const gfx::Rect& screen_bounds,
                                              const int panel_width,
                                              const int panel_height);
  static int GetFocusRingBuffer();

  DISALLOW_COPY_AND_ASSIGN(SwitchAccessPanel);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_PANEL_H_
