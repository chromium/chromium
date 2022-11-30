// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_THEME_UPDATE_SERVICE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_THEME_UPDATE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class ProfileAttributesStorage;

// A KeyedService that listens to browser theme updates and updates profile
// theme colors cached in ProfileAttributesStorage.
//
// These colors are used to display a list of profiles. They are cached to be
// accessible without having to load the profiles from disk.
class ProfileThemeUpdateService : public KeyedService,
                                  public ThemeServiceObserver {
 public:
  ProfileThemeUpdateService(
      Profile* profile,
      ProfileAttributesStorage* profile_attributes_storage);
  ~ProfileThemeUpdateService() override;

  // This class in uncopyable.
  ProfileThemeUpdateService(const ProfileThemeUpdateService&) = delete;
  ProfileThemeUpdateService& operator=(const ProfileThemeUpdateService&) =
      delete;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

 private:
  // Updates profile theme colors in |profile_attributes_storage_| for
  // |profile_|.
  void UpdateProfileThemeColors();

  const raw_ptr<Profile> profile_;
  const raw_ptr<ProfileAttributesStorage> profile_attributes_storage_;
  base::ScopedObservation<ThemeService, ThemeServiceObserver> observation_{
      this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_THEME_UPDATE_SERVICE_H_
