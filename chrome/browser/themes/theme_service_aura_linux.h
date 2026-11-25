// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_AURA_LINUX_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_AURA_LINUX_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/themes/theme_service.h"

class Profile;

namespace ui {
class LinuxUiTheme;
}

// A subclass of ThemeService that manages the CustomThemeSupplier which
// provides the native Linux theme.
class ThemeServiceAuraLinux : public ThemeService {
 public:
  ThemeServiceAuraLinux(Profile* profile, const ThemeHelper& theme_helper);

  ThemeServiceAuraLinux(const ThemeServiceAuraLinux&) = delete;
  ThemeServiceAuraLinux& operator=(const ThemeServiceAuraLinux&) = delete;

  ~ThemeServiceAuraLinux() override;

  // Overridden from ThemeService:
  void Init() override;
  void Shutdown() override;
  ui::SystemTheme GetDefaultSystemTheme() const override;
  void UseTheme(ui::SystemTheme system_theme) override;
  void UseSystemTheme() override;
  bool IsSystemThemeDistinctFromDefaultTheme() const override;
  bool UsingSystemTheme() const override;
  void FixInconsistentPreferencesIfNeeded() override;
  BrowserColorScheme GetBrowserColorScheme() const override;
  bool BrowserUsesDarkColors() const override;

  static ui::SystemTheme GetSystemThemeForProfile(const Profile* profile);

  // Returns the LinuxUiTheme instance for the given profile, or the default
  // instance if `profile` is nullptr.
  static ui::LinuxUiTheme* GetLinuxUiThemeForProfile(const Profile* profile);

 private:
  void UpdateCustomFramePrefDefault();

  base::CallbackListSubscription custom_frame_pref_default_subscription_;

  base::WeakPtrFactory<ThemeServiceAuraLinux> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_AURA_LINUX_H_
