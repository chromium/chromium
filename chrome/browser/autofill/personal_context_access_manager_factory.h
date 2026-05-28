// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace autofill {

class PersonalContextAccessManager;

class PersonalContextAccessManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static autofill::PersonalContextAccessManager* GetForProfile(
      Profile* profile);

  static PersonalContextAccessManagerFactory* GetInstance();

  PersonalContextAccessManagerFactory(
      const PersonalContextAccessManagerFactory&) = delete;
  PersonalContextAccessManagerFactory& operator=(
      const PersonalContextAccessManagerFactory&) = delete;

 private:
  friend base::NoDestructor<PersonalContextAccessManagerFactory>;

  PersonalContextAccessManagerFactory();
  ~PersonalContextAccessManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_
