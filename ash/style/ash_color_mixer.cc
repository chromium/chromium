// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_mixer.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/style_util.h"
#include "ash/style/temp_palette.h"
#include "ash/system/tray/tray_constants.h"
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
  if (ash::features::IsJellyEnabled()) {
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
      features::IsDarkLightModeEnabled()
          ? key.color_mode == ui::ColorProviderManager::ColorMode::kDark
          : DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();

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
}

// Mappings of Controls Colors for Material 2.
void AddControlsColors(ui::ColorMixer& mixer,
                       const ui::ColorProviderManager::Key& key) {
  const bool use_dark_color =
      features::IsDarkLightModeEnabled()
          ? key.color_mode == ui::ColorProviderManager::ColorMode::kDark
          : DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();

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
      features::IsDarkLightModeEnabled()
          ? key.color_mode == ui::ColorProviderManager::ColorMode::kDark
          : DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();

  // ContentLayer colors.
  mixer[kColorAshScrollBarColor] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleGrey200)
                     : ui::ColorTransform(gfx::kGoogleGrey700);
  mixer[kColorAshSeparatorColor] =
      use_dark_color ? ui::ColorTransform(SkColorSetA(SK_ColorWHITE, 0x24))
                     : ui::ColorTransform(SkColorSetA(SK_ColorBLACK, 0x24));
  mixer[kColorAshTextColorPrimary] = {cros_tokens::kColorPrimary};
  mixer[kColorAshTextColorSecondary] = {cros_tokens::kColorSecondary};
  mixer[kColorAshTextColorAlert] = {cros_tokens::kColorAlert};
  mixer[kColorAshTextColorWarning] = {cros_tokens::kColorWarning};
  mixer[kColorAshTextColorPositive] = {cros_tokens::kColorPositive};
  mixer[kColorAshTextColorURL] = {cros_tokens::kColorProminent};
  mixer[kColorAshIconColorPrimary] = {kColorAshTextColorPrimary};
  // TODO(skau): Figure out if this should be kColorSecondary instead.
  mixer[kColorAshIconColorSecondary] = {cros_tokens::kColorDisabledDark};
  mixer[kColorAshIconColorAlert] = {kColorAshTextColorAlert};
  mixer[kColorAshIconColorWarning] = {kColorAshTextColorWarning};
  mixer[kColorAshIconColorPositive] = {kColorAshTextColorPositive};
  mixer[kColorAshIconColorProminent] = {kColorAshTextColorURL};
  mixer[kColorAshIconColorSecondaryBackground] =
      use_dark_color ? ui::ColorTransform(gfx::kGoogleGrey100)
                     : ui::ColorTransform(gfx::kGoogleGrey800);
  mixer[kColorAshButtonLabelColor] = {kColorAshTextColorPrimary};
  mixer[kColorAshButtonLabelColorPrimary] = {
      cros_tokens::kColorPrimaryInverted};
  mixer[kColorAshInvertedTextColorPrimary] = {kColorAshButtonLabelColorPrimary};
  mixer[kColorAshInvertedButtonLabelColor] = {kColorAshButtonLabelColorPrimary};
  mixer[kColorAshTextColorSuggestion] = {cros_tokens::kColorDisabled};
  mixer[kColorAshButtonLabelColorBlue] = {kColorAshTextColorURL};
  mixer[kColorAshButtonIconColor] = {kColorAshTextColorPrimary};
  mixer[kColorAshButtonIconColorPrimary] = {kColorAshButtonLabelColorPrimary};
  mixer[kColorAshAppStateIndicatorColor] = {kColorAshTextColorPrimary};
  mixer[kColorAshAppStateIndicatorColorInactive] =
      ui::SetAlpha(kColorAshAppStateIndicatorColor, kDisabledColorOpacity);
  mixer[kColorAshShelfHandleColor] = {kColorAshSeparatorColor};
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

  mixer[cros_tokens::kBgColor] = {cros_tokens::kCrosSysAppBase2};
  mixer[cros_tokens::kBgColorElevation1] = {
      cros_tokens::kCrosSysAppBaseElevated};
  mixer[cros_tokens::kBgColorElevation2Light] = {
      cros_tokens::kCrosSysAppBaseElevatedLight};
  mixer[cros_tokens::kBgColorElevation2Dark] = {
      cros_tokens::kCrosSysAppBaseElevatedDark};
  mixer[cros_tokens::kBgColorElevation3] = {
      cros_tokens::kCrosSysAppBaseElevated};
  mixer[cros_tokens::kBgColorElevation4] = {
      cros_tokens::kCrosSysAppBaseElevated};
  mixer[cros_tokens::kBgColorElevation5] = {
      cros_tokens::kCrosSysAppBaseElevated};
  mixer[cros_tokens::kBgColorDroppedElevation1] = {
      cros_tokens::kCrosSysAppBase1};
  mixer[cros_tokens::kBgColorDroppedElevation2] = {
      cros_tokens::kCrosSysAppBase1};
}

