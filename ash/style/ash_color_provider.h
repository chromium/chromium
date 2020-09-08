// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ASH_COLOR_PROVIDER_H_
#define ASH_STYLE_ASH_COLOR_PROVIDER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/observer_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/vector_icon_types.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace views {
class ImageButton;
class LabelButton;
}  // namespace views

namespace ash {
class ColorModeObserver;

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
class ASH_EXPORT AshColorProvider : public SessionObserver {
 public:
  // TODO(minch): Remove AshColorMode, |color_mode_| and DeprecatedGet*
  // functions once all the deprecated colors have been removed.
  // The color mode of system UI, which can be set through the dark mode feature
  // pod in the system tray menu.
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

  // Types of Shield layer. Number at the end of each type indicates the alpha
  // value.
  enum class ShieldLayerType {
    kShield20 = 0,
    kShield40,
    kShield60,
    kShield80,
    kShield90,
  };

  // Blur sigma for system UI layers.
  enum class LayerBlurSigma {
    kBlurDefault = 30,  // Default blur sigma is 30.
    kBlurSigma20 = 20,
    kBlurSigma10 = 10,
  };

  // Types of Base layer.
  enum class BaseLayerType {
    // Number at the end of each transparent type indicates the alpha value.
    kTransparent20 = 0,
    kTransparent40,
    kTransparent60,
    kTransparent80,
    kTransparent90,

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
    kFocusRingColor,
  };

  enum class ContentLayerType {
    kSeparatorColor,

    kTextColorPrimary,
    kTextColorSecondary,
    kTextColorAlert,
    kTextColorWarning,
    kTextColorPositive,

    kIconColorPrimary,
    kIconColorSecondary,
    kIconColorAlert,
    kIconColorWarning,
    kIconColorPositive,
    // Color for prominent icon, e.g, "Add connection" icon button inside
    // VPN detailed view.
    kIconColorProminent,

    // The default color for button labels.
    kButtonLabelColor,
    kButtonLabelColorPrimary,

    kButtonIconColor,
    kButtonIconColorPrimary,

    // Color for system menu icon buttons with inverted dark mode colors, e.g,
    // FeaturePodIconButton
    kSystemMenuIconColor,
    kSystemMenuIconColorToggled,

    // Color for sliders (volume, brightness etc.)
    kSliderThumbColorEnabled,
    kSliderThumbColorDisabled,

    // Color for app state indicator.
    kAppStateIndicatorColor,
    kAppStateIndicatorColorInactive,
  };

  // Types of ash styled buttons.
  enum class ButtonType {
    kPillButtonWithIcon,
    kCloseButtonWithSmallBase,
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

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Gets color of Shield layer. See details at the corresponding function of
  // Base layer.
  SkColor DeprecatedGetShieldLayerColor(ShieldLayerType type,
                                        SkColor default_color) const;
  SkColor GetShieldLayerColor(ShieldLayerType type) const;

  // Used by UI elements that need to support |kDefault| mode to get the color
  // of base layer. Deprecated. Remove once all the deprecated calls are
  // converted to light/dark mode colors.
  SkColor DeprecatedGetBaseLayerColor(BaseLayerType type,
                                      SkColor default_color) const;
  // Gets base layer's color of |type|.
  SkColor GetBaseLayerColor(BaseLayerType type) const;

  // Gets color of Controls layer. See details at the corresponding function of
  // Base layer.
  SkColor DeprecatedGetControlsLayerColor(ControlsLayerType type,
                                          SkColor default_color) const;
  SkColor GetControlsLayerColor(ControlsLayerType type) const;

  // Gets color of Content layer. See details at the corresponding function of
  // Base layer.
  SkColor DeprecatedGetContentLayerColor(ContentLayerType type,
                                         SkColor default_color) const;
  SkColor GetContentLayerColor(ContentLayerType type) const;

  // Gets the attributes of ripple on |bg_color|. |bg_color| is the background
  // color of the UI element that wants to show inkdrop.
  RippleAttributes GetRippleAttributes(SkColor bg_color) const;

  // Gets the background color that can be applied on any layer. The returned
  // color will be different based on color mode and color theme (see
  // |is_themed_|).
  SkColor GetBackgroundColor() const;

  // Ink drop color for shelf items.
  SkColor GetInkDropBaseColor(AshColorMode given_color_mode) const;

  // Opacity of the ink drop ripple for shelf items when the ripple is visible.
  float GetInkDropVisibleOpacity() const;

  // Helpers to style buttons based on the desired |type| and theme. Depending
  // on the type may style text, icon and background colors for both enabled and
  // disabled states. May overwrite an prior styles on |button|.
  void DecoratePillButton(views::LabelButton* button,
                          ButtonType type,
                          const gfx::VectorIcon& icon);
  void DecorateCloseButton(views::ImageButton* button,
                           ButtonType type,
                           int button_size,
                           const gfx::VectorIcon& icon);

  void AddObserver(ColorModeObserver* observer);
  void RemoveObserver(ColorModeObserver* observer);

  // True if pref |kDarkModeEnabled| is true, which means the current color mode
  // is dark.
  bool IsDarkModeEnabled() const;

  // Whether the system color mode is themed, by default is true. If true, the
  // background color will be calculated based on extracted wallpaper color.
  bool IsThemed() const;

  // Toggles pref |kDarkModeEnabled|.
  void ToggleColorMode();

  // Updates pref |kColorModeThemed| to |is_themed|.
  void UpdateColorModeThemed(bool is_themed);

  // Gets the background base color for login screen.
  SkColor GetLoginBackgroundBaseColor() const;

 private:
  // Gets the background default color.
  SkColor GetBackgroundDefaultColor() const;

  // Gets the background themed color that's calculated based on the color
  // extracted from wallpaper. For dark mode, it will be dark muted wallpaper
  // prominent color + SK_ColorBLACK 50%. For light mode, it will be light
  // muted wallpaper prominent color + SK_ColorWHITE 75%.
  SkColor GetBackgroundThemedColor() const;

  // Notifies all the observers on |kDarkModeEnabled|'s change.
  void NotifyDarkModeEnabledPrefChange();

  // Notifies all the observers on |kColorModeThemed|'s change.
  void NotifyColorModeThemedPrefChange();

  // Current color mode of system UI.
  AshColorMode color_mode_ = AshColorMode::kDefault;

  base::ObserverList<ColorModeObserver> observers_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  PrefService* active_user_pref_service_ = nullptr;  // Not owned.
};

}  // namespace ash

#endif  // ASH_STYLE_ASH_COLOR_PROVIDER_H_
