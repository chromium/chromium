// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_RGB_KEYBOARD_RGB_KEYBOARD_UTIL_H_
#define ASH_RGB_KEYBOARD_RGB_KEYBOARD_UTIL_H_

#include "ash/ash_export.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"

namespace ash {

inline constexpr SkColor kDefaultColor =
    SkColorSetRGB(/*r=*/255, /*g=*/255, /*b=*/210);

// Util method to convert the |BacklightColor| enum to a predefined SkColor
// which will be set by rgb keyboard manager to change the color of the keyboard
// backlight.
ASH_EXPORT SkColor ConvertBacklightColorToSkColor(
    personalization_app::mojom::BacklightColor backlight_color);

// Util method to convert the |BacklightColor| enum to a |SkColor| that is used
// as the background color for the rgb icon displayed in the system's keyboard
// brightness slider. The color is different from the color returned by
// |ConvertBacklightColorToSkColor| and matches the color displayed in the
// personalization hub as the actual rgb keyboard colors appear to be visually
// darker than what the UX wants to show to users.
SkColor ConvertBacklightColorToIconBackgroundColor(
    personalization_app::mojom::BacklightColor backlight_color);

}  // namespace ash

#endif  // ASH_RGB_KEYBOARD_RGB_KEYBOARD_UTIL_H_
