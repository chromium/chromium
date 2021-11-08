// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_STYLE_UTIL_H_
#define ASH_STYLE_STYLE_UTIL_H_

#include "ash/system/tray/tray_popup_ink_drop_style.h"
#include "ui/gfx/color_palette.h"

namespace views {
class Button;
}  // namespace views

namespace ash {

namespace style_util {

// TODO: Migrate the TrayPopupInkDropStyle to ash/style, remove
// TrayPopupUtils::ConfigureTrayPopupButton and migrate all its clients to this
// function.
// Sets up the inkdrop for the given `button`. Including setting the callback
// for InkDrop, Ripple, Highlight. Inside the callback functions, they will
// setup whether to show the highlight on hover or focus, inkdrop color, opacity
// etc.
void SetUpInkDropForButton(views::Button* button,
                           TrayPopupInkDropStyle style,
                           bool highlight_on_hover,
                           bool highlight_on_focus,
                           SkColor bg_color = gfx::kPlaceholderColor);

}  // namespace style_util

}  // namespace ash

#endif  // ASH_STYLE_STYLE_UTIL_H_
