// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_TAB_STRIP_H_
#define ASH_HUD_DISPLAY_TAB_STRIP_H_

#include "ash/hud_display/hud_constants.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

namespace views {
class View;
}

namespace ash {
namespace hud_display {

class HUDDisplayView;
class HUDTabStrip;

class HUDTabButton : public views::LabelButton {
 public:
  // Defines tab paint style.
  enum class Style {
    LEFT,    // Tab to the left of the active tab.
    ACTIVE,  // Active tab.
    RIGHT    // Tab to the right of the active tab.
  };

  METADATA_HEADER(HUDTabButton);

  HUDTabButton(Style style,
               const DisplayMode display_mode,
               const base::string16& text);
  HUDTabButton(const HUDTabButton&) = delete;
  HUDTabButton& operator=(const HUDTabButton&) = delete;

  ~HUDTabButton() override = default;

  void SetStyle(Style style);

  DisplayMode display_mode() const { return display_mode_; }

 protected:
  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  Style style_ = Style::LEFT;

  // Tab activation sends this display mode to the HUD.
  DisplayMode display_mode_;
};

class HUDTabStrip : public views::View {
 public:
  METADATA_HEADER(HUDTabStrip);

  explicit HUDTabStrip(HUDDisplayView* hud);

  HUDTabStrip(const HUDTabStrip&) = delete;
  HUDTabStrip& operator=(const HUDTabStrip&) = delete;

  ~HUDTabStrip() override;

  HUDTabButton* AddTabButton(const DisplayMode display_mode,
                             const base::string16& label);

  // Mark tabs around the active one need repaint to modify borders.
  void ActivateTab(DisplayMode mode);

 private:
  HUDDisplayView* hud_;
  std::vector<HUDTabButton*> tabs_;  // Ordered list of child tabs.
};

}  // namespace hud_display
}  // namespace ash
#endif  // ASH_HUD_DISPLAY_TAB_STRIP_H_
