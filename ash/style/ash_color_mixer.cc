// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_mixer.h"

#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/harmonized_colors.h"
#include "ash/style/style_util.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ash {

namespace {

constexpr int kAlpha20 = SK_AlphaOPAQUE * 0.2f;
constexpr int kAlpha25 = SK_AlphaOPAQUE * 0.25f;
constexpr int kAlpha40 = SK_AlphaOPAQUE * 0.4f;
constexpr int kAlpha60 = SK_AlphaOPAQUE * 0.6f;
constexpr int kAlpha80 = SK_AlphaOPAQUE * 0.8f;
constexpr int kAlpha90 = SK_AlphaOPAQUE * 0.9f;
constexpr int kAlpha95 = SK_AlphaOPAQUE * 0.95f;

// Color of second tone is always 30% opacity of the color of first tone.
constexpr int kSecondToneOpacity = SK_AlphaOPAQUE * 0.3f;

// The disabled color is always 38% opacity of the enabled color.
constexpr int kDisabledColorOpacity = SK_AlphaOPAQUE * 0.38f;

void AddShieldAndBaseColors(ui::ColorMixer& mixer,
                            const ui::ColorProviderManager::Key& key) {
  if (chromeos::features::IsJellyEnabled()) {
    // Generally, shield and base colors are cros.sys.sys-base-elevated.  That
    // is cros.sys.surface3 @ 90%.  So, map all shield colors to surface3 and
    // keep all the opacities.
    //
    // New users should use cros.sys.sys-base-elevated directly.
    mixer[kColorAshShieldAndBase20] =
        ui::SetAlpha(cros_tokens::kCrosSysSurface3, kAlpha20);
    mixer[kColorAshShieldAndBase40] =
        ui::SetAlpha(cros_tokens::kCrosSysSurface3, kAlpha40);
    mixer[kColorAshShieldAndBase60] =
        ui::SetAlpha(cros_tokens::kCrosSysSurface3, kAlpha60);
    mixer[kColorAshShieldAndBase80] =
        ui::SetAlpha(cros_tokens::kCrosSysSurface3, kAlpha80);
    mixer[kColorAshShieldAndBase90] =
        ui::SetAlpha(cros_tokens::kCrosSysSurface3, kAlpha90);
    mixer[kColorAshShieldAndBase95] =
        ui::SetAlpha(cros_tokens::kCrosSysSurface3, kAlpha95);
    mixer[kColorAshShieldAndBaseOpaque] = {cros_tokens::kCrosSysSurface3};
    return;
  }

  const bool use_dark_color =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;

  // Colors of the Shield and Base layers.
  const SkColor default_background_color =
      use_dark_color ? gfx::kGoogleGrey900 : SK_ColorWHITE;
  // TODO(minch|skau): Investigate/fix whether should DCHECK the existence of
  // the value of `use_color` here.
  const SkColor background_color =
      key.user_color.value_or(default_background_color);

  mixer[kColorAshShieldAndBase20] = {SkColorSetA(background_color, kAlpha20)};
  mixer[kColorAshShieldAndBase40] = {SkColorSetA(background_color, kAlpha40)};
  mixer[kColorAshShieldAndBase60] = {SkColorSetA(background_color, kAlpha60)};
  mixer[kColorAshShieldAndBase80] = {SkColorSetA(background_color, kAlpha80)};
  mixer[kColorAshInvertedShieldAndBase80] = {
      SkColorSetA(color_utils::InvertColor(background_color), kAlpha80)};
  mixer[kColorAshShieldAndBase90] = {SkColorSetA(background_color, kAlpha90)};
  mixer[kColorAshShieldAndBase95] = {SkColorSetA(background_color, kAlpha95)};
  mixer[kColorAshShieldAndBaseOpaque] = {
      SkColorSetA(background_color, SK_AlphaOPAQUE)};

  // TODO(b/270468758): Remove when the last caller has been deleted.
  mixer[kColorAshShieldAndBase80Light] = {SkColorSetA(SK_ColorWHITE, kAlpha80)};
}

// Mappings of Controls Colors for Material 2.
void AddControlsColors(ui::ColorMixer& mixer,
                       const ui::ColorProviderManager::Key& key) {
  const bool use_dark_color =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;

  // ControlsLayer colors
  mixer[kColorAshHairlineBorderColor] =
      use_dark_color ? ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x24))
                     : ui::ColorTransform(SkColorSetA(SK_ColorBLACK, 0x24));
  mixer[kColorAshControlBackgroundColorActive] = {cros_tokens::kColorProminent};
  mixer[kColorAshControlBackgroundColorInactive] =
      use_dark_color ? ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x1A))
                     : ui::ColorTransform(SkColorSetA(SK_ColorBLACK, 0x0D));
  mixer[kColorAshControlBackgroundColorAlert] = {cros_tokens::kColorAlert};
  mixer[kColorAshControlBackgroundColorWarning] = {cros_tokens::kColorWarning};
  mixer[kColorAshControlBackgroundColorPositive] = {
      cros_tokens::kColorPositive};
  mixer[kColorAshFocusAuraColor] =
      ui::SetAlpha(cros_tokens::kColorProminent, 0x3D);
  mixer[ui::kColorAshFocusRing] = {cros_tokens::kColorProminent};
}

