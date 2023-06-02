// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_FACTORY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class Profile;
class ProfileStatistics;

// Singleton that owns all ProfileStatistics and associates them with Profiles.
class ProfileStatisticsFactory : public ProfileKeyedServiceFactory {
 public:
  ProfileStatisticsFactory(const ProfileStatisticsFactory&) = delete;
  ProfileStatisticsFactory& operator=(const ProfileStatisticsFactory&) = delete;
  static ProfileStatistics* GetForProfile(Profile* profile);

  static ProfileStatisticsFactory* GetInstance();

 private:
  friend base::NoDestructor<ProfileStatisticsFactory>;

  ProfileStatisticsFactory();

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_FACTORY_H_
