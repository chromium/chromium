// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_
#define ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace ash {

class ColorModeObserver;

// An interface implemented by Ash that provides colors for the system UI.
class ASH_PUBLIC_EXPORT ColorProvider {
 public:
  // Types of Shield layer. Number at the end of each type indicates the alpha
  // value.
  enum class ShieldLayerType {
    kShield20 = 0,
    kShield40,
    kShield60,
    kShield80,
    kShield90,
    kShield95,
  };

  // Blur sigma for system UI layers.
  static constexpr float kBackgroundBlurSigma = 30.f;

  // The default blur quality for background blur. Using a value less than 1
  // improves performance.
  static constexpr float kBackgroundBlurQuality = 0.33f;

  // Types of Base layer.
  enum class BaseLayerType {
    // Number at the end of each transparent type indicates the alpha value.
    kTransparent20 = 0,
    kTransparent40,
    kTransparent60,
    kTransparent80,
    kTransparent90,
    kTransparent95,

    // Base layer is opaque.
    kOpaque,
  };

  // Types of Controls layer.
  enum class ControlsLayerType {
    kHairlineBorderColor,
    kControlBackgroundColorActive,
    kControlBackgroundColorInactive,
    kControlBackgroundColorAlert,
    kControlBackgroundColorWarning,
    kControlBackgroundColorPositive,
    kFocusAuraColor,
    kFocusRingColor,
    // TODO(crbug/1224694): Rename these once naming in UX spec is finalized.
    kHighlightBorderHighlightColor,
    kHighlightBorderBorderColor,
  };

  enum class ContentLayerType {
    kScrollBarColor,
    kSeparatorColor,

    kTextColorPrimary,
    kTextColorSecondary,
    kTextColorAlert,
    kTextColorWarning,
    kTextColorPositive,
    kTextColorURL,

    kIconColorPrimary,
    kIconColorSecondary,
    kIconColorAlert,
    kIconColorWarning,
    kIconColorPositive,
    // Color for prominent icon, e.g, "Add connection" icon button inside
    // VPN detailed view.
    kIconColorProminent,

    // Background for kIconColorSecondary.
    kIconColorSecondaryBackground,

    // The default color for button labels.
    kButtonLabelColor,
    kButtonLabelColorPrimary,

    // Color for blue button labels, e.g, 'Retry' button of the system toast.
    kButtonLabelColorBlue,

    kButtonIconColor,
    kButtonIconColorPrimary,

    // Color for app state indicator.
    kAppStateIndicatorColor,
    kAppStateIndicatorColorInactive,

    // Color for the shelf drag handle in tablet mode.
    kShelfHandleColor,

    // Color for slider.
    kSliderColorActive,
    kSliderColorInactive,

    // Color for radio button.
    kRadioColorActive,
    kRadioColorInactive,

    // Color for toggle button.
    kSwitchKnobColorActive,
    kSwitchKnobColorInactive,
    kSwitchTrackColorActive,
    kSwitchTrackColorInactive,

    // Color for current active desk's border.
    kCurrentDeskColor,

    // Color for the battery's badge (bolt, unreliable, X).
    kBatteryBadgeColor,

    // Color for the switch access's back button.
    kSwitchAccessInnerStrokeColor,
    kSwitchAccessOuterStrokeColor,

    // Color for the media controls.
    kProgressBarColorForeground,
    kProgressBarColorBackground,

    // Color used to highlight a hovered view.
    kHighlightColorHover
  };

  // Attributes of ripple, includes the base color, opacity of inkdrop and
  // highlight.
  struct RippleAttributes {
    RippleAttributes(SkColor color,
                     float opacity_of_inkdrop,
                     float opacity_of_highlight)
        : base_color(color),
          inkdrop_opacity(opacity_of_inkdrop),
          highlight_opacity(opacity_of_highlight) {}
    const SkColor base_color;
    const float inkdrop_opacity;
    const float highlight_opacity;
  };

  static ColorProvider* Get();

  // Gets the color of |type| of the corresponding layer based on the current
  // color mode.
  virtual SkColor GetShieldLayerColor(ShieldLayerType type) const = 0;
  virtual SkColor GetBaseLayerColor(BaseLayerType type) const = 0;
  virtual SkColor GetControlsLayerColor(ControlsLayerType type) const = 0;
  virtual SkColor GetContentLayerColor(ContentLayerType type) const = 0;

  // Gets the attributes of ripple on |bg_color|. |bg_color| is the background
  // color of the UI element that wants to show inkdrop. Applies the color from
  // GetBackgroundColor if |bg_color| is not given. This means the background
  // color of the UI element is from Shiled or Base layer. See
  // GetShieldLayerColor and GetBaseLayerColor.
  virtual RippleAttributes GetRippleAttributes(
      SkColor bg_color = gfx::kPlaceholderColor) const = 0;

  virtual void AddObserver(ColorModeObserver* observer) = 0;
  virtual void RemoveObserver(ColorModeObserver* observer) = 0;

  // True if the current color mode is DARK. The default color mode is LIGHT if
  // the DarkLightMode feature is enabled. And it can be changed through pref
  // `kDarkModeEnabled`. But the default color mode is DARK if the
  // DarkLightMode feature is disabled. And it can be overridden by
  // ScopedLightModeAsDefault. See `override_light_mode_as_default_` for more
  // details.
  virtual bool IsDarkModeEnabled() const = 0;

 protected:
  ColorProvider();
  virtual ~ColorProvider();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_STYLE_COLOR_PROVIDER_H_
