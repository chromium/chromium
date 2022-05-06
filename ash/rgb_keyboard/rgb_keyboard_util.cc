// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_util.h"

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"

namespace ash {

SkColor ConvertBacklightColorToSkColor(
    personalization_app::mojom::BacklightColor backlight_color) {
  switch (backlight_color) {
    case personalization_app::mojom::BacklightColor::kWhite:
      return SkColorSetRGB(/*r=*/255, /*g=*/255, /*b=*/210);
    case personalization_app::mojom::BacklightColor::kRed:
      return SkColorSetRGB(/*r=*/197, /*g=*/34, /*b=*/31);
    case personalization_app::mojom::BacklightColor::kYellow:
      return SkColorSetRGB(/*r=*/236, /*g=*/106, /*b=*/8);
    case personalization_app::mojom::BacklightColor::kGreen:
      return SkColorSetRGB(/*r=*/51, /*g=*/128, /*b=*/28);
    case personalization_app::mojom::BacklightColor::kBlue:
      return SkColorSetRGB(/*r=*/32, /*g=*/177, /*b=*/137);
    case personalization_app::mojom::BacklightColor::kIndigo:
      return SkColorSetRGB(/*r=*/25, /*g=*/55, /*b=*/210);
    case personalization_app::mojom::BacklightColor::kPurple:
      return SkColorSetRGB(/*r=*/132, /*g=*/32, /*b=*/180);
    default:
      return kInvalidColor;
  }
}

}  // namespace ash