// Mappings the Content layer colors for Material 2.
void AddContentColors(ui::ColorMixer& mixer,
                      const ui::ColorProviderManager::Key& key) {
  const bool use_dark_color =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;

  // ContentLayer colors.
  mixer[kColorAshScrollBarColor] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleGrey200)
                     : ui::ColorTransform(gfx::kGoogleGrey700);
  mixer[kColorAshSeparatorColor] =
      use_dark_color ? ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x24))
                     : ui::ColorTransform(SkColorSetA(SK_ColorBLACK, 0x24));
  mixer[kColorAshTextColorPrimary] = {cros_tokens::kColorPrimary};
  mixer[kColorAshTextColorSecondary] = {cros_tokens::kTextColorSecondary};
  mixer[kColorAshTextColorAlert] = {cros_tokens::kColorAlert};
  mixer[kColorAshTextColorWarning] = {cros_tokens::kColorWarning};
  mixer[kColorAshTextColorPositive] = {cros_tokens::kColorPositive};
  mixer[kColorAshTextColorURL] = {cros_tokens::kColorProminent};
  mixer[kColorAshIconColorPrimary] = {kColorAshTextColorPrimary};
  mixer[kColorAshIconColorSecondary] = {cros_tokens::kColorSecondary};
  mixer[kColorAshIconColorAlert] = {kColorAshTextColorAlert};
  mixer[kColorAshIconColorWarning] = {kColorAshTextColorWarning};
  mixer[kColorAshIconColorPositive] = {kColorAshTextColorPositive};
  mixer[kColorAshIconColorProminent] = {kColorAshTextColorURL};
  mixer[kColorAshIconColorSecondaryBackground] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleGrey100)
                     : ui::ColorTransform(gfx::kGoogleGrey800);
  mixer[kColorAshButtonLabelColor] = {kColorAshTextColorPrimary};
  mixer[kColorAshButtonLabelColorLight] = {cros_tokens::kColorSecondaryLight};
  mixer[kColorAshButtonLabelColorPrimary] = {
      cros_tokens::kColorPrimaryInverted};
  mixer[kColorAshTextOnBackgroundColor] = {cros_tokens::kColorPrimaryInverted};
  mixer[kColorAshIconOnBackgroundColor] = {cros_tokens::kColorPrimaryInverted};
  mixer[kColorAshInvertedTextColorPrimary] = {kColorAshButtonLabelColorPrimary};
  mixer[kColorAshInvertedButtonLabelColor] = {kColorAshButtonLabelColorPrimary};
  mixer[kColorAshTextColorSuggestion] = {cros_tokens::kColorDisabled};
  mixer[kColorAshButtonLabelColorBlue] = {kColorAshTextColorURL};
  mixer[kColorAshButtonIconColor] = {kColorAshTextColorPrimary};
  mixer[kColorAshButtonIconColorLight] = {cros_tokens::kColorSecondaryLight};
  mixer[kColorAshButtonIconColorPrimary] = {kColorAshButtonLabelColorPrimary};
  mixer[kColorAshAppStateIndicatorColor] = {kColorAshTextColorPrimary};
  mixer[kColorAshAppStateIndicatorColorInactive] =
      ui::SetAlpha(kColorAshAppStateIndicatorColor, kDisabledColorOpacity);
  mixer[kColorAshShelfHandleColor] = {cros_tokens::kIconColorSecondary};
  mixer[kColorAshShelfTooltipBackgroundColor] = {
      chromeos::features::IsJellyEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
          : kColorAshInvertedShieldAndBase80};
  mixer[kColorAshShelfTooltipForegroundColor] = {
      chromeos::features::IsJellyEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysInverseOnSurface)
          : cros_tokens::kTextColorPrimaryInverted};
  mixer[kColorAshSliderColorActive] = {kColorAshTextColorURL};
  mixer[kColorAshSliderColorInactive] = {kColorAshScrollBarColor};
  mixer[kColorAshRadioColorActive] = {kColorAshTextColorURL};
  mixer[kColorAshRadioColorInactive] = {kColorAshScrollBarColor};
  mixer[kColorAshSwitchKnobColorActive] = {kColorAshTextColorURL};
  mixer[kColorAshSwitchKnobColorInactive] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleGrey400)
                     : ui::ColorTransform(SK_ColorWHITE);
  mixer[kColorAshSwitchTrackColorActive] =
      ui::SetAlpha(kColorAshSwitchKnobColorActive, kSecondToneOpacity);
  mixer[kColorAshSwitchTrackColorInactive] =
      ui::SetAlpha(kColorAshScrollBarColor, kSecondToneOpacity);
  mixer[kColorAshCurrentDeskColor] = use_dark_color
                                         ? ui::ColorTransform(SK_ColorWHITE)
                                         : ui::ColorTransform(SK_ColorBLACK);
  mixer[kColorAshBatteryBadgeColor] = {kColorAshButtonLabelColorPrimary};
  mixer[kColorAshSwitchAccessInnerStrokeColor] = {
      cros_tokens::kColorProminentDark};
  mixer[kColorAshSwitchAccessOuterStrokeColor] =
      ui::ColorTransform(gfx::kGoogleBlue900);
  mixer[kColorAshProgressBarColorForeground] = {kColorAshTextColorURL};
  mixer[kColorAshProgressBarColorBackground] =
      ui::SetAlpha(kColorAshTextColorURL, 0x4C);
  mixer[kColorAshHighlightColorHover] =
      use_dark_color ? ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x0D))
                     : ui::ColorTransform(SkColorSetA(SK_ColorBLACK, 0x14));
  mixer[kColorAshBatterySystemInfoBackgroundColor] = {
      kColorAshTextColorPositive};
  mixer[kColorAshBatterySystemInfoIconColor] = {
      kColorAshButtonLabelColorPrimary};
  mixer[kColorAshCaptureRegionColor] = {kColorAshProgressBarColorBackground};

  if (key.user_color.has_value()) {
    mixer[kColorAshInkDrop] = ui::SelectBasedOnDarkInput(
        {*key.user_color}, /*output_transform_for_dark_input=*/
        ui::SetAlpha(SK_ColorWHITE,
                     StyleUtil::kDarkInkDropOpacity * SK_AlphaOPAQUE),
        /*output_transform_for_light_input=*/
        ui::SetAlpha(SK_ColorBLACK,
                     StyleUtil::kLightInkDropOpacity * SK_AlphaOPAQUE));
    mixer[kColorAshInkDropOpaqueColor] = ui::SelectBasedOnDarkInput(
        {*key.user_color}, SK_ColorWHITE, SK_ColorBLACK);
  } else {
    // Default `user_color` is dark if color_mode is dark.
    mixer[kColorAshInkDrop] =
        use_dark_color
            ? ui::SetAlpha(SK_ColorWHITE,
                           StyleUtil::kDarkInkDropOpacity * SK_AlphaOPAQUE)
            : ui::SetAlpha(SK_ColorBLACK,
                           StyleUtil::kLightInkDropOpacity * SK_AlphaOPAQUE);
    mixer[kColorAshInkDropOpaqueColor] =
        ui::ColorTransform(use_dark_color ? SK_ColorWHITE : SK_ColorBLACK);
  }
}

