// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_HELPER_WIN_H_
#define CHROME_BROWSER_THEMES_THEME_HELPER_WIN_H_

#include "base/callback_list.h"
#include "chrome/browser/themes/theme_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Tracks updates to the native colors on Windows 10 and calcuates the values we
// should use (which are not always what Windows uses). None of the values here
// are relevant to earlier versions of Windows.
class ThemeHelperWin : public ThemeHelper {
 public:
  ThemeHelperWin();
  ~ThemeHelperWin() override;

  ThemeHelperWin(const ThemeHelperWin&) = delete;
  ThemeHelperWin& operator=(const ThemeHelperWin&) = delete;

 private:
  // ThemeService:
  bool ShouldUseNativeFrame(
      const CustomThemeSupplier* theme_supplier) const override;
  bool ShouldUseIncreasedContrastThemeSupplier(
      ui::NativeTheme* native_theme) const override;
  SkColor GetDefaultColor(
      int id,
      bool incognito,
      const CustomThemeSupplier* theme_supplier) const override;

  // Returns true if colors from DWM can be used, i.e. this is a native frame
  // on Windows 8+.
  bool DwmColorsAllowed(const CustomThemeSupplier* theme_supplier) const;

  // Callback executed when the accent color is updated. This re-reads the
  // accent color and updates |dwm_frame_color_| and
  // |dwm_inactive_frame_color_|.
  void OnAccentColorUpdated();

  // Re-reads the accent colors and updates member variables.
  void FetchAccentColors();

  base::CallbackListSubscription subscription_;

  // The frame color when active. If empty the default colors should be used.
  absl::optional<SkColor> dwm_frame_color_;

  // The frame color when inactive. If empty the default colors should be used.
  absl::optional<SkColor> dwm_inactive_frame_color_;

  // The DWM accent border color, if available; white otherwise.
  SkColor dwm_accent_border_color_;
};

#endif  // CHROME_BROWSER_THEMES_THEME_HELPER_WIN_H_
