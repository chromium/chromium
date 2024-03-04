// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_TAB_STRIP_H_
#define ASH_HUD_DISPLAY_TAB_STRIP_H_

#include <string>

#include "ash/hud_display/hud_constants.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout_view.h"

namespace gfx {
class Canvas;
}

namespace ash {
namespace hud_display {

class HUDDisplayView;
class HUDTabStrip;

class HUDTabButton : public views::LabelButton {
  METADATA_HEADER(HUDTabButton, views::LabelButton)

 public:
  // Defines tab paint style.
  enum class Style {
    LEFT,    // Tab to the left of the active tab.
    ACTIVE,  // Active tab.
    RIGHT,   // Tab to the right of the active tab.
  };

  HUDTabButton(Style style,
               const HUDDisplayMode display_mode,
               const std::u16string& text);
  HUDTabButton(const HUDTabButton&) = delete;
  HUDTabButton& operator=(const HUDTabButton&) = delete;

  ~HUDTabButton() override = default;

  void SetStyle(Style style);

  HUDDisplayMode display_mode() const { return display_mode_; }

 protected:
  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  Style style_ = Style::LEFT;

  // Tab activation sends this display mode to the HUD.
  HUDDisplayMode display_mode_;
};

class HUDTabStrip : public views::BoxLayoutView {
  METADATA_HEADER(HUDTabStrip, views::BoxLayoutView)

 public:
  explicit HUDTabStrip(HUDDisplayView* hud);

  HUDTabStrip(const HUDTabStrip&) = delete;
  HUDTabStrip& operator=(const HUDTabStrip&) = delete;

  ~HUDTabStrip() override;

  HUDTabButton* AddTabButton(const HUDDisplayMode display_mode,
                             const std::u16string& label);

  // Mark tabs around the active one need repaint to modify borders.
  void ActivateTab(HUDDisplayMode mode);

 private:
  raw_ptr<HUDDisplayView> hud_;
  std::vector<raw_ptr<HUDTabButton, VectorExperimental>>
      tabs_;  // Ordered list of child tabs.
};

}  // namespace hud_display
}  // namespace ash
#endif  // ASH_HUD_DISPLAY_TAB_STRIP_H_