// Remaps colors generated by cros_colors.json5 to point to equivalent tokens.
void RemapLegacySemanticColors(ui::ColorMixer& mixer) {
  // The colors here that have 'generate_per_mode: true' in the
  // cros_colors.json5 file need to remap the generated Light and Dark
  // variables instead of the original.
  mixer[cros_tokens::kColorPrimaryLight] = {
      cros_tokens::kCrosSysOnSurfaceLight};
  mixer[cros_tokens::kColorPrimaryDark] = {cros_tokens::kCrosSysOnSurfaceDark};

  mixer[cros_tokens::kColorSecondaryLight] = {
      cros_tokens::kCrosSysSecondaryLight};
  mixer[cros_tokens::kColorSecondaryDark] = {
      cros_tokens::kCrosSysSecondaryDark};

  mixer[cros_tokens::kColorProminentLight] = {
      cros_tokens::kCrosSysPrimaryLight};
  mixer[cros_tokens::kColorProminentDark] = {cros_tokens::kCrosSysPrimaryDark};

  mixer[cros_tokens::kColorDisabled] = {cros_tokens::kCrosSysDisabled};

  mixer[cros_tokens::kColorSelection] = {
      cros_tokens::kCrosSysOnPrimaryContainer};

  mixer[cros_tokens::kTextColorSecondaryLight] = {
      cros_tokens::kCrosSysOnSurfaceVariantLight};
  mixer[cros_tokens::kTextColorSecondaryDark] = {
      cros_tokens::kCrosSysOnSurfaceVariantDark};
  mixer[cros_tokens::kTextColorSecondary] = {
      cros_tokens::kCrosSysOnSurfaceVariant};

  mixer[cros_tokens::kBgColor] = {cros_tokens::kCrosSysAppBase};
  mixer[cros_tokens::kBgColorElevation1] = {cros_tokens::kCrosSysBaseElevated};
  mixer[cros_tokens::kBgColorElevation2Light] = {
      cros_tokens::kCrosSysBaseElevatedLight};
  mixer[cros_tokens::kBgColorElevation2Dark] = {
      cros_tokens::kCrosSysBaseElevatedDark};
  mixer[cros_tokens::kBgColorElevation3] = {cros_tokens::kCrosSysBaseElevated};
  mixer[cros_tokens::kBgColorElevation4] = {cros_tokens::kCrosSysBaseElevated};
  mixer[cros_tokens::kBgColorElevation5] = {cros_tokens::kCrosSysBaseElevated};
  mixer[cros_tokens::kBgColorDroppedElevation1] = {
      cros_tokens::kCrosSysAppBaseShaded};
  mixer[cros_tokens::kBgColorDroppedElevation2] = {
      cros_tokens::kCrosSysAppBaseShaded};

  mixer[cros_tokens::kRippleColorDark] = {cros_tokens::kCrosSysHoverOnSubtle};
  mixer[cros_tokens::kRippleColorLight] = {
      cros_tokens::kCrosSysHoverOnProminent};
  mixer[cros_tokens::kRippleColorProminent] = {
      cros_tokens::kCrosSysRipplePrimary};

  mixer[cros_tokens::kSeparatorColor] = {cros_tokens::kCrosSysSeparator};
  mixer[cros_tokens::kLinkColor] = {cros_tokens::kCrosSysPrimary};
  mixer[cros_tokens::kAppScrollbarColor] = {cros_tokens::kCrosSysScrollbar};
  mixer[cros_tokens::kAppScrollbarColorHover] = {
      cros_tokens::kCrosSysScrollbarHover};

  mixer[cros_tokens::kAppShieldColor] = {cros_tokens::kCrosSysScrim};
  mixer[cros_tokens::kAppShield20] = {cros_tokens::kCrosSysScrim};
  mixer[cros_tokens::kAppShield40] = {cros_tokens::kCrosSysScrim};
  mixer[cros_tokens::kAppShield60] = {cros_tokens::kCrosSysScrim};
  mixer[cros_tokens::kAppShield80] = {cros_tokens::kCrosSysScrim};

  mixer[cros_tokens::kHighlightColor] = {cros_tokens::kCrosSysHighlightShape};
  mixer[cros_tokens::kHighlightColorHover] = {
      cros_tokens::kCrosSysHoverOnSubtle};
  mixer[cros_tokens::kHighlightColorFocus] = {
      cros_tokens::kCrosSysRippleNeutralOnSubtle};
  mixer[cros_tokens::kHighlightColorError] = {
      cros_tokens::kCrosSysErrorContainer};
  mixer[cros_tokens::kHighlightColorGreen] = {
      cros_tokens::kCrosSysPositiveContainer};
  mixer[cros_tokens::kHighlightColorRed] = {
      cros_tokens::kCrosSysErrorContainer};
  mixer[cros_tokens::kHighlightColorYellow] = {
      cros_tokens::kCrosSysWarningContainer};
  mixer[cros_tokens::kTextHighlightColor] = {
      cros_tokens::kCrosSysHighlightText};

  mixer[cros_tokens::kButtonLabelColorSecondary] = {
      cros_tokens::kCrosSysOnPrimaryContainer};
  mixer[cros_tokens::kButtonRippleColorSecondary] = {
      cros_tokens::kCrosSysRipplePrimary};
  mixer[cros_tokens::kHighlightColor] = {cros_tokens::kCrosSysPrimary};
  mixer[cros_tokens::kTextfieldBackgroundColor] = {
      cros_tokens::kCrosSysInputFieldOnShaded};
  mixer[cros_tokens::kTextfieldLabelColor] = {cros_tokens::kCrosSysOnSurface};

  mixer[cros_tokens::kSliderColorActive] = {cros_tokens::kCrosSysPrimary};
  mixer[cros_tokens::kSliderTrackColorActive] =
      ui::SetAlpha(cros_tokens::kCrosSysPrimaryContainer,
                   0x4C);  // cros.sys.primary-container @ 30%
  mixer[cros_tokens::kSliderTrackColorInactive] = ui::SetAlpha(
      cros_tokens::kCrosSysDisabled, 0x4C);  // cros.sys.disabled @ 30%
  mixer[cros_tokens::kSliderLabelTextColor] = {cros_tokens::kCrosSysOnPrimary};
  mixer[cros_tokens::kSliderColorInactive] = {cros_tokens::kCrosSysDisabled};

  mixer[cros_tokens::kSwitchKnobColorActive] = {cros_tokens::kCrosSysOnPrimary};
  mixer[cros_tokens::kSwitchKnobColorInactive] = {
      cros_tokens::kCrosSysOnSecondary};

  mixer[cros_tokens::kSwitchTrackColorActive] = {cros_tokens::kCrosSysPrimary};
  mixer[cros_tokens::kSwitchTrackColorInactive] = {
      cros_tokens::kCrosSysSecondary};

  mixer[cros_tokens::kTooltipLabelColor] = {
      cros_tokens::kCrosSysInverseOnSurface};
  mixer[cros_tokens::kTooltipBackgroundColor] = {
      cros_tokens::kCrosSysOnSurface};

  mixer[cros_tokens::kNudgeLabelColor] = {cros_tokens::kCrosSysOnPrimary};
  mixer[cros_tokens::kNudgeIconColor] = {cros_tokens::kCrosSysOnPrimary};
  mixer[cros_tokens::kNudgeBackgroundColor] = {cros_tokens::kCrosSysPrimary};

  mixer[cros_tokens::kMenuLabelColor] = {cros_tokens::kCrosSysOnSurface};
  mixer[cros_tokens::kMenuIconColor] = {cros_tokens::kCrosSysOnSurface};
  mixer[cros_tokens::kMenuShortcutColor] = {cros_tokens::kCrosSysSecondary};
  mixer[cros_tokens::kMenuItemBackgroundHover] = {
      cros_tokens::kCrosSysHoverOnSubtle};

  // Harmonized Colors
  mixer[cros_tokens::kColorPositive] = {cros_tokens::kCrosSysPositive};
  mixer[cros_tokens::kColorWarning] = {cros_tokens::kCrosSysWarning};
  mixer[cros_tokens::kColorAlert] = {cros_tokens::kCrosSysError};
}