// Adds the dynamic color palette tokens based on user_color. This is the base
// palette so it is independent of ColorMode.
void AddRefPalette(ui::ColorMixer& mixer,
                   const ui::ColorProviderManager::Key& key) {
  // TODO(skau): Before this launches, make sure this is always populated.
  SkColor seed_color = key.user_color.value_or(gfx::kGoogleBlue400);

  // TODO(skau): Replace with official implementation when available.
  ToneMap tone_map = GetTempPalette(seed_color);

  mixer[cros_tokens::kCrosRefPrimary0] = {tone_map.primary[Luma::k0]};
  mixer[cros_tokens::kCrosRefPrimary10] = {tone_map.primary[Luma::k10]};
  mixer[cros_tokens::kCrosRefPrimary20] = {tone_map.primary[Luma::k20]};
  mixer[cros_tokens::kCrosRefPrimary30] = {tone_map.primary[Luma::k30]};
  mixer[cros_tokens::kCrosRefPrimary40] = {tone_map.primary[Luma::k40]};
  mixer[cros_tokens::kCrosRefPrimary50] = {tone_map.primary[Luma::k50]};
  mixer[cros_tokens::kCrosRefPrimary60] = {tone_map.primary[Luma::k60]};
  mixer[cros_tokens::kCrosRefPrimary70] = {tone_map.primary[Luma::k70]};
  mixer[cros_tokens::kCrosRefPrimary80] = {tone_map.primary[Luma::k80]};
  mixer[cros_tokens::kCrosRefPrimary90] = {tone_map.primary[Luma::k90]};
  mixer[cros_tokens::kCrosRefPrimary95] = {tone_map.primary[Luma::k95]};
  mixer[cros_tokens::kCrosRefPrimary99] = {tone_map.primary[Luma::k99]};
  mixer[cros_tokens::kCrosRefPrimary100] = {tone_map.primary[Luma::k100]};

  mixer[cros_tokens::kCrosRefSecondary0] = {tone_map.secondary[Luma::k0]};
  mixer[cros_tokens::kCrosRefSecondary10] = {tone_map.secondary[Luma::k10]};
  mixer[cros_tokens::kCrosRefSecondary20] = {tone_map.secondary[Luma::k20]};
  mixer[cros_tokens::kCrosRefSecondary30] = {tone_map.secondary[Luma::k30]};
  mixer[cros_tokens::kCrosRefSecondary40] = {tone_map.secondary[Luma::k40]};
  mixer[cros_tokens::kCrosRefSecondary50] = {tone_map.secondary[Luma::k50]};
  mixer[cros_tokens::kCrosRefSecondary60] = {tone_map.secondary[Luma::k60]};
  mixer[cros_tokens::kCrosRefSecondary70] = {tone_map.secondary[Luma::k70]};
  mixer[cros_tokens::kCrosRefSecondary80] = {tone_map.secondary[Luma::k80]};
  mixer[cros_tokens::kCrosRefSecondary90] = {tone_map.secondary[Luma::k90]};
  mixer[cros_tokens::kCrosRefSecondary95] = {tone_map.secondary[Luma::k95]};
  mixer[cros_tokens::kCrosRefSecondary99] = {tone_map.secondary[Luma::k99]};
  mixer[cros_tokens::kCrosRefSecondary100] = {tone_map.secondary[Luma::k100]};

  mixer[cros_tokens::kCrosRefTertiary0] = {tone_map.tertiary[Luma::k0]};
  mixer[cros_tokens::kCrosRefTertiary10] = {tone_map.tertiary[Luma::k10]};
  mixer[cros_tokens::kCrosRefTertiary20] = {tone_map.tertiary[Luma::k20]};
  mixer[cros_tokens::kCrosRefTertiary30] = {tone_map.tertiary[Luma::k30]};
  mixer[cros_tokens::kCrosRefTertiary40] = {tone_map.tertiary[Luma::k40]};
  mixer[cros_tokens::kCrosRefTertiary50] = {tone_map.tertiary[Luma::k50]};
  mixer[cros_tokens::kCrosRefTertiary60] = {tone_map.tertiary[Luma::k60]};
  mixer[cros_tokens::kCrosRefTertiary70] = {tone_map.tertiary[Luma::k70]};
  mixer[cros_tokens::kCrosRefTertiary80] = {tone_map.tertiary[Luma::k80]};
  mixer[cros_tokens::kCrosRefTertiary90] = {tone_map.tertiary[Luma::k90]};
  mixer[cros_tokens::kCrosRefTertiary95] = {tone_map.tertiary[Luma::k95]};
  mixer[cros_tokens::kCrosRefTertiary99] = {tone_map.tertiary[Luma::k99]};
  mixer[cros_tokens::kCrosRefTertiary100] = {tone_map.tertiary[Luma::k100]};

  mixer[cros_tokens::kCrosRefError0] = {tone_map.error[Luma::k0]};
  mixer[cros_tokens::kCrosRefError10] = {tone_map.error[Luma::k10]};
  mixer[cros_tokens::kCrosRefError20] = {tone_map.error[Luma::k20]};
  mixer[cros_tokens::kCrosRefError30] = {tone_map.error[Luma::k30]};
  mixer[cros_tokens::kCrosRefError40] = {tone_map.error[Luma::k40]};
  mixer[cros_tokens::kCrosRefError50] = {tone_map.error[Luma::k50]};
  mixer[cros_tokens::kCrosRefError60] = {tone_map.error[Luma::k60]};
  mixer[cros_tokens::kCrosRefError70] = {tone_map.error[Luma::k70]};
  mixer[cros_tokens::kCrosRefError80] = {tone_map.error[Luma::k80]};
  mixer[cros_tokens::kCrosRefError90] = {tone_map.error[Luma::k90]};
  mixer[cros_tokens::kCrosRefError95] = {tone_map.error[Luma::k95]};
  mixer[cros_tokens::kCrosRefError99] = {tone_map.error[Luma::k99]};
  mixer[cros_tokens::kCrosRefError100] = {tone_map.error[Luma::k100]};

  mixer[cros_tokens::kCrosRefNeutral0] = {tone_map.neutral1[Luma::k0]};
  mixer[cros_tokens::kCrosRefNeutral10] = {tone_map.neutral1[Luma::k10]};
  mixer[cros_tokens::kCrosRefNeutral20] = {tone_map.neutral1[Luma::k20]};
  mixer[cros_tokens::kCrosRefNeutral30] = {tone_map.neutral1[Luma::k30]};
  mixer[cros_tokens::kCrosRefNeutral40] = {tone_map.neutral1[Luma::k40]};
  mixer[cros_tokens::kCrosRefNeutral50] = {tone_map.neutral1[Luma::k50]};
  mixer[cros_tokens::kCrosRefNeutral60] = {tone_map.neutral1[Luma::k60]};
  mixer[cros_tokens::kCrosRefNeutral70] = {tone_map.neutral1[Luma::k70]};
  mixer[cros_tokens::kCrosRefNeutral80] = {tone_map.neutral1[Luma::k80]};
  mixer[cros_tokens::kCrosRefNeutral90] = {tone_map.neutral1[Luma::k90]};
  mixer[cros_tokens::kCrosRefNeutral95] = {tone_map.neutral1[Luma::k95]};
  mixer[cros_tokens::kCrosRefNeutral99] = {tone_map.neutral1[Luma::k99]};
  mixer[cros_tokens::kCrosRefNeutral100] = {tone_map.neutral1[Luma::k100]};

  mixer[cros_tokens::kCrosRefNeutralvariant0] = {tone_map.neutral2[Luma::k0]};
  mixer[cros_tokens::kCrosRefNeutralvariant10] = {tone_map.neutral2[Luma::k10]};
  mixer[cros_tokens::kCrosRefNeutralvariant20] = {tone_map.neutral2[Luma::k20]};
  mixer[cros_tokens::kCrosRefNeutralvariant30] = {tone_map.neutral2[Luma::k30]};
  mixer[cros_tokens::kCrosRefNeutralvariant40] = {tone_map.neutral2[Luma::k40]};
  mixer[cros_tokens::kCrosRefNeutralvariant50] = {tone_map.neutral2[Luma::k50]};
  mixer[cros_tokens::kCrosRefNeutralvariant60] = {tone_map.neutral2[Luma::k60]};
  mixer[cros_tokens::kCrosRefNeutralvariant70] = {tone_map.neutral2[Luma::k70]};
  mixer[cros_tokens::kCrosRefNeutralvariant80] = {tone_map.neutral2[Luma::k80]};
  mixer[cros_tokens::kCrosRefNeutralvariant90] = {tone_map.neutral2[Luma::k90]};
  mixer[cros_tokens::kCrosRefNeutralvariant95] = {tone_map.neutral2[Luma::k95]};
  mixer[cros_tokens::kCrosRefNeutralvariant99] = {tone_map.neutral2[Luma::k99]};
  mixer[cros_tokens::kCrosRefNeutralvariant100] = {
      tone_map.neutral2[Luma::k100]};
}

}  // namespace

