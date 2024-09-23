// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"

#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"

// static
MediaGalleriesPreferences*
MediaGalleriesPreferencesFactory::GetForProfile(Profile* profile) {
  return static_cast<MediaGalleriesPreferences*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MediaGalleriesPreferencesFactory*
MediaGalleriesPreferencesFactory::GetInstance() {
  static base::NoDestructor<MediaGalleriesPreferencesFactory> instance;
  return instance.get();
}

MediaGalleriesPreferencesFactory::MediaGalleriesPreferencesFactory()
    : ProfileKeyedServiceFactory(
          "MediaGalleriesPreferences",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

MediaGalleriesPreferencesFactory::~MediaGalleriesPreferencesFactory() = default;

std::unique_ptr<KeyedService>
MediaGalleriesPreferencesFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<MediaGalleriesPreferences>(
      static_cast<Profile*>(profile));
}

void MediaGalleriesPreferencesFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  MediaGalleriesPreferences::RegisterProfilePrefs(prefs);
}
