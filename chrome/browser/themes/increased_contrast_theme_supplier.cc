// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/increased_contrast_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "ui/native_theme/native_theme.h"

IncreasedContrastThemeSupplier::IncreasedContrastThemeSupplier(
    ui::NativeTheme* native_theme)
    : CustomThemeSupplier(INCREASED_CONTRAST),
      native_theme_(native_theme),
      is_dark_mode_(native_theme->ShouldUseDarkColors()) {
  native_theme->AddObserver(this);
}

IncreasedContrastThemeSupplier::~IncreasedContrastThemeSupplier() {
  native_theme_->RemoveObserver(this);
}

// TODO(ellyjones): Follow up with a11y designers about these color choices.
bool IncreasedContrastThemeSupplier::GetColor(int id, SkColor* color) const {
  const SkColor foreground = is_dark_mode_ ? SK_ColorWHITE : SK_ColorBLACK;
  const SkColor background = is_dark_mode_ ? SK_ColorBLACK : SK_ColorWHITE;
  switch (id) {
    case ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE:
      *color = foreground;
      return true;
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO:
      *color = SK_ColorWHITE;
      return true;
    case ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE:
    case ThemeProperties::
        COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO:
      *color = SK_ColorBLACK;
      return true;
    case ThemeProperties::COLOR_TOOLBAR:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE:
    case ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE:
      *color = background;
      return true;
    case ThemeProperties::COLOR_FRAME_INACTIVE:
    case ThemeProperties::COLOR_FRAME_INACTIVE_INCOGNITO:
      *color = SK_ColorGRAY;
      return true;
    case ThemeProperties::COLOR_FRAME_ACTIVE:
    case ThemeProperties::COLOR_FRAME_ACTIVE_INCOGNITO:
      *color = SK_ColorDKGRAY;
      return true;
    case ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR:
      *color = is_dark_mode_ ? SK_ColorDKGRAY : SK_ColorLTGRAY;
      return true;
    case ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR:
      *color = foreground;
      return true;
    case ThemeProperties::COLOR_LOCATION_BAR_BORDER:
      *color = foreground;
      return true;
  }
  return false;
}

bool IncreasedContrastThemeSupplier::CanUseIncognitoColors() const {
  return false;
}

void IncreasedContrastThemeSupplier::OnNativeThemeUpdated(
    ui::NativeTheme* native_theme) {
  is_dark_mode_ = native_theme->ShouldUseDarkColors();
}
