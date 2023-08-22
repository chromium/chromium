// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class Profile;

namespace badging {

class BadgeManager;

// Singleton that provides access to Profile specific BadgeManagers.
class BadgeManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the BadgeManager for the current profile. |nullptr| for guest and
  // incognito profiles.
  static BadgeManager* GetForProfile(Profile* profile);

  // Returns the BadgeManagerFactory singleton.
  static BadgeManagerFactory* GetInstance();

  BadgeManagerFactory(const BadgeManagerFactory&) = delete;
  BadgeManagerFactory& operator=(const BadgeManagerFactory&) = delete;

 private:
  friend base::NoDestructor<BadgeManagerFactory>;

  BadgeManagerFactory();
  ~BadgeManagerFactory() override;

  // BrowserContextKeyedServiceFactory
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_FACTORY_H_
