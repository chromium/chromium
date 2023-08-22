// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class GAIAInfoUpdateService;
class Profile;

// Singleton that owns all GAIAInfoUpdateServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated GAIAInfoUpdateService.
class GAIAInfoUpdateServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of GAIAInfoUpdateService associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have a
  // GAIAInfoUpdateService (for example, if |profile| is incognito).
  static GAIAInfoUpdateService* GetForProfile(Profile* profile);

  // Returns an instance of the GAIAInfoUpdateServiceFactory singleton.
  static GAIAInfoUpdateServiceFactory* GetInstance();

  GAIAInfoUpdateServiceFactory(const GAIAInfoUpdateServiceFactory&) = delete;
  GAIAInfoUpdateServiceFactory& operator=(const GAIAInfoUpdateServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<GAIAInfoUpdateServiceFactory>;

  GAIAInfoUpdateServiceFactory();
  ~GAIAInfoUpdateServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_FACTORY_H_
