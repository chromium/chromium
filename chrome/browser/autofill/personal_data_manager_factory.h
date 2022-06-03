// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_FACTORY_H_

#include "base/compiler_specific.h"
#include "chrome/browser/autofill/autofill_profile_validator_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

class Profile;

namespace autofill {

class PersonalDataManager;

// Singleton that owns all PersonalDataManagers and associates them with
// Profiles.
// Listens for the Profile's destruction notification and cleans up the
// associated PersonalDataManager.
class PersonalDataManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the PersonalDataManager for |profile|, creating it if it is not
  // yet created.
  static PersonalDataManager* GetForProfile(Profile* profile);

  // Returns the PersonalDataManager for |context|, creating it if it is not
  // yet created.
  static PersonalDataManager* GetForBrowserContext(
      content::BrowserContext* context);

  static PersonalDataManagerFactory* GetInstance();

  static KeyedService* BuildPersonalDataManager(
      autofill::AutofillProfileValidator* autofill_validator,
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<PersonalDataManagerFactory>;

  PersonalDataManagerFactory();
  ~PersonalDataManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_FACTORY_H_
