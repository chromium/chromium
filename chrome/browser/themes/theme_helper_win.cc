// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_helper_win.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/win/windows_version.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/win/titlebar_config.h"
#include "chrome/grit/theme_resources.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/win/shell.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/views_features.h"

namespace {

bool GetPlatformHighContrastColor(int id, SkColor* color) {
  ui::NativeTheme::SystemThemeColor system_theme_color =
      ui::NativeTheme::SystemThemeColor::kNotSupported;

  switch (id) {
    // Window Background
    case ThemeProperties::COLOR_FRAME_ACTIVE:
    case ThemeProperties::COLOR_FRAME_ACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_FRAME_INACTIVE:
    case ThemeProperties::COLOR_FRAME_INACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE:
    case ThemeProperties::
        COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_DOWNLOAD_SHELF:
    case ThemeProperties::COLOR_INFOBAR:
    case ThemeProperties::COLOR_TOOLBAR:
      system_theme_color = ui::NativeTheme::SystemThemeColor::kWindow;
      break;

    // Window Text
    case ThemeProperties::COLOR_BOOKMARK_SEPARATOR:
    case ThemeProperties::COLOR_TAB_STROKE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_STROKE_FRAME_INACTIVE:
    case ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR:
    case ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE:
    case ThemeProperties::COLOR_LOCATION_BAR_BORDER:
      system_theme_color = ui::NativeTheme::SystemThemeColor::kWindowText;
      break;

    // Button Background
    case ThemeProperties::COLOR_OMNIBOX_BACKGROUND:
    case ThemeProperties::COLOR_OMNIBOX_BACKGROUND_HOVERED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_BG:
      system_theme_color = ui::NativeTheme::SystemThemeColor::kButtonFace;
      break;

    // Button Text Foreground
    case ThemeProperties::COLOR_BOOKMARK_BUTTON_ICON:
    case ThemeProperties::COLOR_DOWNLOAD_SHELF_CONTENT_AREA_SEPARATOR:
    case ThemeProperties::COLOR_INFOBAR_CONTENT_AREA_SEPARATOR:
    case ThemeProperties::COLOR_INFOBAR_TEXT:
    case ThemeProperties::COLOR_OMNIBOX_BUBBLE_OUTLINE:
    case ThemeProperties::
        COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_URL:
    case ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT:
    case ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_SECURE:
    case ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS:
    case ThemeProperties::COLOR_OMNIBOX_SELECTED_KEYWORD:
    case ThemeProperties::COLOR_OMNIBOX_TEXT:
    case ThemeProperties::COLOR_OMNIBOX_TEXT_DIMMED:
    case ThemeProperties::COLOR_SIDE_PANEL_CONTENT_AREA_SEPARATOR:
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE:
    case ThemeProperties::
        COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON:
    case ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR:
    case ThemeProperties::COLOR_TOOLBAR_TEXT:
      system_theme_color = ui::NativeTheme::SystemThemeColor::kButtonText;
      break;

    // Highlight/Selected Background
    case ThemeProperties::COLOR_TOOLBAR_INK_DROP:
      if (!base::FeatureList::IsEnabled(
              views::features::kEnablePlatformHighContrastInkDrop))
        return false;
      [[fallthrough]];
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_HOVERED:
      system_theme_color = ui::NativeTheme::SystemThemeColor::kHighlight;
      break;

    case ThemeProperties::COLOR_OMNIBOX_RESULTS_BUTTON_INK_DROP:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_BUTTON_INK_DROP_SELECTED:
      *color = color_utils::GetColorWithMaxContrast(
          ui::NativeTheme::GetInstanceForNativeUi()
              ->GetSystemThemeColor(
                  ui::NativeTheme::SystemThemeColor::kHighlight)
              .value());
      return true;

    // Highlight/Selected Text Foreground
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_HOVERED:
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_PRESSED:
      if (!base::FeatureList::IsEnabled(
              views::features::kEnablePlatformHighContrastInkDrop)) {
        return GetPlatformHighContrastColor(
            ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON, color);
      }
      [[fallthrough]];
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_URL_SELECTED:
    case ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE:
      system_theme_color = ui::NativeTheme::SystemThemeColor::kHighlightText;
      break;

    // Gray/Disabled Text
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE:
      system_theme_color = ui::NativeTheme::SystemThemeColor::kGrayText;
      break;

    default:
      return false;
  }

  *color = ui::NativeTheme::GetInstanceForNativeUi()
               ->GetSystemThemeColor(system_theme_color)
               .value();
  return true;
}

SkColor GetDefaultInactiveFrameColor() {
  return base::win::GetVersion() < base::win::Version::WIN10
             ? SkColorSetRGB(0xEB, 0xEB, 0xEB)
             : SK_ColorWHITE;
}

}  // namespace

