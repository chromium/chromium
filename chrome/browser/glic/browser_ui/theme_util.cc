// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/theme_util.h"

#include "ui/native_theme/native_theme.h"

namespace glic {

bool UseDarkMode(ThemeService* theme_service) {
  // Taken from lens_overlay_theme_utils.cc
  ThemeService::BrowserColorScheme color_scheme =
      theme_service->GetBrowserColorScheme();
  return color_scheme == ThemeService::BrowserColorScheme::kSystem
             ? ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
             : color_scheme == ThemeService::BrowserColorScheme::kDark;
}

}  // namespace glic
