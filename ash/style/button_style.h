// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_BUTTON_STYLE_H_
#define ASH_STYLE_BUTTON_STYLE_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {

// A circular ImageButton with kCloseButtonIcon inside. It has small, medium and
// large three different size. The touch area of the small close button will be
// expanded.
class CloseButton : public views::ImageButton,
                    public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(CloseButton);

  enum class Type {
    kSmall,
    kMedium,
    kLarge,
  };

  CloseButton(PressedCallback callback,
              Type type,
              bool use_light_colors = false);
  CloseButton(const CloseButton&) = delete;
  CloseButton& operator=(const CloseButton&) = delete;
  ~CloseButton() override;

  bool DoesIntersectScreenRect(const gfx::Rect& screen_rect) const;

 private:
  // views::ImageButton:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  const Type type_;
  const bool use_light_colors_;
};

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

  // Keeps the button in light mode if `use_light_colors` is true.
  // InstallRoundRectHighlightPathGenerator for the button only if
  // `rounded_highlight_path` is true. This is special handlings for buttons
  // inside the old notifications UI, might can be removed once
  // `kNotificationsRefresh` is fully launched.
  PillButton(PressedCallback callback,
             const std::u16string& text,
             Type type,
             const gfx::VectorIcon* icon,
             bool use_light_colors = false,
             bool rounded_highlight_path = true);
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

  // True if the button wants use light colors when the D/L mode feature is not
  // enabled.
  bool use_light_colors_;
};

}  // namespace ash

#endif  // ASH_STYLE_BUTTON_STYLE_H_
