// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_FACTORY_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class ThemeService;

namespace extensions {
class Extension;
}

// Singleton that owns all ThemeServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ThemeService.
class ThemeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the ThemeService that provides theming resources for
  // |profile|. Note that even if a Profile doesn't have a theme installed, it
  // still needs a ThemeService to hand back the default theme images.
  static ThemeService* GetForProfile(Profile* profile);

  // Returns the Extension that implements the theme associated with
  // |profile|. Returns NULL if the theme is no longer installed, if there is
  // no installed theme, or the theme was cleared.
  static const extensions::Extension* GetThemeForProfile(Profile* profile);

  static ThemeServiceFactory* GetInstance();

  ThemeServiceFactory(const ThemeServiceFactory&) = delete;
  ThemeServiceFactory& operator=(const ThemeServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ThemeServiceFactory>;

  ThemeServiceFactory();
  ~ThemeServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  void BrowserContextDestroyed(
      content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_FACTORY_H_
