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

class IbanManager;

// Singleton that owns all IbanManagers and associates them with Profiles.
class IbanManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the IbanManager for |profile|, creating it if it is not yet
  // created.
  static IbanManager* GetForProfile(Profile* profile);

  static IbanManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<IbanManagerFactory>;

  IbanManagerFactory();
  ~IbanManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_IBAN_MANAGER_FACTORY_H_
