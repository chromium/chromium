// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_helper_win.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/win/windows_version.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/win/titlebar_config.h"
#include "chrome/grit/theme_resources.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/views_features.h"

namespace {

SkColor GetDefaultInactiveFrameColor() {
  return base::win::GetVersion() < base::win::Version::WIN10
             ? SkColorSetRGB(0xEB, 0xEB, 0xEB)
             : SK_ColorWHITE;
}

}  // namespace

ThemeHelperWin::ThemeHelperWin() {
  // This just checks for Windows 8+ instead of calling DwmColorsAllowed()
  // because we want to monitor the frame color even when a custom frame is in
  // use, so that it will be correct if at any time the user switches to the
  // native frame.
  if (base::win::GetVersion() >= base::win::Version::WIN8) {
    dwm_key_.reset(new base::win::RegKey(
        HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", KEY_READ));
    if (dwm_key_->Valid())
      OnDwmKeyUpdated();
    else
      dwm_key_.reset();
  }
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

  if (DwmColorsAllowed(theme_supplier)) {
    // In Windows 10, native inactive borders are #555555 with 50% alpha.
    // Prior to version 1809, native active borders use the accent color.
    // In version 1809 and following, the active border is #262626 with 66%
    // alpha unless the accent color is also used for the frame.
    if (id == ThemeProperties::COLOR_ACCENT_BORDER_ACTIVE) {
      return (base::win::GetVersion() >= base::win::Version::WIN10_RS5 &&
              !dwm_frame_color_)
                 ? SkColorSetARGB(0xa8, 0x26, 0x26, 0x26)
                 : dwm_accent_border_color_;
    }
    if (id == ThemeProperties::COLOR_ACCENT_BORDER_INACTIVE)
      return SkColorSetARGB(0x80, 0x55, 0x55, 0x55);

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
      if (!ShouldCustomDrawSystemTitlebar()) {
        return inactive_frame_color_from_registry_
                   ? dwm_inactive_frame_color_.value()
                   : GetDefaultInactiveFrameColor();
      }
      if (dwm_frame_color_ && !inactive_frame_color_from_registry_) {
        // Tint to create inactive color. Always use the non-incognito version
        // of the tint, since the frame should look the same in both modes.
        return color_utils::HSLShift(
            dwm_frame_color_.value(),
            GetTint(ThemeProperties::TINT_FRAME_INACTIVE, false,
                    theme_supplier));
      }
      if (dwm_inactive_frame_color_)
        return dwm_inactive_frame_color_.value();
      // Fall through and use default.
    }
  }

  return ThemeHelper::GetDefaultColor(id, incognito, theme_supplier);
}

bool ThemeHelperWin::GetPlatformHighContrastColor(int id,
                                                  SkColor* color) const {
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
    case ThemeProperties::COLOR_STATUS_BUBBLE:
      system_theme_color = ui::NativeTheme::SystemThemeColor::kWindow;
      break;

    // Window Text
    case ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR:
    case ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR:
    case ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE:
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
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON:
    case ThemeProperties::COLOR_BOOKMARK_TEXT:
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE:
    case ThemeProperties::
        COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO:
    case ThemeProperties::COLOR_OMNIBOX_TEXT:
    case ThemeProperties::COLOR_OMNIBOX_SELECTED_KEYWORD:
    case ThemeProperties::COLOR_OMNIBOX_BUBBLE_OUTLINE:
    case ThemeProperties::COLOR_OMNIBOX_TEXT_DIMMED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_URL:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED:
    case ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT:
    case ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_SECURE:
    case ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS:
      system_theme_color = ui::NativeTheme::SystemThemeColor::kButtonText;
      break;

    // Highlight/Selected Background
    case ThemeProperties::COLOR_TOOLBAR_INK_DROP:
      if (!base::FeatureList::IsEnabled(
              views::features::kEnablePlatformHighContrastInkDrop))
        return false;
      FALLTHROUGH;
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_HOVERED:
      system_theme_color = ui::NativeTheme::SystemThemeColor::kHighlight;
      break;

    // Highlight/Selected Text Foreground
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_HOVERED:
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_PRESSED:
      if (!base::FeatureList::IsEnabled(
              views::features::kEnablePlatformHighContrastInkDrop)) {
        return GetPlatformHighContrastColor(
            ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON, color);
      }
      FALLTHROUGH;
    case ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_URL_SELECTED:
    case ThemeProperties::COLOR_OMNIBOX_RESULTS_FOCUS_BAR:
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

bool ThemeHelperWin::DwmColorsAllowed(
    const CustomThemeSupplier* theme_supplier) const {
  return ShouldUseNativeFrame(theme_supplier) &&
         (base::win::GetVersion() >= base::win::Version::WIN8);
}

void ThemeHelperWin::OnDwmKeyUpdated() {
  dwm_accent_border_color_ = GetDefaultInactiveFrameColor();
  DWORD colorization_color, colorization_color_balance;
  if ((dwm_key_->ReadValueDW(L"ColorizationColor", &colorization_color) ==
       ERROR_SUCCESS) &&
      (dwm_key_->ReadValueDW(L"ColorizationColorBalance",
                             &colorization_color_balance) == ERROR_SUCCESS)) {
    // The accent border color is a linear blend between the colorization
    // color and the neutral #d9d9d9. colorization_color_balance is the
    // percentage of the colorization color in that blend.
    //
    // On Windows version 1611 colorization_color_balance can be 0xfffffff3 if
    // the accent color is taken from the background and either the background
    // is a solid color or was just changed to a slideshow. It's unclear what
    // that value's supposed to mean, so change it to 80 to match Edge's
    // behavior.
    if (colorization_color_balance > 100)
      colorization_color_balance = 80;

    // colorization_color's high byte is not an alpha value, so replace it
    // with 0xff to make an opaque ARGB color.
    SkColor input_color = SkColorSetA(colorization_color, 0xff);

    dwm_accent_border_color_ =
        color_utils::AlphaBlend(input_color, SkColorSetRGB(0xd9, 0xd9, 0xd9),
                                colorization_color_balance / 100.0f);
  }

  inactive_frame_color_from_registry_ = false;
  if (base::win::GetVersion() < base::win::Version::WIN10) {
    dwm_frame_color_ = dwm_accent_border_color_;
  } else {
    DWORD accent_color, color_prevalence;
    bool use_dwm_frame_color =
        dwm_key_->ReadValueDW(L"AccentColor", &accent_color) == ERROR_SUCCESS &&
        dwm_key_->ReadValueDW(L"ColorPrevalence", &color_prevalence) ==
            ERROR_SUCCESS &&
        color_prevalence == 1;
    if (use_dwm_frame_color) {
      dwm_frame_color_ = skia::COLORREFToSkColor(accent_color);
      DWORD accent_color_inactive;
      if (dwm_key_->ReadValueDW(L"AccentColorInactive",
                                &accent_color_inactive) == ERROR_SUCCESS) {
        dwm_inactive_frame_color_ =
            skia::COLORREFToSkColor(accent_color_inactive);
        inactive_frame_color_from_registry_ = true;
      }
    } else {
      dwm_frame_color_.reset();
      dwm_inactive_frame_color_.reset();
    }
  }

  // Notify native theme observers that the native theme has changed.
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();

  // Watch for future changes.
  if (!dwm_key_->StartWatching(base::BindOnce(&ThemeHelperWin::OnDwmKeyUpdated,
                                              base::Unretained(this)))) {
    dwm_key_.reset();
  }
}