// Adds the dynamic color palette tokens based on user_color. This is the base
// palette so it is independent of ColorMode.
void AddRefPalette(ui::ColorMixer& mixer,
                   const ui::ColorProviderManager::Key& key) {
  // TODO(skau): Currently these colors are mapped 1-1 with the ui ref color ids
  // for compatibility with the older generated CrOS ids. Uses of these CrOS ids
  // can eventually be migrated to use the equivalent ui ids.
  mixer[cros_tokens::kCrosRefPrimary0] = {ui::kColorRefPrimary0};
  mixer[cros_tokens::kCrosRefPrimary10] = {ui::kColorRefPrimary10};
  mixer[cros_tokens::kCrosRefPrimary20] = {ui::kColorRefPrimary20};
  mixer[cros_tokens::kCrosRefPrimary30] = {ui::kColorRefPrimary30};
  mixer[cros_tokens::kCrosRefPrimary40] = {ui::kColorRefPrimary40};
  mixer[cros_tokens::kCrosRefPrimary50] = {ui::kColorRefPrimary50};
  mixer[cros_tokens::kCrosRefPrimary60] = {ui::kColorRefPrimary60};
  mixer[cros_tokens::kCrosRefPrimary70] = {ui::kColorRefPrimary70};
  mixer[cros_tokens::kCrosRefPrimary80] = {ui::kColorRefPrimary80};
  mixer[cros_tokens::kCrosRefPrimary90] = {ui::kColorRefPrimary90};
  mixer[cros_tokens::kCrosRefPrimary95] = {ui::kColorRefPrimary95};
  mixer[cros_tokens::kCrosRefPrimary99] = {ui::kColorRefPrimary99};
  mixer[cros_tokens::kCrosRefPrimary100] = {ui::kColorRefPrimary100};

  mixer[cros_tokens::kCrosRefSecondary0] = {ui::kColorRefSecondary0};
  mixer[cros_tokens::kCrosRefSecondary10] = {ui::kColorRefSecondary10};
  mixer[cros_tokens::kCrosRefSecondary15] = {ui::kColorRefSecondary15};
  mixer[cros_tokens::kCrosRefSecondary20] = {ui::kColorRefSecondary20};
  mixer[cros_tokens::kCrosRefSecondary30] = {ui::kColorRefSecondary30};
  mixer[cros_tokens::kCrosRefSecondary40] = {ui::kColorRefSecondary40};
  mixer[cros_tokens::kCrosRefSecondary50] = {ui::kColorRefSecondary50};
  mixer[cros_tokens::kCrosRefSecondary60] = {ui::kColorRefSecondary60};
  mixer[cros_tokens::kCrosRefSecondary70] = {ui::kColorRefSecondary70};
  mixer[cros_tokens::kCrosRefSecondary80] = {ui::kColorRefSecondary80};
  mixer[cros_tokens::kCrosRefSecondary90] = {ui::kColorRefSecondary90};
  mixer[cros_tokens::kCrosRefSecondary95] = {ui::kColorRefSecondary95};
  mixer[cros_tokens::kCrosRefSecondary99] = {ui::kColorRefSecondary99};
  mixer[cros_tokens::kCrosRefSecondary100] = {ui::kColorRefSecondary100};

  mixer[cros_tokens::kCrosRefTertiary0] = {ui::kColorRefTertiary0};
  mixer[cros_tokens::kCrosRefTertiary10] = {ui::kColorRefTertiary10};
  mixer[cros_tokens::kCrosRefTertiary20] = {ui::kColorRefTertiary20};
  mixer[cros_tokens::kCrosRefTertiary30] = {ui::kColorRefTertiary30};
  mixer[cros_tokens::kCrosRefTertiary40] = {ui::kColorRefTertiary40};
  mixer[cros_tokens::kCrosRefTertiary50] = {ui::kColorRefTertiary50};
  mixer[cros_tokens::kCrosRefTertiary60] = {ui::kColorRefTertiary60};
  mixer[cros_tokens::kCrosRefTertiary70] = {ui::kColorRefTertiary70};
  mixer[cros_tokens::kCrosRefTertiary80] = {ui::kColorRefTertiary80};
  mixer[cros_tokens::kCrosRefTertiary90] = {ui::kColorRefTertiary90};
  mixer[cros_tokens::kCrosRefTertiary95] = {ui::kColorRefTertiary95};
  mixer[cros_tokens::kCrosRefTertiary99] = {ui::kColorRefTertiary99};
  mixer[cros_tokens::kCrosRefTertiary100] = {ui::kColorRefTertiary100};

  mixer[cros_tokens::kCrosRefError0] = {ui::kColorRefError0};
  mixer[cros_tokens::kCrosRefError10] = {ui::kColorRefError10};
  mixer[cros_tokens::kCrosRefError20] = {ui::kColorRefError20};
  mixer[cros_tokens::kCrosRefError30] = {ui::kColorRefError30};
  mixer[cros_tokens::kCrosRefError40] = {ui::kColorRefError40};
  mixer[cros_tokens::kCrosRefError50] = {ui::kColorRefError50};
  mixer[cros_tokens::kCrosRefError60] = {ui::kColorRefError60};
  mixer[cros_tokens::kCrosRefError70] = {ui::kColorRefError70};
  mixer[cros_tokens::kCrosRefError80] = {ui::kColorRefError80};
  mixer[cros_tokens::kCrosRefError90] = {ui::kColorRefError90};
  mixer[cros_tokens::kCrosRefError95] = {ui::kColorRefError95};
  mixer[cros_tokens::kCrosRefError99] = {ui::kColorRefError99};
  mixer[cros_tokens::kCrosRefError100] = {ui::kColorRefError100};

  mixer[cros_tokens::kCrosRefNeutral0] = {ui::kColorRefNeutral0};
  mixer[cros_tokens::kCrosRefNeutral10] = {ui::kColorRefNeutral10};
  mixer[cros_tokens::kCrosRefNeutral20] = {ui::kColorRefNeutral20};
  mixer[cros_tokens::kCrosRefNeutral30] = {ui::kColorRefNeutral30};
  mixer[cros_tokens::kCrosRefNeutral40] = {ui::kColorRefNeutral40};
  mixer[cros_tokens::kCrosRefNeutral50] = {ui::kColorRefNeutral50};
  mixer[cros_tokens::kCrosRefNeutral60] = {ui::kColorRefNeutral60};
  mixer[cros_tokens::kCrosRefNeutral70] = {ui::kColorRefNeutral70};
  mixer[cros_tokens::kCrosRefNeutral80] = {ui::kColorRefNeutral80};
  mixer[cros_tokens::kCrosRefNeutral90] = {ui::kColorRefNeutral90};
  mixer[cros_tokens::kCrosRefNeutral95] = {ui::kColorRefNeutral95};
  mixer[cros_tokens::kCrosRefNeutral99] = {ui::kColorRefNeutral99};
  mixer[cros_tokens::kCrosRefNeutral100] = {ui::kColorRefNeutral100};

  mixer[cros_tokens::kCrosRefNeutralvariant0] = {ui::kColorRefNeutralVariant0};
  mixer[cros_tokens::kCrosRefNeutralvariant10] = {
      ui::kColorRefNeutralVariant10};
  mixer[cros_tokens::kCrosRefNeutralvariant20] = {
      ui::kColorRefNeutralVariant20};
  mixer[cros_tokens::kCrosRefNeutralvariant30] = {
      ui::kColorRefNeutralVariant30};
  mixer[cros_tokens::kCrosRefNeutralvariant40] = {
      ui::kColorRefNeutralVariant40};
  mixer[cros_tokens::kCrosRefNeutralvariant50] = {
      ui::kColorRefNeutralVariant50};
  mixer[cros_tokens::kCrosRefNeutralvariant60] = {
      ui::kColorRefNeutralVariant60};
  mixer[cros_tokens::kCrosRefNeutralvariant70] = {
      ui::kColorRefNeutralVariant70};
  mixer[cros_tokens::kCrosRefNeutralvariant80] = {
      ui::kColorRefNeutralVariant80};
  mixer[cros_tokens::kCrosRefNeutralvariant90] = {
      ui::kColorRefNeutralVariant90};
  mixer[cros_tokens::kCrosRefNeutralvariant95] = {
      ui::kColorRefNeutralVariant95};
  mixer[cros_tokens::kCrosRefNeutralvariant99] = {
      ui::kColorRefNeutralVariant99};
  mixer[cros_tokens::kCrosRefNeutralvariant100] = {
      ui::kColorRefNeutralVariant100};
}

