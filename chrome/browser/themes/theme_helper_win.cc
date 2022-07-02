// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_helper_win.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/win/windows_version.h"
#include "chrome/browser/win/titlebar_config.h"
#include "chrome/grit/theme_resources.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/win/shell.h"
#include "ui/native_theme/native_theme.h"

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
