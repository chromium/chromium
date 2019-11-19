// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ASH_COLOR_PROVIDER_H_
#define ASH_STYLE_ASH_COLOR_PROVIDER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// The color provider for system UI. It provides colors for Shield layer, Base
// layer and +1 layer. Shield layer is a combination of color, opacity and blur
// which may change depending on the context, it is usually a fullscreen layer.
// e.g, PowerButtoneMenuScreenView for power button menu. Base layer is the
// bottom layer of any UI displayed on top of all other UIs. e.g, the ShelfView
// that contains all the shelf items. Controls layer is where components such as
// icons and inkdrops lay on, it may also indicate the state of an interactive
// element (active/inactive states). The color of an element in system UI will
// be the combination of the colors of the three layers.
class ASH_EXPORT AshColorProvider {
 public:
  // The color mode of system UI. Switch "--ash-color-mode" can only set
  // |color_mode_| to |kLight| or |kDark|, |color_mode_| will be |kDefault| if
  // the flag is not set.
  enum class AshColorMode {
    // This is the color mode of current system UI, which is a combination of
    // dark and light mode. e.g, shelf and system tray are dark while many other
    // elements like notification are light.
    kDefault = 0,
    // The text is black while the background is white or light.
    kLight,
    // The text is light color while the background is black or dark grey.
    kDark
  };

  // Types of Shield layer.
  enum class ShieldLayerType {
    kAlpha20,  // opacity of the layer is 20%
    kAlpha40,  // opacity of the layer is 40%
    kAlpha60,  // opacity of the layer is 60%
  };

  // Types of Base layer.
  enum class BaseLayerType {
    // Base layer is transparent with blur.
    kTransparentWithBlur = 0,

    // Base layer is transparent without blur.
    kTransparentWithoutBlur,

    // Base layer is opaque.
    kOpaque,

    // Base layer is red. e.g, the "sign out" button inside status area.
    kRed,
  };

  // Types of Controls layer.
  enum class ControlsLayerType {
    kHairlineBorder,
    kActiveControlBackground,
    kInactiveControlBackground,
    kFocusRing,
  };

  enum class ContentLayerType {
    kSeparator,
    kTextPrimary,
    kTextSecondary,
    kIconPrimary,
    kIconSecondary,
    kIconRed,
    // Color for prominent icon button, e.g, "Add connection" icon button inside
    // VPN detailed view.
    kProminentIconButton,
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

  AshColorProvider();
  ~AshColorProvider();

  static AshColorProvider* Get();

  // Gets the disabled color on |enabled_color|. It can be disabled background,
  // an disabled icon, etc.
  static SkColor GetDisabledColor(SkColor enabled_color);

  // Gets the color of second tone on the given |color_of_first_tone|. e.g,
  // power status icon inside status area is a dual tone icon.
  static SkColor GetSecondToneColor(SkColor color_of_first_tone);

  // Gets color of Shield layer. See details at the corresponding function of
  // Base layer.
  SkColor DeprecatedGetShieldLayerColor(ShieldLayerType type,
                                        SkColor default_color) const;
  SkColor GetShieldLayerColor(ShieldLayerType type,
                              AshColorMode given_color_mode) const;

  // Used by UI elements that need to support |kDefault| mode to get the color
  // of base layer. |default_color| is provided while |color_mode_| is not set.
  // Otherwise, gets the base layer color on |type| and |color_mode_|. Note,
  // this function will be removed after launch dark/light mode.
  SkColor DeprecatedGetBaseLayerColor(BaseLayerType type,
                                      SkColor default_color) const;
  // Used by new specs to get the color of base layer. |given_color_mode| is
  // provided since the colors of new specs will always follow |kLight| or
  // |kDark| mode. But |color_mode_| should have higher priority, gets the color
  // on |color_mode_| instead if it is set.
  SkColor GetBaseLayerColor(BaseLayerType type,
                            AshColorMode given_color_mode) const;

  // Gets color of Controls layer. See details at the corresponding function of
  // Base layer.
  SkColor DeprecatedGetControlsLayerColor(ControlsLayerType type,
                                          SkColor default_color) const;
  SkColor GetControlsLayerColor(ControlsLayerType type,
                                AshColorMode given_color_mode) const;

  // Gets color of Content layer. See details at the corresponding function of
  // Base layer.
  SkColor DeprecatedGetContentLayerColor(ContentLayerType type,
                                         SkColor default_color) const;
  SkColor GetContentLayerColor(ContentLayerType type,
                               AshColorMode given_color_mode) const;

  // Gets the attributes of ripple on |bg_color|. |bg_color| is the background
  // color of the UI element that wants to show inkdrop.
  RippleAttributes GetRippleAttributes(SkColor bg_color) const;

  AshColorMode color_mode() const { return color_mode_; }

 private:
  // Gets Shield layer color on |type| and |color_mode|. This function will be
  // merged into GetShieldLayerColor after DeprecatedGetShieldLayerColor got be
  // removed.
  SkColor GetShieldLayerColorImpl(ShieldLayerType type,
                                  AshColorMode color_mode) const;

  // Gets Base layer color on |type| and |color_mode|. This function will be
  // merged into GetBaseLayerColor after DeprecatedGetBaseLayerColor got be
  // removed.
  SkColor GetBaseLayerColorImpl(BaseLayerType type,
                                AshColorMode color_mode) const;

  // Gets Controls layer color on |type| and |color_mode|. This function will be
  // merged into GetControlsLayerColor after DeprecatedGetControlsLayerColor got
  // be removed.
  SkColor GetControlsLayerColorImpl(ControlsLayerType type,
                                    AshColorMode color_mode) const;

  // Gets Content layer color on |type| and |color_mode|. This function will be
  // merged into GetContentLayerColor after DeprecatedGetContentLayerColor got
  // be removed.
  SkColor GetContentLayerColorImpl(ContentLayerType type,
                                   AshColorMode color_mode) const;

  // Current color mode of system UI.
  AshColorMode color_mode_ = AshColorMode::kDefault;

  DISALLOW_COPY_AND_ASSIGN(AshColorProvider);
};

}  // namespace ash

#endif  // ASH_STYLE_ASH_COLOR_PROVIDER_H_