// Overrides some cros.sys colors in `mixer` with values that are appropriate
// for pre-Jelly features.
void ReverseMapSysColors(ui::ColorMixer& mixer, bool dark_mode) {
  mixer[cros_tokens::kCrosSysPrimary] = {dark_mode ? gfx::kGoogleBlue200
                                                   : gfx::kGoogleBlue600};
  mixer[cros_tokens::kCrosSysOnPrimary] = {dark_mode ? gfx::kGoogleGrey900
                                                     : gfx::kGoogleGrey200};
  mixer[cros_tokens::kCrosSysSecondary] = {cros_tokens::kColorSecondary};
  mixer[cros_tokens::kCrosSysOnSecondary] = {dark_mode ? gfx::kGoogleGrey800
                                                       : gfx::kGoogleGrey600};

  if (dark_mode) {
    // LightInkRipple in dark mode
    mixer[cros_tokens::kCrosSysDisabledContainer] = ui::SetAlpha(
        SK_ColorBLACK, StyleUtil::kLightInkDropOpacity * SK_AlphaOPAQUE);
  } else {
    // DarkInkRipple in dark mode
    mixer[cros_tokens::kCrosSysDisabledContainer] = ui::SetAlpha(
        SK_ColorWHITE, StyleUtil::kDarkInkDropOpacity * SK_AlphaOPAQUE);
  }

  mixer[cros_tokens::kCrosSysHoverOnSubtle] = {SK_ColorTRANSPARENT};
  mixer[cros_tokens::kCrosSysSystemBaseElevated] = {kColorAshShieldAndBase80};
  mixer[cros_tokens::kCrosSysSystemOnBase] = {
      kColorAshControlBackgroundColorInactive};
  mixer[cros_tokens::kCrosSysSystemOnNegativeContainer] = {
      kColorAshTextColorPrimary};
  mixer[cros_tokens::kCrosSysSystemOnPrimaryContainer] = {
      dark_mode ? ui::ColorTransform(gfx::kGoogleGrey900)
                : ui::ColorTransform(kColorAshTextColorPrimary)};

  mixer[cros_tokens::kCrosSysSystemNegativeContainer] = {gfx::kGoogleRed300};
  mixer[cros_tokens::kCrosSysPositive] = {cros_tokens::kColorPositive};

  mixer[cros_tokens::kCrosSysSystemPrimaryContainer] = {
      dark_mode ? gfx::kGoogleBlue200 : gfx::kGoogleBlue300};

  // Colors for feature tile that differ from sys token mappings.
  mixer[kColorAshTileSmallCircle] =
      dark_mode ? ui::ColorTransform(cros_tokens::kCrosSysHighlightShape)
                : ui::SetAlpha(gfx::kGoogleBlue600, 31);  // 12% opacity
}

}  // namespace