ThemeHelperWin::ThemeHelperWin() {
  subscription_ = ui::AccentColorObserver::Get()->Subscribe(base::BindRepeating(
      &ThemeHelperWin::OnAccentColorUpdated, base::Unretained(this)));
  FetchAccentColors();
}

ThemeHelperWin::~ThemeHelperWin() = default;

bool ThemeHelperWin::ShouldUseNativeFrame(
    const CustomThemeSupplier* theme_supplier) const {
  const bool use_native_frame_if_enabled =
      ShouldCustomDrawSystemTitlebar() ||
      !HasCustomImage(IDR_THEME_FRAME, theme_supplier);
  return use_native_frame_if_enabled && ui::win::IsAeroGlassEnabled();
}

bool ThemeHelperWin::ShouldUseIncreasedContrastThemeSupplier(
    ui::NativeTheme* native_theme) const {
  // On Windows the platform provides the high contrast colors, so don't use the
  // IncreasedContrastThemeSupplier.
  return false;
}

SkColor ThemeHelperWin::GetDefaultColor(
    int id,
    bool incognito,
    const CustomThemeSupplier* theme_supplier) const {
  // In high contrast mode on Windows the platform provides the color. Try to
  // get that color first.
  SkColor color;
  if (ui::NativeTheme::GetInstanceForNativeUi()->InForcedColorsMode() &&
      GetPlatformHighContrastColor(id, &color)) {
    return color;
  }

  // In Windows 10, native inactive borders are #555555 with 50% alpha.
  // Prior to version 1809, native active borders use the accent color.
  // In version 1809 and following, the active border is #262626 with 66%
  // alpha unless the accent color is also used for the frame.
  // NOTE: These cases are always handled, even on Win7, in order to ensure the
  // the color provider redirection tests function. Win7 callers should never
  // actually pass in these IDs.
  if (id == ThemeProperties::COLOR_ACCENT_BORDER_ACTIVE) {
    return (base::win::GetVersion() >= base::win::Version::WIN10_RS5 &&
            !dwm_frame_color_)
               ? SkColorSetARGB(0xa8, 0x26, 0x26, 0x26)
               : dwm_accent_border_color_;
  }
  if (id == ThemeProperties::COLOR_ACCENT_BORDER_INACTIVE)
    return SkColorSetARGB(0x80, 0x55, 0x55, 0x55);

  if (DwmColorsAllowed(theme_supplier)) {
    // When we're custom-drawing the titlebar we want to use either the colors
    // we calculated in OnDwmKeyUpdated() or the default colors. When we're not
    // custom-drawing the titlebar we want to match the color Windows actually
    // uses because some things (like the incognito icon) use this color to
    // decide whether they should draw in light or dark mode. Incognito colors
    // should be the same as non-incognito in all cases here.
    if (id == ThemeProperties::COLOR_FRAME_ACTIVE) {
      if (dwm_frame_color_)
        return dwm_frame_color_.value();
      if (!ShouldCustomDrawSystemTitlebar())
        return SK_ColorWHITE;
      // Fall through and use default.
    }
    if (id == ThemeProperties::COLOR_FRAME_INACTIVE) {
      if (dwm_inactive_frame_color_)
        return dwm_inactive_frame_color_.value();
      if (!ShouldCustomDrawSystemTitlebar())
        return GetDefaultInactiveFrameColor();
      if (dwm_frame_color_) {
        // Tint to create inactive color. Always use the non-incognito version
        // of the tint, since the frame should look the same in both modes.
        return color_utils::HSLShift(
            dwm_frame_color_.value(),
            GetTint(ThemeProperties::TINT_FRAME_INACTIVE, false,
                    theme_supplier));
      }
      // Fall through and use default.
    }
  }

  return ThemeHelper::GetDefaultColor(id, incognito, theme_supplier);
}

bool ThemeHelperWin::DwmColorsAllowed(
    const CustomThemeSupplier* theme_supplier) const {
  return ShouldUseNativeFrame(theme_supplier) &&
         (base::win::GetVersion() >= base::win::Version::WIN8);
}

void ThemeHelperWin::OnAccentColorUpdated() {
  FetchAccentColors();
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
}

void ThemeHelperWin::FetchAccentColors() {
  const auto* accent_color_observer = ui::AccentColorObserver::Get();
  dwm_accent_border_color_ =
      accent_color_observer->accent_border_color().value_or(
          GetDefaultInactiveFrameColor());

  if (base::win::GetVersion() < base::win::Version::WIN10) {
    dwm_frame_color_ = dwm_accent_border_color_;
  } else {
    dwm_frame_color_ = accent_color_observer->accent_color();
    dwm_inactive_frame_color_ = accent_color_observer->accent_color_inactive();
  }
}
