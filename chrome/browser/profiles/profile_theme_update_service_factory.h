// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_THEME_UPDATE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_THEME_UPDATE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ProfileThemeUpdateService;
class Profile;

// Singleton that owns all ProfileThemeUpdateServices and associates them with
// Profiles.
class ProfileThemeUpdateServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ProfileThemeUpdateService* GetForProfile(Profile* profile);
  static ProfileThemeUpdateServiceFactory* GetInstance();

  // This class is uncopyable.
  ProfileThemeUpdateServiceFactory(const ProfileThemeUpdateServiceFactory&) =
      delete;
  ProfileThemeUpdateServiceFactory& operator=(
      const ProfileThemeUpdateServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ProfileThemeUpdateServiceFactory>;

  ProfileThemeUpdateServiceFactory();
  ~ProfileThemeUpdateServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_THEME_UPDATE_SERVICE_FACTORY_H_
