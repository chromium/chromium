// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_HELPER_WIN_H_
#define CHROME_BROWSER_THEMES_THEME_HELPER_WIN_H_

#include "chrome/browser/themes/theme_helper.h"

class ThemeHelperWin : public ThemeHelper {
 public:
  ThemeHelperWin() = default;
  ~ThemeHelperWin() override = default;

  // ThemeHelper:
  int GetDefaultDisplayProperty(int id) const override;

  bool ShouldUseNativeFrame(
      const CustomThemeSupplier* theme_supplier) const override;
};

#endif  // CHROME_BROWSER_THEMES_THEME_HELPER_WIN_H_
