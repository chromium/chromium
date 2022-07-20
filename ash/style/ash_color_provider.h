// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ASH_COLOR_PROVIDER_H_
#define ASH_STYLE_ASH_COLOR_PROVIDER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/callback_helpers.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace ui {
class ColorProvider;
}

namespace ash {

// TODO(minch): AshColorProvider is not needed to be a class now.
// The color provider for system UI. It provides colors for Shield layer, Base
// layer, Controls layer and Content layer. Shield layer is a combination of
// color, opacity and blur which may change depending on the context, it is
// usually a fullscreen layer. e.g, PowerButtoneMenuScreenView for power button
// menu. Base layer is the bottom layer of any UI displayed on top of all other
// UIs. e.g, the ShelfView that contains all the shelf items. Controls layer is
// where components such as icons and inkdrops lay on, it may also indicate the
// state of an interactive element (active/inactive states). Content layer means
// the UI elements, e.g., separator, text, icon. The color of an element in
// system UI will be the combination of the colors of the four layers.
class ASH_EXPORT AshColorProvider : public ColorProvider {
 public:
  AshColorProvider();
  AshColorProvider(const AshColorProvider& other) = delete;
  AshColorProvider operator=(const AshColorProvider& other) = delete;
  ~AshColorProvider() override;

  static AshColorProvider* Get();

  // Gets the disabled color on |enabled_color|. It can be disabled background,
  // an disabled icon, etc.
  static SkColor GetDisabledColor(SkColor enabled_color);

  // Gets the color of second tone on the given |color_of_first_tone|. e.g,
  // power status icon inside status area is a dual tone icon.
  static SkColor GetSecondToneColor(SkColor color_of_first_tone);

  // ColorProvider:
  SkColor GetShieldLayerColor(ShieldLayerType type) const override;
  SkColor GetBaseLayerColor(BaseLayerType type) const override;
  SkColor GetControlsLayerColor(ControlsLayerType type) const override;
  SkColor GetContentLayerColor(ContentLayerType type) const override;
  SkColor GetActiveDialogTitleBarColor() const override;
  SkColor GetInactiveDialogTitleBarColor() const override;
  std::pair<SkColor, float> GetInkDropBaseColorAndOpacity(
      SkColor background_color = gfx::kPlaceholderColor) const override;
  std::pair<SkColor, float> GetInvertedInkDropBaseColorAndOpacity(
      SkColor background_color = gfx::kPlaceholderColor) const override;

  // Gets the color of |type| of the corresponding layer based on the current
  // inverted color mode. For views that need LIGHT colors while DARK mode is
  // active, and vice versa.
  SkColor GetInvertedBaseLayerColor(BaseLayerType type) const;

  // Gets the background color that can be applied on any layer. The returned
  // color will be different based on color mode and color theme (see
  // |is_themed_|).
  SkColor GetBackgroundColor() const;
  // Same as above, but returns the color based on the current inverted color
  // mode and color theme.
  SkColor GetInvertedBackgroundColor() const;
  // Gets the background color in the desired color mode dark/light.
  SkColor GetBackgroundColorInMode(bool use_dark_color) const;

 private:
  // Gets the color of |type| of the corresponding layer. Returns color based on
  // the current inverted color mode if |inverted| is true.
  SkColor GetShieldLayerColorImpl(ShieldLayerType type, bool inverted) const;
  SkColor GetBaseLayerColorImpl(BaseLayerType type, bool inverted) const;
  // Gets the color of |type| of the corresponding layer. Returns the color on
  // dark mode if |use_dark_color| is true.
  SkColor GetControlsLayerColorImpl(ControlsLayerType type) const;
  SkColor GetContentLayerColorImpl(ContentLayerType type,
                                   bool use_dark_color) const;

  // Gets the background default color based on the current color mode.
  SkColor GetBackgroundDefaultColor() const;
  // Gets the background default color based on the current inverted color mode.
  SkColor GetInvertedBackgroundDefaultColor() const;

  // Gets the background themed color that's calculated based on the color
  // extracted from wallpaper. For dark mode, it will be dark muted wallpaper
  // prominent color + SK_ColorBLACK 50%. For light mode, it will be light
  // muted wallpaper prominent color + SK_ColorWHITE 75%. Extracts the color on
  // dark mode if |use_dark_color| is true.
  SkColor GetBackgroundThemedColorImpl(SkColor default_color,
                                       bool use_dark_color) const;

  // Returns a ColorProvider for the current NativeTheme which will correctly
  // reflect the current ColorMode.
  ui::ColorProvider* GetColorProvider() const;
};

}  // namespace ash

#endif  // ASH_STYLE_ASH_COLOR_PROVIDER_H_
