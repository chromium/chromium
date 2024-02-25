// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_HARMONIZED_COLORS_H_
#define ASH_STYLE_HARMONIZED_COLORS_H_

#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_key.h"

namespace ash {

// Returns the angle in degrees of the hue for `seed_color`.
int HueAngle(SkColor seed_color);

// Adds the harmonized reference colors to `mixer` based on the seed color in
// `key`. If a seed color is not specified, an arbitrary set of harmonized
// colors are used.
void AddHarmonizedColors(ui::ColorMixer& mixer,
                         const ui::ColorProviderKey& key);

}  // namespace ash

#endif  // ASH_STYLE_HARMONIZED_COLORS_H_
