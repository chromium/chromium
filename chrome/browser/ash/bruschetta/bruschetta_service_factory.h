// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace bruschetta {

class BruschettaService;

class BruschettaServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static bruschetta::BruschettaService* GetForProfile(Profile* profile);
  static BruschettaServiceFactory* GetInstance();

  BruschettaServiceFactory(const BruschettaServiceFactory&) = delete;
  BruschettaServiceFactory& operator=(const BruschettaServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<BruschettaServiceFactory>;

  BruschettaServiceFactory();
  ~BruschettaServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_FACTORY_H_
