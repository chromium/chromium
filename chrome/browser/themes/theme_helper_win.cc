// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_helper_win.h"

#include "base/win/windows_version.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/win/mica_titlebar.h"
#include "chrome/browser/win/titlebar_config.h"
#include "chrome/grit/theme_resources.h"

BASE_FEATURE(kForceXpTheme,
             "ForceXpTheme",
             base::FEATURE_DISABLED_BY_DEFAULT);

int ThemeHelperWin::GetDefaultDisplayProperty(int id) const {
  if (id == ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR) {
    return !ShouldDefaultThemeUseMicaTitlebar();
  }

  return ThemeHelper::GetDefaultDisplayProperty(id);
}

bool ThemeHelperWin::ShouldUseNativeFrame(
    const CustomThemeSupplier* theme_supplier) const {
		// If it returns false, the XP fallback theme is used.
		// And yes, Chromium has muddied the waters of what is considered "native".
		// Aero Glass is "native", Mica is "native", and so is the Windows 10-style theme that is drawn
		// by Chromium itself.
		// Only the "original" Chromium theme that mimicks Aero is not considered native.
  return !HasCustomImage(IDR_THEME_FRAME, theme_supplier) && ((base::win::GetVersion() >= base::win::Version::WIN8) 
         || !ShouldCustomDrawSystemTitlebar()) && !base::FeatureList::IsEnabled(kForceXpTheme);
}
