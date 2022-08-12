// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include <math.h>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

namespace ash {

using ColorName = cros_styles::ColorName;

namespace {

// Opacity of the light/dark indrop.
constexpr float kLightInkDropOpacity = 0.08f;
constexpr float kDarkInkDropOpacity = 0.06f;

// The disabled color is always 38% opacity of the enabled color.
constexpr float kDisabledColorOpacity = 0.38f;

// Color of second tone is always 30% opacity of the color of first tone.
constexpr float kSecondToneOpacity = 0.3f;

// Different alpha values that can be used by Shield and Base layers.
constexpr int kAlpha20 = 51;   // 20%
constexpr int kAlpha40 = 102;  // 40%
constexpr int kAlpha60 = 153;  // 60%
constexpr int kAlpha80 = 204;  // 80%
constexpr int kAlpha90 = 230;  // 90%
constexpr int kAlpha95 = 242;  // 95%

// Alpha value that is used to calculate themed color. Please see function
// GetBackgroundThemedColor() about how the themed color is calculated.
constexpr int kDarkBackgroundBlendAlpha = 127;   // 50%
constexpr int kLightBackgroundBlendAlpha = 127;  // 50%

AshColorProvider* g_instance = nullptr;

// Get the corresponding ColorName for |type|. ColorName is an enum in
// cros_styles.h file that is generated from cros_colors.json5, which
// includes the color IDs and colors that will be used by ChromeOS WebUI.
ColorName TypeToColorName(AshColorProvider::ContentLayerType type) {
  switch (type) {
    case AshColorProvider::ContentLayerType::kTextColorPrimary:
      return ColorName::kTextColorPrimary;
    case AshColorProvider::ContentLayerType::kTextColorSecondary:
      return ColorName::kTextColorSecondary;
    case AshColorProvider::ContentLayerType::kTextColorAlert:
      return ColorName::kTextColorAlert;
    case AshColorProvider::ContentLayerType::kTextColorWarning:
      return ColorName::kTextColorWarning;
    case AshColorProvider::ContentLayerType::kTextColorPositive:
      return ColorName::kTextColorPositive;
    case AshColorProvider::ContentLayerType::kIconColorPrimary:
      return ColorName::kIconColorPrimary;
    case AshColorProvider::ContentLayerType::kIconColorAlert:
      return ColorName::kIconColorAlert;
    case AshColorProvider::ContentLayerType::kIconColorWarning:
      return ColorName::kIconColorWarning;
    case AshColorProvider::ContentLayerType::kIconColorPositive:
      return ColorName::kIconColorPositive;
    default:
      DCHECK_EQ(AshColorProvider::ContentLayerType::kIconColorProminent, type);
      return ColorName::kIconColorProminent;
  }
}

// Get the color from cros_styles.h header file that is generated from
// cros_colors.json5. Colors there will also be used by ChromeOS WebUI.
SkColor ResolveColor(AshColorProvider::ContentLayerType type,
                     bool use_dark_color) {
  return cros_styles::ResolveColor(TypeToColorName(type), use_dark_color);
}

bool IsDarkModeEnabled() {
  // May be null in unit tests.
  if (!Shell::HasInstance())
    return true;
  return Shell::Get()->dark_light_mode_controller()->IsDarkModeEnabled();
}

}  // namespace

AshColorProvider::AshColorProvider() {
  DCHECK(!g_instance);
  g_instance = this;
}

AshColorProvider::~AshColorProvider() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AshColorProvider* AshColorProvider::Get() {
  return g_instance;
}

// static
SkColor AshColorProvider::GetDisabledColor(SkColor enabled_color) {
  return SkColorSetA(enabled_color, std::round(SkColorGetA(enabled_color) *
                                               kDisabledColorOpacity));
}

// static
SkColor AshColorProvider::GetSecondToneColor(SkColor color_of_first_tone) {
  return SkColorSetA(
      color_of_first_tone,
      std::round(SkColorGetA(color_of_first_tone) * kSecondToneOpacity));
}

SkColor AshColorProvider::GetShieldLayerColor(ShieldLayerType type) const {
  return GetShieldLayerColorImpl(type, /*inverted=*/false);
}

SkColor AshColorProvider::GetBaseLayerColor(BaseLayerType type) const {
  return GetBaseLayerColorImpl(type, /*inverted=*/false);
}

SkColor AshColorProvider::GetControlsLayerColor(ControlsLayerType type) const {
  // TODO(skau): Delete this function
  return GetControlsLayerColorImpl(type);
}

SkColor AshColorProvider::GetContentLayerColor(ContentLayerType type) const {
  return GetContentLayerColorImpl(type, IsDarkModeEnabled());
}

SkColor AshColorProvider::GetActiveDialogTitleBarColor() const {
  return cros_styles::ResolveColor(cros_styles::ColorName::kDialogTitleBarColor,
                                   IsDarkModeEnabled());
}

SkColor AshColorProvider::GetInactiveDialogTitleBarColor() const {
  // TODO(wenbojie): Use a different inactive color in future.
  return GetActiveDialogTitleBarColor();
}

std::pair<SkColor, float> AshColorProvider::GetInkDropBaseColorAndOpacity(
    SkColor background_color) const {
  if (background_color == gfx::kPlaceholderColor)
    background_color = GetBackgroundColor();

  const bool is_dark = color_utils::IsDark(background_color);
  const SkColor base_color = is_dark ? SK_ColorWHITE : SK_ColorBLACK;
  const float opacity = is_dark ? kLightInkDropOpacity : kDarkInkDropOpacity;
  return std::make_pair(base_color, opacity);
}

std::pair<SkColor, float>
AshColorProvider::GetInvertedInkDropBaseColorAndOpacity(
    SkColor background_color) const {
  if (background_color == gfx::kPlaceholderColor)
    background_color = GetBackgroundColor();

  const bool is_light = !color_utils::IsDark(background_color);
  const SkColor base_color = is_light ? SK_ColorWHITE : SK_ColorBLACK;
  const float opacity = is_light ? kLightInkDropOpacity : kDarkInkDropOpacity;
  return std::make_pair(base_color, opacity);
}

SkColor AshColorProvider::GetInvertedBaseLayerColor(BaseLayerType type) const {
  return GetBaseLayerColorImpl(type, /*inverted=*/true);
}

SkColor AshColorProvider::GetBackgroundColor() const {
  return GetBackgroundThemedColorImpl(GetBackgroundDefaultColor(),
                                      IsDarkModeEnabled());
}

SkColor AshColorProvider::GetInvertedBackgroundColor() const {
  return GetBackgroundThemedColorImpl(GetInvertedBackgroundDefaultColor(),
                                      !IsDarkModeEnabled());
}

SkColor AshColorProvider::GetBackgroundColorInMode(bool use_dark_color) const {
  return cros_styles::ResolveColor(cros_styles::ColorName::kBgColor,
                                   use_dark_color);
}

SkColor AshColorProvider::GetShieldLayerColorImpl(ShieldLayerType type,
                                                  bool inverted) const {
  constexpr int kAlphas[] = {kAlpha20, kAlpha40, kAlpha60,
                             kAlpha80, kAlpha90, kAlpha95};
  DCHECK_LT(static_cast<size_t>(type), std::size(kAlphas));
  return SkColorSetA(
      inverted ? GetInvertedBackgroundColor() : GetBackgroundColor(),
      kAlphas[static_cast<int>(type)]);
}

SkColor AshColorProvider::GetBaseLayerColorImpl(BaseLayerType type,
                                                bool inverted) const {
  constexpr int kAlphas[] = {kAlpha20, kAlpha40, kAlpha60, kAlpha80,
                             kAlpha90, kAlpha95, 0xFF};
  DCHECK_LT(static_cast<size_t>(type), std::size(kAlphas));
  return SkColorSetA(
      inverted ? GetInvertedBackgroundColor() : GetBackgroundColor(),
      kAlphas[static_cast<int>(type)]);
}

SkColor AshColorProvider::GetControlsLayerColorImpl(
    ControlsLayerType type) const {
  // TODO(crbug.com/1292244): Delete this function after all callers migrate.
  auto* color_provider = GetColorProvider();
  DCHECK(color_provider);

  switch (type) {
    case ControlsLayerType::kHairlineBorderColor:
      return color_provider->GetColor(kColorAshHairlineBorderColor);
    case ControlsLayerType::kControlBackgroundColorActive:
      return color_provider->GetColor(kColorAshControlBackgroundColorActive);
    case ControlsLayerType::kControlBackgroundColorInactive:
      return color_provider->GetColor(kColorAshControlBackgroundColorInactive);
    case ControlsLayerType::kControlBackgroundColorAlert:
      return color_provider->GetColor(kColorAshControlBackgroundColorAlert);
    case ControlsLayerType::kControlBackgroundColorWarning:
      return color_provider->GetColor(kColorAshControlBackgroundColorWarning);
    case ControlsLayerType::kControlBackgroundColorPositive:
      return color_provider->GetColor(kColorAshControlBackgroundColorPositive);
    case ControlsLayerType::kFocusAuraColor:
      return color_provider->GetColor(kColorAshFocusAuraColor);
    case ControlsLayerType::kFocusRingColor:
      return color_provider->GetColor(ui::kColorAshFocusRing);
    case ControlsLayerType::kHighlightColor1:
      return color_provider->GetColor(ui::kColorHighlightBorderHighlight1);
    case ControlsLayerType::kHighlightColor2:
      return color_provider->GetColor(ui::kColorHighlightBorderHighlight2);
    case ControlsLayerType::kHighlightColor3:
      return color_provider->GetColor(ui::kColorHighlightBorderHighlight3);
    case ControlsLayerType::kBorderColor1:
      return color_provider->GetColor(ui::kColorHighlightBorderBorder1);
    case ControlsLayerType::kBorderColor2:
      return color_provider->GetColor(ui::kColorHighlightBorderBorder2);
    case ControlsLayerType::kBorderColor3:
      return color_provider->GetColor(ui::kColorHighlightBorderBorder3);
  }
}

SkColor AshColorProvider::GetContentLayerColorImpl(ContentLayerType type,
                                                   bool use_dark_color) const {
  switch (type) {
    case ContentLayerType::kSeparatorColor:
    case ContentLayerType::kShelfHandleColor:
      return use_dark_color ? SkColorSetA(SK_ColorWHITE, 0x24)
                            : SkColorSetA(SK_ColorBLACK, 0x24);
    case ContentLayerType::kIconColorSecondary:
      return gfx::kGoogleGrey500;
    case ContentLayerType::kIconColorSecondaryBackground:
      return use_dark_color ? gfx::kGoogleGrey100 : gfx::kGoogleGrey800;
    case ContentLayerType::kScrollBarColor:
    case ContentLayerType::kSliderColorInactive:
    case ContentLayerType::kRadioColorInactive:
      return use_dark_color ? gfx::kGoogleGrey200 : gfx::kGoogleGrey700;
    case ContentLayerType::kSwitchKnobColorInactive:
      return use_dark_color ? gfx::kGoogleGrey400 : SK_ColorWHITE;
    case ContentLayerType::kSwitchTrackColorInactive:
      return GetSecondToneColor(use_dark_color ? gfx::kGoogleGrey200
                                               : gfx::kGoogleGrey700);
    case ContentLayerType::kButtonLabelColorBlue:
    case ContentLayerType::kTextColorURL:
    case ContentLayerType::kSliderColorActive:
    case ContentLayerType::kRadioColorActive:
    case ContentLayerType::kSwitchKnobColorActive:
    case ContentLayerType::kProgressBarColorForeground:
      return use_dark_color ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600;
    case ContentLayerType::kProgressBarColorBackground:
    case ContentLayerType::kCaptureRegionColor:
      return SkColorSetA(
          use_dark_color ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600, 0x4C);
    case ContentLayerType::kSwitchTrackColorActive:
      return GetSecondToneColor(GetContentLayerColorImpl(
          ContentLayerType::kSwitchKnobColorActive, use_dark_color));
    case ContentLayerType::kButtonLabelColorPrimary:
    case ContentLayerType::kButtonIconColorPrimary:
    case ContentLayerType::kBatteryBadgeColor:
      return use_dark_color ? gfx::kGoogleGrey900 : gfx::kGoogleGrey200;
    case ContentLayerType::kAppStateIndicatorColorInactive:
      return GetDisabledColor(GetContentLayerColorImpl(
          ContentLayerType::kAppStateIndicatorColor, use_dark_color));
    case ContentLayerType::kCurrentDeskColor:
      return use_dark_color ? SK_ColorWHITE : SK_ColorBLACK;
    case ContentLayerType::kSwitchAccessInnerStrokeColor:
      return gfx::kGoogleBlue300;
    case ContentLayerType::kSwitchAccessOuterStrokeColor:
      return gfx::kGoogleBlue900;
    case ContentLayerType::kHighlightColorHover:
      return use_dark_color ? SkColorSetA(SK_ColorWHITE, 0x0D)
                            : SkColorSetA(SK_ColorBLACK, 0x14);
    case ContentLayerType::kAppStateIndicatorColor:
    case ContentLayerType::kButtonIconColor:
    case ContentLayerType::kButtonLabelColor:
      return use_dark_color ? gfx::kGoogleGrey200 : gfx::kGoogleGrey900;
    case ContentLayerType::kBatterySystemInfoBackgroundColor:
      return use_dark_color ? gfx::kGoogleGreen300 : gfx::kGoogleGreen600;
    case ContentLayerType::kBatterySystemInfoIconColor:
    case ContentLayerType::kInvertedTextColorPrimary:
    case ContentLayerType::kInvertedButtonLabelColor:
      return use_dark_color ? gfx::kGoogleGrey900 : gfx::kGoogleGrey200;
    default:
      return ResolveColor(type, use_dark_color);
  }
}

SkColor AshColorProvider::GetBackgroundDefaultColor() const {
  return GetBackgroundColorInMode(IsDarkModeEnabled());
}

SkColor AshColorProvider::GetInvertedBackgroundDefaultColor() const {
  return GetBackgroundColorInMode(!IsDarkModeEnabled());
}

SkColor AshColorProvider::GetBackgroundThemedColorImpl(
    SkColor default_color,
    bool use_dark_color) const {
  // May be null in unit tests.
  if (!Shell::HasInstance())
    return default_color;
  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  if (!wallpaper_controller)
    return default_color;

  color_utils::LumaRange luma_range = use_dark_color
                                          ? color_utils::LumaRange::DARK
                                          : color_utils::LumaRange::LIGHT;
  SkColor muted_color =
      wallpaper_controller->GetProminentColor(color_utils::ColorProfile(
          luma_range, color_utils::SaturationRange::MUTED));
  if (muted_color == kInvalidWallpaperColor)
    return default_color;

  return color_utils::GetResultingPaintColor(
      SkColorSetA(use_dark_color ? SK_ColorBLACK : SK_ColorWHITE,
                  use_dark_color ? kDarkBackgroundBlendAlpha
                                 : kLightBackgroundBlendAlpha),
      muted_color);
}

ui::ColorProvider* AshColorProvider::GetColorProvider() const {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      native_theme->GetColorProviderKey(nullptr));
}

}  // namespace ash
