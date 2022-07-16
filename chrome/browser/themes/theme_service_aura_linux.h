// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_AURA_LINUX_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_AURA_LINUX_H_

#include "chrome/browser/themes/theme_service.h"

class Profile;

// A subclass of ThemeService that manages the CustomThemeSupplier which
// provides the native Linux theme.
class ThemeServiceAuraLinux : public ThemeService {
 public:
  using ThemeService::ThemeService;

  ThemeServiceAuraLinux(const ThemeServiceAuraLinux&) = delete;
  ThemeServiceAuraLinux& operator=(const ThemeServiceAuraLinux&) = delete;

  ~ThemeServiceAuraLinux() override;

  // Overridden from ThemeService:
  bool ShouldInitWithSystemTheme() const override;
  void UseSystemTheme() override;
  bool IsSystemThemeDistinctFromDefaultTheme() const override;
  bool UsingSystemTheme() const override;
  void FixInconsistentPreferencesIfNeeded() override;

  static bool ShouldUseSystemThemeForProfile(const Profile* profile);
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_AURA_LINUX_H_
