// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_helper_win.h"

#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/win/mica_titlebar.h"
#include "chrome/grit/theme_resources.h"

int ThemeHelperWin::GetDefaultDisplayProperty(int id) const {
  if (id == ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR) {
    return !ShouldDefaultThemeUseMicaTitlebar();
  }

  return ThemeHelper::GetDefaultDisplayProperty(id);
}

bool ThemeHelperWin::ShouldUseNativeFrame(
    const CustomThemeSupplier* theme_supplier) const {
  return true;
}
