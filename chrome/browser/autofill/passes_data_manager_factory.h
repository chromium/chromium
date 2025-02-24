// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PASSES_DATA_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_PASSES_DATA_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class KeyedService;
class Profile;

namespace autofill {

class PassesDataManager;

class PassesDataManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static PassesDataManager* GetForProfile(Profile* profile);
  static PassesDataManagerFactory* GetInstance();

 protected:
  // ProfileKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<PassesDataManagerFactory>;

  PassesDataManagerFactory();
  ~PassesDataManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_PASSES_DATA_MANAGER_FACTORY_H_
