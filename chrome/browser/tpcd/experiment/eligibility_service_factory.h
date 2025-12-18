// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace tpcd::experiment {

class EligibilityService;

class EligibilityServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static EligibilityService* GetForProfile(Profile* profile);
  static EligibilityServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<EligibilityServiceFactory>;
  EligibilityServiceFactory();

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_FACTORY_H_
