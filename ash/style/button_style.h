// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_BUTTON_STYLE_H_
#define ASH_STYLE_BUTTON_STYLE_H_

#include "ash/system/tray/tray_popup_ink_drop_style.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

// A label button with a rounded rectangle background. It can have an icon
// inside as well, and its text and background colors will be different based on
// the type of the button.
class PillButton : public views::LabelButton {
 public:
  METADATA_HEADER(PillButton);

  // Types of the PillButton.
  enum class Type {
    // PillButton with an icon, default text and background colors.
    kIcon,
    // PillButton without an icon, default text and background colors.
    kIconless,
    // PillButton without an icon, `kButtonLabelColorPrimary` as the text color
    // and `kControlBackgroundColorAlert` as the background color.
    kIconlessAlert,
    // PillButton without an icon, `kButtonLabelColorBlue` as the text color and
    // default background color.
    kIconlessAccent,
    // PillButton without an icon, default text color and
    // `kControlBackgroundColorActive` as the background color.
    kIconlessProminent,
    // `kIconless` button without background.
    kIconlessFloating,
    // `kIconlessAccent` button without background.
    kIconlessAccentFloating,
  };

  // TODO: Move this function outside of PillButton after we built up more
  // button styles under this file. And remove all the corresponding functions
  // from TrayPopupUtils to make all the clients get the ink drop styles from
  // ash/style.
  static void ConfigureInkDrop(views::Button* button,
                               TrayPopupInkDropStyle style,
                               bool highlight_on_hover,
                               bool highlight_on_focus);

  PillButton(PressedCallback callback,
             const std::u16string& text,
             Type type,
             const gfx::VectorIcon* icon);
  PillButton(const PillButton&) = delete;
  PillButton& operator=(const PillButton&) = delete;
  ~PillButton() override;

  // views::LabelButton:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnThemeChanged() override;

 private:
  const Type type_;
  const gfx::VectorIcon* const icon_;
};

}  // namespace ash

#endif  // ASH_STYLE_BUTTON_STYLE_H_
