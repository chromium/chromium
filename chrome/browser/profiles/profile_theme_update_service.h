// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_THEME_UPDATE_SERVICE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_THEME_UPDATE_SERVICE_H_

#include "chrome/browser/themes/theme_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class ProfileAttributesStorage;
class ThemeService;

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
      ProfileAttributesStorage* profile_attributes_storage,
      ThemeService* theme_service);
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

  Profile* const profile_;
  ProfileAttributesStorage* const profile_attributes_storage_;
  ThemeService* const theme_service_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_THEME_UPDATE_SERVICE_H_
