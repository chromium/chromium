// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_IBAN_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_IBAN_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
class NoDestructor;
}

class Profile;

namespace autofill {

class IBANManager;

// Singleton that owns all IBANManagers and associates them with Profiles.
class IBANManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the IBANManager for |profile|, creating it if it is not yet
  // created.
  static IBANManager* GetForProfile(Profile* profile);

  static IBANManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<IBANManagerFactory>;

  IBANManagerFactory();
  ~IBANManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_IBAN_MANAGER_FACTORY_H_