void AddCrosStylesColorMixer(ui::ColorProvider* provider,
                             const ui::ColorProviderManager::Key& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  bool dark_mode = key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  if (chromeos::features::IsJellyEnabled()) {
    AddRefPalette(mixer, key);
  } else {
    cros_tokens::AddCrosRefColorsToMixer(mixer, dark_mode);
  }
  // Add after ref colors since it needs to override them.
  AddHarmonizedColors(mixer, key);
  cros_tokens::AddCrosSysColorsToMixer(mixer, dark_mode);
  if (!chromeos::features::IsJellyEnabled()) {
    // Overrides some cros.sys colors with pre-Jelly values so they can used in
    // UI with the Jelly flag off.
    ReverseMapSysColors(mixer, dark_mode);
  }

  // TODO(b/234400002): Remove legacy colors once all usages are cleaned up.
  cros_tokens::AddLegacySemanticColorsToMixer(mixer, dark_mode);

  if (chromeos::features::IsJellyEnabled()) {
    RemapLegacySemanticColors(mixer);
  }
}

void AddAshColorMixer(ui::ColorProvider* provider,
                      const ui::ColorProviderManager::Key& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  const bool use_dark_color =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;

  AddShieldAndBaseColors(mixer, key);
  AddControlsColors(mixer, key);
  AddContentColors(mixer, key);

  mixer[kColorAshAssistantBgPlate] = {use_dark_color
                                          ? SkColorSetRGB(0x1c, 0x2b, 0x3b)
                                          : SkColorSetRGB(0xec, 0xef, 0xee)};
  mixer[kColorAshAssistantGreetingEnabled] = {cros_tokens::kColorPrimary};
  mixer[kColorAshSuggestionChipViewTextView] = {cros_tokens::kColorSecondary};
  mixer[kColorAshAssistantQueryHighConfidenceLabel] = {
      cros_tokens::kColorPrimary};
  mixer[kColorAshAssistantQueryLowConfidenceLabel] = {
      cros_tokens::kColorSecondary};
  mixer[kColorAshAssistantTextColorPrimary] = {cros_tokens::kColorPrimary};

  mixer[ui::kColorAshActionLabelFocusRingEdit] = {
      cros_tokens::kColorProminentDark};
  mixer[ui::kColorAshActionLabelFocusRingError] = {
      cros_tokens::kColorAlertDark};
  mixer[ui::kColorAshActionLabelFocusRingHover] =
      ui::SetAlpha(cros_tokens::kColorPrimaryDark, 0x60);

  mixer[ui::kColorAshPrivacyIndicatorsBackground] = {
      cros_tokens::kCrosSysPrivacyIndicator};

  mixer[ui::kColorAshAppListFocusRingNoKeyboard] = {SK_AlphaTRANSPARENT};
  mixer[ui::kColorAshAppListSeparatorLight] = {
      ui::kColorAshSystemUIMenuSeparator};
  mixer[ui::kColorAshAppListSeparator] =
      ui::SetAlpha(cros_tokens::kColorPrimaryLight, 0x24);
  mixer[ui::kColorAshArcInputMenuSeparator] = {SK_ColorGRAY};
  mixer[ui::kColorAshInputOverlayFocusRing] = {
      cros_tokens::kColorProminentDark};
  mixer[ui::kColorAshIconInOobe] = {kIconColorInOobe};

  // TODO(skau): Remove when dark/light mode launches.
  mixer[ui::kColorAshAppListFocusRingCompat] = {
      cros_tokens::kColorProminentLight};

  mixer[ui::kColorAshLightFocusRing] = {cros_tokens::kColorProminentDark};

  mixer[ui::kColorAshOnboardingFocusRing] = {cros_tokens::kColorProminentDark};

  mixer[ui::kColorAshSystemUIMenuBackground] = {
      chromeos::features::IsJellyEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysBaseElevated)
          : kColorAshShieldAndBase80};
  mixer[ui::kColorAshSystemUIMenuIcon] = {kColorAshIconColorPrimary};
  mixer[ui::kColorAshSystemUIMenuItemBackgroundSelected] = {kColorAshInkDrop};
  mixer[ui::kColorAshSystemUIMenuSeparator] = {kColorAshSeparatorColor};

  mixer[kColorAshDialogBackgroundColor] =
      use_dark_color ? ui::ColorTransform(SkColorSetRGB(0x32, 0x33, 0x36))
                     : ui::ColorTransform(SK_ColorWHITE);

  mixer[kColorAshButtonIconDisabledColor] =
      ui::SetAlpha(kColorAshButtonIconColor, kDisabledColorOpacity);
  mixer[kColorAshIconSecondaryDisabledColor] =
      ui::SetAlpha(cros_tokens::kCrosSysSecondary, kDisabledColorOpacity);
  mixer[kColorAshIconPrimaryDisabledColor] =
      ui::SetAlpha(cros_tokens::kCrosSysPrimary, kDisabledColorOpacity);
  mixer[KColorAshTextDisabledColor] =
      ui::SetAlpha(cros_tokens::kCrosSysOnSurface, kDisabledColorOpacity);

  mixer[kColorAshIconColorBlocked] = {gfx::kGoogleGrey100};

  mixer[kColorAshEcheIconColorStreaming] = {ui::ColorTransform(SK_ColorGREEN)};

  mixer[kColorAshMultiSelectTextColor] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleBlue100)
                     : ui::ColorTransform(gfx::kGoogleBlue800);

  mixer[kColorAshCheckmarkIconColor] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleGrey900)
                     : ui::ColorTransform(SK_ColorWHITE);

  mixer[kColorAshDragImageOverflowBadgeTextColor] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleGrey900)
                     : ui::ColorTransform(gfx::kGoogleGrey200);

  mixer[kColorAshFolderItemCountBackgroundColor] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleBlue300)
                     : ui::ColorTransform(gfx::kGoogleBlue600);
  mixer[kColorAshPhantomWindowBackgroundColor] =
      ui::SetAlpha(cros_tokens::kCrosSysPrimary, kAlpha25);

  mixer[ui::kColorToggleButtonThumbOn] = {cros_tokens::kCrosSysOnPrimary};
  mixer[ui::kColorToggleButtonThumbOff] = {cros_tokens::kCrosSysOnSecondary};
  mixer[ui::kColorToggleButtonThumbOnDisabled] =
      ui::SetAlpha({ui::kColorToggleButtonThumbOn}, kDisabledColorOpacity);
  mixer[ui::kColorToggleButtonThumbOffDisabled] =
      ui::SetAlpha({ui::kColorToggleButtonThumbOff}, kDisabledColorOpacity);
  mixer[ui::kColorToggleButtonTrackOn] = {cros_tokens::kCrosSysPrimary};
  mixer[ui::kColorToggleButtonTrackOff] = {cros_tokens::kCrosSysSecondary};
  mixer[ui::kColorToggleButtonTrackOnDisabled] =
      ui::SetAlpha({ui::kColorToggleButtonTrackOn}, kDisabledColorOpacity);
  mixer[ui::kColorToggleButtonTrackOffDisabled] =
      ui::SetAlpha({ui::kColorToggleButtonTrackOff}, kDisabledColorOpacity);
  mixer[ui::kColorToggleButtonHover] = {cros_tokens::kCrosSysHoverOnProminent};

  mixer[ui::kColorTooltipBackground] = {cros_tokens::kCrosSysOnSurface};
  mixer[ui::kColorTooltipForeground] = {cros_tokens::kCrosSysInverseOnSurface};
}

}  // namespace ash
