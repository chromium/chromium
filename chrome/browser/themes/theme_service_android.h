// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_ANDROID_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_ANDROID_H_

#include "chrome/browser/themes/theme_service.h"

class Profile;
class ThemeHelper;

// A subclass of ThemeService whose browser color scheme is backed by the
// Android app's light / dark theme setting rather than a profile pref.
class ThemeServiceAndroid final : public ThemeService {
 public:
  ThemeServiceAndroid(Profile* profile, const ThemeHelper& theme_helper);

  ThemeServiceAndroid(const ThemeServiceAndroid&) = delete;
  ThemeServiceAndroid& operator=(const ThemeServiceAndroid&) = delete;

  ~ThemeServiceAndroid() override;

  // Notifies the observers of all ThemeServiceAndroid instances. Called when
  // the app-wide Android theme setting changes.
  static void OnThemeSettingChanged();

  // Overridden from ThemeService:
  // Applied asynchronously. Observers are notified via OnThemeChanged() when
  // the Android theme setting changes (see GlobalNightModeStateController).
  void SetBrowserColorScheme(BrowserColorScheme color_scheme) override;
  BrowserColorScheme GetBrowserColorScheme() const override;
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_ANDROID_H_
