// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_PALETTE_H_
#define ASH_LOGIN_UI_LOGIN_PALETTE_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// LoginPalette provides color values to LoginPasswordView and LoginPinView,
// so that these views can be adapted for different scenarios, e.g. the
// LoginPasswordView for in-session auth dialog will have different colors from
// the LoginPasswordView for lock screen.
struct LoginPalette {
  SkColor password_text_color;
  SkColor password_placeholder_text_color;
  SkColor password_background_color;
  SkColor button_enabled_color;
  SkColor button_annotation_color;
  SkColor pin_ink_drop_highlight_color;
  SkColor pin_ink_drop_ripple_color;
};

// For login screen and lock screen.
ASH_EXPORT LoginPalette CreateDefaultLoginPalette();

// For in-session auth dialog.
ASH_EXPORT LoginPalette CreateInSessionAuthPalette();

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_PALETTE_H_
