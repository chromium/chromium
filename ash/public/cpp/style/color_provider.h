// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_
#define ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"

namespace ash {

// An interface implemented by Ash that provides colors for the system UI.
class ASH_PUBLIC_EXPORT ColorProvider {
 public:
  // Blur sigma for system UI layers.
  static constexpr float kBackgroundBlurSigma = 30.f;

  // The default blur quality for background blur. Using a value less than 1
  // improves performance.
  static constexpr float kBackgroundBlurQuality = 0.33f;

  static ColorProvider* Get();

  // Gets the color by resolving the `color_id`.
  virtual SkColor GetColor(ui::ColorId color_id) const = 0;

  // Gets the ink drop base color and opacity. Since the inkdrop ripple and
  // highlight have the same opacity, we are keeping only one opacity here. The
  // base color will be gotten based on current color mode, which will be WHITE
  // in dark mode and BLACK in light mode. Please provide `background_color` if
  // different base color needed on current color mode. See more details of
  // IsDarkModeEnabled for current color mode.
  virtual std::pair<SkColor, float> GetInkDropBaseColorAndOpacity(
      SkColor background_color = gfx::kPlaceholderColor) const = 0;

 protected:
  ColorProvider();
  virtual ~ColorProvider();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_
