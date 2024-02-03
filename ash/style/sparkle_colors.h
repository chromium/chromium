// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SPARKLE_COLORS_H_
#define ASH_STYLE_SPARKLE_COLORS_H_

#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_key.h"

namespace ash {

// Adds the sparkle reference colors to `mixer` based on the hue of seed color
// in `key`. If a seed color is not specified, an arbitrary set of sparkle
// colors are used.
void AddSparkleColors(ui::ColorMixer& mixer, const ui::ColorProviderKey& key);

}  // namespace ash

#endif  // ASH_STYLE_SPARKLE_COLORS_H_
