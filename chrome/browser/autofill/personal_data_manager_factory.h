// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class KeyedService;
class Profile;

namespace autofill {

class PersonalDataManager;

// Singleton that owns all PersonalDataManagers and associates them with
// Profiles.
// Listens for the Profile's destruction notification and cleans up the
// associated PersonalDataManager.
class PersonalDataManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the PersonalDataManager for |context|, creating it if it is not
  // yet created.
  static PersonalDataManager* GetForBrowserContext(
      content::BrowserContext* context);

  static PersonalDataManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<PersonalDataManagerFactory>;

  PersonalDataManagerFactory();
  ~PersonalDataManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_FACTORY_H_
