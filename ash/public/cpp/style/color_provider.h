// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_
#define ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"
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

  // Types of Controls layer.
  enum class ControlsLayerType {
    kControlBackgroundColorActive,
    kControlBackgroundColorInactive,
    kFocusRingColor,
  };

  enum class ContentLayerType {
    kSeparatorColor,

    kTextColorPrimary,
    kTextColorSecondary,
    kTextColorURL,

    kIconColorPrimary,
    kIconColorSecondary,
    kIconColorAlert,
    kIconColorWarning,

    // Color for prominent icon, e.g, "Add connection" icon button inside
    // VPN detailed view.
    kIconColorProminent,

    // Background for kIconColorSecondary.
    kIconColorSecondaryBackground,

    // The default color for button labels.
    kButtonLabelColor,

    // Color for blue button labels, e.g, 'Retry' button of the system toast.
    kButtonLabelColorBlue,

    kButtonIconColor,
    kButtonIconColorPrimary,

    // Color for toggle button.
    kSwitchKnobColorActive,
    kSwitchKnobColorInactive,
    kSwitchTrackColorActive,
    kSwitchTrackColorInactive,
  };

  static ColorProvider* Get();

  // Gets the color of |type| of the corresponding layer based on the current
  // color mode.
  virtual SkColor GetControlsLayerColor(ControlsLayerType type) const = 0;
  virtual SkColor GetContentLayerColor(ContentLayerType type) const = 0;

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
