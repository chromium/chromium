// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PREFERENCES_FACTORY_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PREFERENCES_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class MediaGalleriesPreferences;
class Profile;

// Singleton that owns all MediaGalleriesPreferences and associates them with
// Profiles.
class MediaGalleriesPreferencesFactory : public ProfileKeyedServiceFactory {
 public:
  // Use MediaFileSystemRegistry::GetPreferences() to get
  // MediaGalleriesPreferences.
  static MediaGalleriesPreferences* GetForProfile(Profile* profile);

  static MediaGalleriesPreferencesFactory* GetInstance();

  MediaGalleriesPreferencesFactory(const MediaGalleriesPreferencesFactory&) =
      delete;
  MediaGalleriesPreferencesFactory& operator=(
      const MediaGalleriesPreferencesFactory&) = delete;

 private:
  friend base::NoDestructor<MediaGalleriesPreferencesFactory>;

  MediaGalleriesPreferencesFactory();
  ~MediaGalleriesPreferencesFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PREFERENCES_FACTORY_H_