void AddCrosStylesColorMixer(ui::ColorProvider* provider,
                             const ui::ColorProviderManager::Key& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  bool dark_mode =
      features::IsDarkLightModeEnabled()
          ? key.color_mode == ui::ColorProviderManager::ColorMode::kDark
          : DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();
  if (ash::features::IsJellyEnabled()) {
    AddRefPalette(mixer, key);
  } else {
    cros_tokens::AddCrosRefColorsToMixer(mixer, dark_mode);
  }
  cros_tokens::AddCrosSysColorsToMixer(mixer, dark_mode);

  // TODO(b/234400002): Remove legacy colors once all usages are cleaned up.
  cros_tokens::AddLegacySemanticColorsToMixer(mixer, dark_mode);

  if (ash::features::IsJellyEnabled())
    RemapLegacySemanticColors(mixer);
}

void AddAshColorMixer(ui::ColorProvider* provider,
                      const ui::ColorProviderManager::Key& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  AddShieldAndBaseColors(mixer, key);
  AddControlsColors(mixer, key);
  AddContentColors(mixer, key);

  if (!features::IsProductivityLauncherEnabled() &&
      !features::IsDarkLightModeEnabled()) {
    mixer[kColorAshAssistantGreetingEnabled] = {
        cros_tokens::kColorPrimaryLight};
  } else {
    mixer[kColorAshAssistantGreetingEnabled] = {cros_tokens::kColorPrimary};
  }

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
  mixer[ui::kColorAshEditFinishFocusRing] = {cros_tokens::kColorProminentDark};
  mixer[ui::kColorAshIconInOobe] = {kIconColorInOobe};

  // TODO(skau): Remove when dark/light mode launches.
  mixer[ui::kColorAshAppListFocusRingCompat] = {
      cros_tokens::kColorProminentLight};

  mixer[ui::kColorAshLightFocusRing] = {cros_tokens::kColorProminentDark};

  mixer[ui::kColorAshOnboardingFocusRing] = {cros_tokens::kColorProminentDark};

  if (!features::IsDarkLightModeEnabled()) {
    ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
    mixer[ui::kColorAshSystemUILightBorderColor1] = {
        ui::kColorHighlightBorderBorder1};
    mixer[ui::kColorAshSystemUILightBorderColor2] = {
        ui::kColorHighlightBorderBorder2};
    mixer[ui::kColorAshSystemUILightHighlightColor1] = {
        ui::kColorHighlightBorderHighlight1};
    mixer[ui::kColorAshSystemUILightHighlightColor2] = {
        ui::kColorHighlightBorderHighlight2};
    return;
  }

  mixer[ui::kColorAshSystemUIMenuBackground] = {kColorAshShieldAndBase80};
  mixer[ui::kColorAshSystemUIMenuIcon] = {kColorAshIconColorPrimary};
  mixer[ui::kColorAshSystemUIMenuItemBackgroundSelected] = {kColorAshInkDrop};
  mixer[ui::kColorAshSystemUIMenuSeparator] = {kColorAshSeparatorColor};
}

}  // namespace ash
