// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ELEMENT_STYLE_H_
#define ASH_STYLE_ELEMENT_STYLE_H_

#include "ash/style/ash_color_provider.h"
#include "ui/gfx/vector_icon_types.h"

namespace views {
class ImageButton;
class LabelButton;
}  // namespace views

namespace ash {

// This file includes defined styles of the UI elements. For example, styles for
// the button, which includes the button's size, icon size, border, color etc.
namespace element_style {

// Constants of the icon buttons.
constexpr int kSmallIconButtonSize = 32;
constexpr int kMediumIconButtonSize = 36;
constexpr int kLargeIconButtonSize = 48;
constexpr int kIconButtonIconSize = 20;
constexpr int kIconButtonBorderSize = 4;

// Multiple types of icon buttons. It is an ImageButton with a circular
// background, which is toggleable. And its background will have different
// colors while it is toggled. Border will be added to the button if
// `has_border` is true. The only difference among small/medium/large icon
// button is the size of the circular background. E.g, FeaturePodButton inside
// the system tray menu is a medium size icon button with a border.
void DecorateSmallIconButton(views::ImageButton* button,
                             const gfx::VectorIcon& icon,
                             bool toggled,
                             bool has_border);
void DecorateMediumIconButton(views::ImageButton* button,
                              const gfx::VectorIcon& icon,
                              bool toggled,
                              bool has_border);
void DecorateLargeIconButton(views::ImageButton* button,
                             const gfx::VectorIcon& icon,
                             bool toggled,
                             bool has_border);

// Floating icon button. It is an ImageButton without circular background and
// it is not toggleable. For example, CollapseButton inside the system tray
// menu is a floating icon button.
void DecorateFloatingIconButton(views::ImageButton* button,
                                const gfx::VectorIcon& icon);

// A LabelButton with label color `kButtonLabelColorBlue` without background.
void DecorateIconlessFloatingPillButton(views::LabelButton* button);

// Icon-less pill button is a LabelButton with a rounded rectangle background.
void DecorateIconlessPillButton(views::LabelButton* button);

// With-icon pill button is a LabelButton with an icon and a rounded rectangle
// background.
void DecorateIconPillButton(views::LabelButton* button,
                            const gfx::VectorIcon* icon);

// Same as icon-less pill button but with different label and background color.
// The background color is `kControlBackgroundColorAlert`.
void DecorateIconlessAlertPillButton(views::LabelButton* button);

// Same as icon-less pill button but with label color `kButtonLabelColorBlue`.
void DecorateIconlessAccentPillButton(views::LabelButton* button);

// Same as icon-less pill button but with background color
// `kControlBackgroundColorActive`.
void DecorateIconlessProminentPillButton(views::LabelButton* button);

// Multiple types of close buttons. It is an ImageButton with a close icon
// inside. And it has a circular background. The only difference among
// small/medium/large close buttons is the size of the background. For
// example, the CloseDeskButton is a small close button.
void DecorateSmallCloseButton(views::ImageButton* button,
                              const gfx::VectorIcon& icon);
void DecorateMediumCloseButton(views::ImageButton* button,
                               const gfx::VectorIcon& icon);
void DecorateLargeCloseButton(views::ImageButton* button,
                              const gfx::VectorIcon& icon);

}  // namespace element_style

}  // namespace ash

#endif  // ASH_STYLE_ELEMENT_STYLE_H_
